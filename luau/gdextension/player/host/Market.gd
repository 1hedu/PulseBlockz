# COPY -- do not edit. The original is luau/gdextension/host/Market.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# DexScreener, read from a place. No verb takes a URL: a place names a search, the client owns
# the endpoint, and the client as a whole reaches three sites and no fourth -- the explorer
# (Scan.gd), DexScreener here, PulseX (Pulsex.gd). `MarketRequest`/`MarketResult` are written by
# client scripts only, and a client's attribute write does not replicate upward, so each player
# searches from their own machine.
extends Node

@export var market_api: String = "https://api.dexscreener.com/latest/dex"
@export var market_site: String = "dexscreener.com"
## A search answers with every chain DexScreener indexes; rows are matched on `chainId`.
@export var market_chain: String = "pulsechain"

## Rows one search answers with: a screenful. Every extra one is bytes crossing in an attribute.
const KEEP := 25
## Seconds an answer is reused for: typing a symbol asks a question per keystroke, and a price
## a minute old still quotes the market closely enough to show.
const CACHE_SECONDS := 60.0

var _http: HTTPRequest
var _busy := false                 # one GET at a time: two callers would share one answer
var _chain_id := 0
var _handled: Array[int] = []
var _cache := {}                   # query -> {"at": seconds, "rows": [...]}

func _ready() -> void:
	# Its own node and its own HTTPRequest, beside Scan.gd: _http_get serialises on _busy, so a
	# shared channel would park a keyless read behind a signed purchase and put both on one wire.
	_http = HTTPRequest.new()
	_http.use_threads = true   # handshake and download off the frame (see Rpc.gd)
	add_child(_http)
	_watch()

## Id of ReplicatedStorage.Chain, or 0 for absent: 0 is the root, so no child carries it.
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

func _watch() -> void:
	while is_inside_tree():
		await get_tree().create_timer(0.2).timeout
		var world = get_parent().get_node_or_null("World")
		if world == null:
			continue
		if _chain_id == 0:
			_chain_id = _find_chain(world)
			if _chain_id == 0:
				continue
			print("[market] listening on ReplicatedStorage.Chain.MarketRequest")
		var raw := String((world.get_attributes(_chain_id) as Dictionary).get("AskMarket", ""))
		if raw == "":
			continue
		# AskMarket holds a queue, oldest first: a second question written while the first is
		# still outstanding must not erase it. A lone object is accepted as a queue of one.
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
			var answer := await _answer(req)
			answer["n"] = seq
			world.run_client_chunk("market_reply", """
local rs = game:GetService("ReplicatedStorage")
local c = rs:FindFirstChild("Chain")
if c then c:SetAttribute("MarketResult", %s) end
""" % preload("res://host/Luau.gd").quote(JSON.stringify(answer)))

func _answer(req: Dictionary) -> Dictionary:
	if not preload("res://host/Uses.gd").permits("market"):
		return {"ok": false, "message": preload("res://host/Uses.gd").refusal("market")}
	var q := String(req.get("search", "")).strip_edges()
	if q.length() < 2:
		return {"ok": false, "message": "Type at least two characters."}
	# A place wrote this text and it ends up in a URL: keep what names a pair, drop the rest.
	var clean := ""
	for c in q:
		if c.is_valid_identifier() or c.is_valid_int() or c in [" ", ".", "-", "_", "/"]:
			clean += c
	if clean.strip_edges() == "":
		return {"ok": false, "message": "Nothing searchable in that."}
	var key := clean.to_lower()
	var hit = _cache.get(key)
	if hit != null and Time.get_ticks_msec() / 1000.0 - float(hit["at"]) < CACHE_SECONDS:
		return {"ok": true, "rows": hit["rows"], "query": clean, "cached": true,
			"site": market_site}
	var body := await _http_get("%s/search?q=%s" % [market_api, clean.uri_encode()])
	if body.is_empty():
		return {"ok": false, "message": "%s didn't answer." % market_site}
	var doc = JSON.parse_string(body.get_string_from_utf8())
	if typeof(doc) != TYPE_DICTIONARY:
		return {"ok": false, "message": "%s sent something unreadable." % market_site}
	var rows := _rows(doc.get("pairs", []))
	_cache[key] = {"at": Time.get_ticks_msec() / 1000.0, "rows": rows}
	return {"ok": true, "rows": rows, "query": clean, "cached": false, "site": market_site}

## Pairs on this chain, one per pairAddress, deepest liquidity first.
func _rows(pairs: Array) -> Array:
	var out := []
	var seen := {}
	for p in pairs:
		if typeof(p) != TYPE_DICTIONARY:
			continue
		if String(p.get("chainId", "")).to_lower() != market_chain:
			continue
		var addr := String(p.get("pairAddress", "")).to_lower()
		if addr == "" or seen.has(addr):
			continue
		seen[addr] = true
		var base: Dictionary = p.get("baseToken", {}) if typeof(p.get("baseToken")) == TYPE_DICTIONARY else {}
		var quote: Dictionary = p.get("quoteToken", {}) if typeof(p.get("quoteToken")) == TYPE_DICTIONARY else {}
		var liq: Dictionary = p.get("liquidity", {}) if typeof(p.get("liquidity")) == TYPE_DICTIONARY else {}
		# DexScreener's own field shape, so a place feeds these to the same Dex.row() its board
		# is built with; trimmed to what that draws, since the row crosses as JSON in an
		# attribute. Every key the board reads must be present -- a nil cell kills the panel.
		var vol: Dictionary = p.get("volume", {}) if typeof(p.get("volume")) == TYPE_DICTIONARY else {}
		var chg: Dictionary = p.get("priceChange", {}) if typeof(p.get("priceChange")) == TYPE_DICTIONARY else {}
		var txns: Dictionary = p.get("txns", {}) if typeof(p.get("txns")) == TYPE_DICTIONARY else {}
		var day: Dictionary = txns.get("h24", {}) if typeof(txns.get("h24")) == TYPE_DICTIONARY else {}
		out.append({
			"address": addr,
			"pairAddress": addr,
			"chainId": String(p.get("chainId", "")),
			"dexId": String(p.get("dexId", "?")),
			"url": String(p.get("url", "")),
			"priceUsd": String(p.get("priceUsd", "")),
			"pairCreatedAt": float(p.get("pairCreatedAt", 0.0)),
			"fdv": float(p.get("fdv", 0.0)),
			"baseToken": {"symbol": String(base.get("symbol", "?"))},
			"quoteToken": {"symbol": String(quote.get("symbol", "?"))},
			"liquidity": {"usd": float(liq.get("usd", 0.0))},
			"volume": {"h24": float(vol.get("h24", 0.0))},
			"priceChange": {"m5": float(chg.get("m5", 0.0)), "h1": float(chg.get("h1", 0.0)),
				"h6": float(chg.get("h6", 0.0)), "h24": float(chg.get("h24", 0.0))},
			"txns": {"h24": {"buys": int(day.get("buys", 0)), "sells": int(day.get("sells", 0))}},
			"liquidity_raw": float(liq.get("usd", 0.0)),
		})
	out.sort_custom(func(a, b): return a.liquidity_raw > b.liquidity_raw)
	if out.size() > KEEP:
		out.resize(KEEP)
	return out

func _http_get(url: String) -> PackedByteArray:
	var queued := 0
	while _busy and queued < 600:
		queued += 1
		await get_tree().create_timer(0.05).timeout
	_busy = true
	for _i in 40:
		if _http.get_http_client_status() == HTTPClient.STATUS_DISCONNECTED:
			break
		await get_tree().create_timer(0.05).timeout
	if _http.request(url) != OK:
		push_warning("Market: could not start a request for %s" % url)
		_busy = false
		return PackedByteArray()
	var res: Array = await _http.request_completed
	_busy = false
	return res[3] if res[1] == 200 else PackedByteArray()
