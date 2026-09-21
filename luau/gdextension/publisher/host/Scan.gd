# COPY -- do not edit. The original is luau/gdextension/host/Scan.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# The block explorer, read from inside the town: no key, no wallet, its own request lanes and
# its own channel (ReplicatedStorage.Chain's AskScan / ScanResult). Blockscout API v2
# throughout, so a view/id pair is a URL on the site: "block"/25337983 is /block/25337983.
extends Node

## The indexer's API, not the RPC node.
@export var explorer_api: String = "https://api.scan.v4.testnet.pulsechain.com/api"
@export var explorer_site: String = "scan.v4.testnet.pulsechain.com"

## Concurrent HTTPRequests. An HTTPRequest refuses a second request while the first is in
## flight, so a page that asks several endpoints at once needs a node per endpoint: the home
## page asks three, no other view asks more than one, and one lane is left over.
const LANES := 4

var _lanes: Array[HTTPRequest] = []
var _taken := {}
var _chain_id := 0
var _handled: Array[int] = []
## Settled pages only: a mined block or transaction cannot change.
var _scan_cache := {}

func _ready() -> void:
	for _i in LANES:
		var lane := HTTPRequest.new()
		lane.use_threads = true   # TLS handshake and download off the frame (see Rpc.gd)
		# HTTPRequest has no timeout by default: a silent explorer would hold its lane for good
		# and no later page would be served. Shorter than the 30 s a GET waits for a lane.
		lane.timeout = 15.0
		add_child(lane)
		_lanes.append(lane)
	_watch()

## Serves AskScan. Separate from the wallet's channel, so neither waits on the other.
func _watch() -> void:
	while is_inside_tree():
		await get_tree().create_timer(0.2).timeout
		var world = get_parent().get_node_or_null("World")
		if world == null:
			continue
		# Every role runs this, joined clients included: AskScan is a local attribute, so
		# the machine looking at the explorer is the one that fetches.
		if _chain_id == 0:
			_chain_id = _find_chain(world)
			if _chain_id == 0:
				continue
			print("[scan] listening on ReplicatedStorage.Chain.AskScan")
		var raw := String((world.get_attributes(_chain_id) as Dictionary).get("AskScan", ""))
		if raw == "":
			continue
		# An array, oldest first: one attribute polled at 5 Hz in front of a fetch that takes
		# seconds loses requests written back to back. A bare dictionary is still accepted.
		var parsed = JSON.parse_string(raw)
		var queue: Array = []
		if typeof(parsed) == TYPE_ARRAY:
			queue = parsed
		elif typeof(parsed) == TYPE_DICTIONARY:
			queue = [parsed]
		else:
			continue
		for entry in queue:
			if typeof(entry) != TYPE_DICTIONARY:
				continue
			var req: Dictionary = entry
			var seq := int(req.get("n", -1))
			if seq < 0 or _handled.has(seq):
				continue
			_handled.append(seq)
			if _handled.size() > 256:
				_handled.remove_at(0)
			# view and id come from a place's own script and go straight into a URL: this is
			# the last point that can refuse.
			var view := String(req.get("view", "home"))
			var id := String(req.get("id", ""))
			var page: Dictionary
			if not preload("res://host/Uses.gd").permits("scan"):
				page = {"ok": false, "message": preload("res://host/Uses.gd").refusal("scan")}
			elif not _sane(view, id):
				page = {"ok": false, "message": "That is not a page on the scan."}
			else:
				page = await scan_view(view, id)
			page["n"] = seq
			world.run_client_chunk("scan_reply", """
local rs = game:GetService("ReplicatedStorage")
local c = rs:FindFirstChild("Chain")
if c then c:SetAttribute("ScanResult", %s) end
""" % preload("res://host/Luau.gd").quote(JSON.stringify(page)))

## A known view, and an id that can be pasted into a URL. Search terms are looser because
## _scan_search uri_encodes them; every other id is a block number, hash or address.
func _sane(view: String, id: String) -> bool:
	if not view in ["home", "blocks", "block", "txs", "tx", "address", "tokens", "token", "search"]:
		return false
	if view == "search":
		return id.length() <= 128
	if id.length() > 66:
		return false
	for c in id:
		if not (c.is_valid_identifier() or c.is_valid_int()):
			return false
	return true

## By name: the instance table has no lookup by class. The same walk, over the same two
## names, is copied in Wallet.gd, Market.gd and Toast.gd.
func _find_chain(world) -> int:
	var id := 0
	for want in ["ReplicatedStorage", "Chain"]:
		var found := 0
		for cid in world.get_child_ids(id):
			if String((world.get_instance(cid) as Dictionary).get("name", "")) == want:
				found = cid
				break
		if found == 0:
			return 0
		id = found
	return id


## One page, in the one shape the panel draws: stat cards, labelled rows, and tables whose
## rows carry a view/id to follow. Every view returns that shape, so the panel needs no
## knowledge of which page it has.
func scan_view(view: String, id: String) -> Dictionary:
	var key := view + "/" + id
	if _scan_cache.has(key):
		return _scan_cache[key]

	var page := {"ok": true, "view": view, "id": id, "host": explorer_site,
			"url": "", "title": "", "subtitle": "", "stats": [], "rows": [], "tables": []}
	match view:
		"home": page = await _scan_home(page)
		"blocks": page = await _scan_blocks(page)
		"block": page = await _scan_block(page, id)
		"txs": page = await _scan_txs(page)
		"tx": page = await _scan_tx(page, id)
		"address": page = await _scan_address(page, id)
		"tokens": page = await _scan_tokens(page)
		"token": page = await _scan_token(page, id)
		"search": page = await _scan_search(page, id)
		_: return {"ok": false, "message": "No such page on the scan."}

	if page.get("ok", false) and view in ["block", "tx"]:
		_scan_cache[key] = page
	return page

func _scan_address(page: Dictionary, id: String) -> Dictionary:
	var a := await _scan_get("/addresses/" + id)
	if a.is_empty():
		return {"ok": false, "message": "The explorer has nothing at that address."}
	page.url = "/address/" + id
	page.title = "Contract" if a.get("is_contract", false) else "Address"
	var named := _scan_text(a.get("name"), "")
	page.subtitle = named if named != "" else id
	page.rows = [
		{"label": "Balance", "value": _scan_coin(_scan_num(a.get("coin_balance")), 6)},
		{"label": "Transactions", "value": _scan_text(a.get("transactions_count"), "-")},
		{"label": "Contract", "value": "yes" if a.get("is_contract", false) else "no"},
		{"label": "Verified", "value": "yes" if a.get("is_verified", false) else "no"},
		{"label": "Address", "value": id},
	]
	var d := await _scan_get("/addresses/" + id + "/transactions?filter=to%7Cfrom")
	var rows := []
	for t in d.get("items", []):
		var out: bool = _scan_text(_scan_dict(t.get("from")).get("hash"), "").to_lower() == id.to_lower()
		rows.append(_scan_row([_short(_scan_text(t.get("hash"), "")), "out" if out else "in",
			_scan_who(t.get("to", {}) if out else t.get("from", {})),
			_scan_coin(_scan_num(t.get("value")), 2),
			_scan_age(_scan_text(t.get("timestamp"), ""))], "tx", _scan_text(t.get("hash"), "")))
	page.tables = [{"title": "Transactions", "columns": ["Hash", "", "With", "Value", "Age"], "rows": rows}]
	return page

func _scan_block(page: Dictionary, id: String) -> Dictionary:
	var b := await _scan_get("/blocks/" + id)
	if b.is_empty():
		return {"ok": false, "message": "No block %s on this chain." % id}
	page.url = "/block/" + id
	page.title = "Block #" + _scan_text(b.get("height"), id)
	var age := _scan_age(_scan_text(b.get("timestamp"), ""))
	page.subtitle = "" if age == "-" else age + " ago"
	page.rows = [
		{"label": "Timestamp", "value": _scan_text(b.get("timestamp"), "-")},
		{"label": "Transactions", "value": "%d" % int(b.get("tx_count", 0))},
		{"label": "Validated by", "value": _scan_who(b.get("miner", {})),
		 "view": "address", "id": _scan_text(_scan_dict(b.get("miner")).get("hash"), "")},
		{"label": "Size", "value": "%d bytes" % int(b.get("size", 0))},
		{"label": "Gas used", "value": "%s of %s" % [_scan_text(b.get("gas_used"), "-"), _scan_text(b.get("gas_limit"), "-")]},
		{"label": "Base fee", "value": _scan_text(b.get("base_fee_per_gas"), "-")},
		{"label": "Burnt fees", "value": _scan_coin(_scan_num(b.get("burnt_fees")), 4)},
		{"label": "Hash", "value": _scan_text(b.get("hash"), "-")},
		{"label": "Parent", "value": _short(_scan_text(b.get("parent_hash"), "")),
		 "view": "block", "id": "%d" % (int(b.get("height", 0)) - 1)},
	]
	var d := await _scan_get("/blocks/" + id + "/transactions")
	var rows := []
	for t in d.get("items", []):
		rows.append(_scan_row([_short(_scan_text(t.get("hash"), "")), _scan_text(t.get("method"), "-"),
			_scan_who(t.get("from", {})), _scan_who(t.get("to", {})),
			_scan_coin(_scan_text(t.get("value"), "0"), 2)], "tx", _scan_text(t.get("hash"), "")))
	page.tables = [{"title": "Transactions in this block",
		"columns": ["Hash", "Method", "From", "To", "Value"], "rows": rows}]
	return page

func _scan_blocks(page: Dictionary) -> Dictionary:
	var d := await _scan_get("/blocks?type=block")
	if d.is_empty():
		return {"ok": false, "message": "The explorer isn't answering."}
	page.url = "/blocks"
	page.title = "Blocks"
	var rows := []
	for b in d.get("items", []):
		rows.append(_scan_row([_scan_text(b.get("height"), ""), "%d" % int(b.get("tx_count", 0)),
			_scan_who(b.get("miner")), _scan_text(b.get("gas_used"), "-"),
			_scan_age(_scan_text(b.get("timestamp"), ""))], "block", _scan_text(b.get("height"), "")))
	page.tables = [{"title": "", "columns": ["Block", "Txns", "Validator", "Gas used", "Age"], "rows": rows}]
	return page

## The three endpoints this page needs fail independently, and each failure costs the
## explorer's own timeout: asked one after another the page costs their sum and draws nothing,
## asked together it costs the slowest and draws whichever answered.
func _scan_home(page: Dictionary) -> Dictionary:
	var got := await _scan_many(["/stats", "/main-page/blocks", "/main-page/transactions"])
	var st: Dictionary = got[0]
	var blocks: Dictionary = got[1]
	var txs: Dictionary = got[2]
	if st.is_empty() and blocks.is_empty() and txs.is_empty():
		return {"ok": false, "message": "The explorer isn't answering."}
	page.url = "/"
	page.title = "PulseChain Testnet v4"
	page.subtitle = "Blocks, transactions and addresses, as the explorer has them"
	# No /stats, no stat cards: five cards reading "-" say less than their absence.
	if not st.is_empty():
		var gas := _scan_dict(st.get("gas_prices"))
		page.stats = [
			{"label": "TOTAL BLOCKS", "value": _scan_text(st.get("total_blocks"), "-")},
			{"label": "TRANSACTIONS", "value": _scan_text(st.get("total_transactions"), "-")},
			{"label": "ADDRESSES", "value": _scan_text(st.get("total_addresses"), "-")},
			{"label": "BLOCK TIME", "value": "%.1fs" % (float(st.get("average_block_time", 0)) / 1000.0)},
			{"label": "GAS (AVG)", "value": "%.0f" % float(gas.get("average", 0))},
		]
	var brows := []
	for b in blocks.get("items", []):
		brows.append(_scan_row([_scan_text(b.get("height"), ""), "%d txns" % int(b.get("tx_count", 0)),
			_scan_who(b.get("miner", {})), _scan_age(_scan_text(b.get("timestamp"), ""))],
			"block", _scan_text(b.get("height"), "")))
	var trows := []
	for t in txs.get("items", []):
		trows.append(_scan_row([_short(_scan_text(t.get("hash"), "")), _scan_who(t.get("from", {})),
			_scan_who(t.get("to", {})), _scan_coin(_scan_text(t.get("value"), "0"), 2)],
			"tx", _scan_text(t.get("hash"), "")))
	# Last argument: no answer, which is not the same as an answer with no rows.
	page.tables = [
		_scan_table("Latest blocks", ["Block", "Txns", "Validator", "Age"], brows, blocks.is_empty()),
		_scan_table("Latest transactions", ["Hash", "From", "To", "Value"], trows, txs.is_empty()),
	]
	return page

## One endpoint for addresses, blocks, transactions and tokens by name or ticker. Each
## result carries a `type`, and that decides which page its row opens.
func _scan_search(page: Dictionary, q: String) -> Dictionary:
	var term := q.strip_edges()
	if term == "":
		return {"ok": false, "message": "Nothing to look for."}
	var d := await _scan_get("/search?q=" + term.uri_encode())
	page.url = "/search?q=" + term
	page.title = "Search"
	page.subtitle = "\"%s\"" % term
	var rows := []
	for r in d.get("items", []):
		var kind := _scan_text(r.get("type"), "")
		var name := _scan_text(r.get("name"), "")
		var view := ""
		var id := ""
		match kind:
			"address", "contract":
				view = "address"
				id = _scan_text(r.get("address"), "")
			"token":
				# The token page, not the address page: an address page carries neither the
				# holders nor the transfers.
				view = "token"
				id = _scan_text(r.get("address"), "")
				if name != "":
					name = "%s  (%s)" % [name, _scan_text(r.get("symbol"), "?")]
			"block":
				view = "block"
				id = _scan_text(r.get("block_number"), "")
			"transaction", "transaction_hash":
				view = "tx"
				id = _scan_text(r.get("tx_hash"), "")
			_:
				continue
		if id == "":
			continue
		rows.append(_scan_row([kind, name if name != "" else _short(id), _short(id)], view, id))
	if rows.is_empty():
		page.subtitle = "Nothing on this chain matches \"%s\"." % term
	page.tables = [{"title": "", "columns": ["Kind", "Name", "Which"], "rows": rows}]
	return page

## The token, its holders and its transfers: three endpoints, one page, no tabs.
func _scan_token(page: Dictionary, id: String) -> Dictionary:
	var t := await _scan_get("/tokens/" + id)
	if t.is_empty():
		return {"ok": false, "message": "The explorer has no token at that address."}
	var decimals := int(_scan_num(t.get("decimals")).to_int()) if _scan_num(t.get("decimals")) != "" else 18
	var symbol := _scan_text(t.get("symbol"), "?")
	page.url = "/token/" + id
	page.title = "Token"
	var named := _scan_text(t.get("name"), "")
	page.subtitle = ("%s (%s)" % [named, symbol]) if named != "" else symbol
	page.rows = [
		{"label": "Symbol", "value": symbol},
		{"label": "Type", "value": _scan_text(t.get("type"), "-")},
		{"label": "Decimals", "value": str(decimals)},
		{"label": "Total supply", "value": _format_units(_scan_num(t.get("total_supply")), decimals, 2)},
		{"label": "Holders", "value": _scan_text(t.get("holders"), "-")},
		{"label": "Contract", "value": id},
	]

	# Already sorted by balance.
	var holders := await _scan_get("/tokens/" + id + "/holders")
	var hrows := []
	for h in holders.get("items", []):
		var who := _scan_dict(h.get("address"))
		hrows.append(_scan_row([_scan_who(h.get("address", {})),
			_format_units(_scan_num(h.get("value")), decimals, 2)],
			"address", _scan_text(who.get("hash"), "")))

	# `tx_hash`, not `transaction_hash`: the transfer list carries both and the long spelling
	# is always null, which reads as a row with no link.
	var moves := await _scan_get("/tokens/" + id + "/transfers")
	var mrows := []
	for m in moves.get("items", []):
		var total := _scan_dict(m.get("total"))
		# A transfer's own decimals need not be the token's.
		var scale := decimals
		if _scan_num(total.get("decimals")) != "":
			scale = _scan_num(total.get("decimals")).to_int()
		mrows.append(_scan_row([_scan_who(m.get("from", {})), _scan_who(m.get("to", {})),
			_format_units(_scan_num(total.get("value")), scale, 2),
			_scan_age(_scan_text(m.get("timestamp"), ""))],
			"tx", _scan_text(m.get("tx_hash"), "")))

	# Last argument: no answer, which is not the same as an answer with no rows.
	page.tables = [
		_scan_table("Holders", ["Address", symbol], hrows, holders.is_empty()),
		_scan_table("Transfers", ["From", "To", symbol, "Age"], mrows, moves.is_empty()),
	]
	return page

func _scan_table(title: String, columns: Array, rows: Array, unanswered: bool) -> Dictionary:
	var t := {"title": title, "columns": columns, "rows": rows}
	if rows.is_empty() and unanswered:
		t["note"] = "The explorer didn't answer for this one."
	return t

func _scan_tokens(page: Dictionary) -> Dictionary:
	var d := await _scan_get("/tokens?type=ERC-20")
	if d.is_empty():
		return {"ok": false, "message": "The explorer isn't answering."}
	page.url = "/tokens"
	page.title = "Tokens"
	var rows := []
	for t in d.get("items", []):
		rows.append(_scan_row([_scan_text(t.get("symbol"), "-"), _scan_text(t.get("name"), "-"),
			_scan_text(t.get("holders"), "-"), _short(_scan_text(t.get("address"), ""))],
			"token", _scan_text(t.get("address"), "")))
	page.tables = [{"title": "", "columns": ["Symbol", "Name", "Holders", "Contract"], "rows": rows}]
	return page

func _scan_tx(page: Dictionary, id: String) -> Dictionary:
	var t := await _scan_get("/transactions/" + id)
	if t.is_empty():
		return {"ok": false, "message": "No transaction by that name."}
	page.url = "/tx/" + id
	page.title = "Transaction"
	page.subtitle = _short(id)
	var block := _scan_text(t.get("block"), "")
	page.rows = [
		{"label": "Status", "value": _scan_text(t.get("status"), "-")},
		{"label": "Method", "value": String(t.get("method", "") if t.get("method") != null else "-")},
		{"label": "Block", "value": block, "view": "block", "id": block},
		{"label": "Timestamp", "value": _scan_text(t.get("timestamp"), "-")},
		{"label": "From", "value": _scan_who(t.get("from", {})),
		 "view": "address", "id": _scan_text(_scan_dict(t.get("from")).get("hash"), "")},
		{"label": "To", "value": _scan_who(t.get("to", {})),
		 "view": "address", "id": _scan_text(_scan_dict(t.get("to")).get("hash"), "")},
		{"label": "Value", "value": _scan_coin(_scan_num(t.get("value")), 6)},
		{"label": "Fee", "value": _scan_coin(_scan_num(_scan_dict(t.get("fee")).get("value")), 6)},
		{"label": "Gas used", "value": _scan_text(t.get("gas_used"), "-")},
		{"label": "Nonce", "value": "%d" % int(t.get("nonce", 0))},
		{"label": "Hash", "value": id},
	]
	return page

func _scan_txs(page: Dictionary) -> Dictionary:
	var d := await _scan_get("/transactions?filter=validated")
	if d.is_empty():
		return {"ok": false, "message": "The explorer isn't answering."}
	page.url = "/txs"
	page.title = "Transactions"
	var rows := []
	for t in d.get("items", []):
		rows.append(_scan_row([_short(_scan_text(t.get("hash"), "")), _scan_who(t.get("from", {})),
			_scan_who(t.get("to", {})), _scan_coin(_scan_text(t.get("value"), "0"), 2),
			_scan_age(_scan_text(t.get("timestamp"), ""))], "tx", _scan_text(t.get("hash"), "")))
	page.tables = [{"title": "", "columns": ["Hash", "From", "To", "Value", "Age"], "rows": rows}]
	return page

## Cut to 19 characters: Godot's datetime parser rejects Blockscout's fractional seconds.
func _scan_age(iso: String) -> String:
	if iso == "":
		return "-"
	var t := Time.get_unix_time_from_datetime_string(iso.substr(0, 19))
	return _age_since(t * 1000.0) if t > 0 else "-"

func _scan_coin(wei: String, places: int) -> String:
	return _format_units(wei if wei != "" else "0", 18, places) + " PLS"

## GET one v2 path, parsed. An empty dictionary means the explorer said nothing.
func _scan_get(path: String) -> Dictionary:
	var got: Array = await _scan_many([path])
	return got[0]

## GET several v2 paths at once, parsed, answered in the order asked; {} for one that did not
## answer -- an endpoint returning 500 is an outcome, not an error.
##
## Every request starts before any is awaited, so the set costs about as long as its slowest
## lane rather than the sum of them. Answers arrive through one-shot callbacks: a signal is not
## queued, so awaiting a lane that already finished hangs for good.
func _scan_many(paths: Array) -> Array:
	var out := []
	out.resize(paths.size())
	for i in out.size():
		out[i] = {}
	var waiting := 0
	var done := [0]
	for i in paths.size():
		var lane := await _free_lane()
		if lane == null:
			break
		var at := i
		waiting += 1
		# Released per request, not per set: one stalled request must not hold the pool.
		var answered := func(_result, code, _headers, bytes):
			if code == 200:
				out[at] = _scan_parse(bytes as PackedByteArray)
			done[0] += 1
			_release(lane)
		lane.request_completed.connect(answered, CONNECT_ONE_SHOT)
		if lane.request(explorer_api + "/v2" + String(paths[i])) != OK:
			push_warning("Scan: could not start a request for %s" % paths[i])
			# A one-shot that never fires stays connected, and the next request to take this lane
			# would be answered twice and release it twice over.
			lane.request_completed.disconnect(answered)
			done[0] += 1
			_release(lane)
	# 20 s, past the lanes' own 15 s timeout: reached only if a lane never reports at all.
	var spun := 0
	while done[0] < waiting and spun < 400:
		spun += 1
		await get_tree().create_timer(0.05).timeout
	return out

## A v2 body. A bare list is wrapped, so every caller reads `items`.
func _scan_parse(body: PackedByteArray) -> Dictionary:
	if body.is_empty():
		return {}
	var parsed = JSON.parse_string(body.get_string_from_utf8())
	if typeof(parsed) == TYPE_DICTIONARY:
		return parsed
	if typeof(parsed) == TYPE_ARRAY:
		return {"items": parsed}
	return {}

func _scan_row(cells: Array, view: String, id: String) -> Dictionary:
	return {"cells": cells, "view": view, "id": id}

## Blockscout sends null where a field does not apply -- no `to` on a contract creation, no
## `fee` on an unfinished block. Dictionary.get covers a missing key but not a present null,
## and .get through a null is a hard error.
func _scan_dict(v) -> Dictionary:
	return v if typeof(v) == TYPE_DICTIONARY else {}

func _scan_text(v, fallback: String) -> String:
	# str() of a null is the literal text "<null>", which would be drawn as a fee or a value.
	if v == null:
		return fallback
	# A field can be a record or a list rather than a scalar -- `miner`, `rewards` -- and
	# String() has no constructor for either.
	var ty := typeof(v)
	if ty == TYPE_DICTIONARY or ty == TYPE_ARRAY or ty == TYPE_OBJECT:
		return fallback
	# str(), not String(): there is no String(float) overload, and every number JSON parses
	# is a float.
	var t := str(v)
	return fallback if t == "" else t

func _scan_num(v) -> String:
	if v == null or typeof(v) in [TYPE_DICTIONARY, TYPE_ARRAY, TYPE_OBJECT]:
		return "0"
	return str(v)

func _scan_who(d) -> String:
	var e := _scan_dict(d)
	if e.is_empty():
		return "-"
	return _scan_text(e.get("name"), _short(_scan_text(e.get("hash"), "")))


# ---- http ---------------------------------------------------------------------------------

## An unused lane, marked taken; null if none comes free within 30 s. A flag rather than the
## node's status: two callers can both find a lane idle in one frame, and one request_completed
## wakes everyone awaiting the same node. The loop counts 0.05 s timers, not frames: the cap is
## wall clock and must not stretch or shrink with frame rate.
func _free_lane() -> HTTPRequest:
	for _try in 600:
		for lane in _lanes:
			if not _taken.get(lane, false):
				_taken[lane] = true
				return lane
		await get_tree().create_timer(0.05).timeout
	print("[scan] no lane came free in 30 s")
	return null

func _release(lane: HTTPRequest) -> void:
	_taken[lane] = false


func _short(a: String) -> String:
	return a if a.length() < 12 else a.substr(0, 6) + "..." + a.substr(a.length() - 4)


## "3d", "5h", "12m" from a millisecond timestamp.
func _age_since(ms: float) -> String:
	if ms <= 0:
		return "-"
	var seconds := int(Time.get_unix_time_from_system() - ms / 1000.0)
	if seconds < 0:
		return "-"
	if seconds < 3600:
		return "%dm" % maxi(1, seconds / 60)
	if seconds < 86400:
		return "%dh" % (seconds / 3600)
	if seconds < 86400 * 365:
		return "%dd" % (seconds / 86400)
	return "%dy" % (seconds / (86400 * 365))


## "1234500" with 6 decimals -> "1.23"
func _format_units(dec: String, decimals: int, places: int) -> String:
	if dec == "":
		return "?"
	if decimals <= 0:
		return dec
	var s := dec
	while s.length() <= decimals:
		s = "0" + s
	var whole := s.substr(0, s.length() - decimals)
	var frac := s.substr(s.length() - decimals)
	if places <= 0:
		return whole
	while frac.length() < places:
		frac += "0"
	frac = frac.substr(0, places)
	while frac.length() > 1 and frac.ends_with("0"):
		frac = frac.substr(0, frac.length() - 1)
	return whole if frac == "0" else whole + "." + frac

## A threaded request holds the process open until it answers or times out, so quitting a place
## with a lane still waiting on a node that is not answering waits with it. They are cancelled on
## the way out: an answer nobody is left to read is worth nothing.
func _exit_tree() -> void:
	for lane in _lanes:
		if is_instance_valid(lane):
			lane.cancel_request()
