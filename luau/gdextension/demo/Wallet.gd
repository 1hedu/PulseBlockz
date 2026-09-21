# Reads PulseChain for the game. HttpService's request methods are disabled in the sandbox, so
# a creator script cannot fetch this itself: the host does, and publishes one JSON blob into
# ReplicatedStorage.Chain, which src/server/Bank.server.luau reads. Reads only, no signing key.
extends Node

signal published(data: Dictionary)

## Whose account the teller reports on. The default already holds testnet tokens and items.
@export var wallet_address: String = "0x9E53d6dE1b989ed13aEE38645F3F3462Ca0Cff09"
@export var rpc_url: String = "https://rpc.v4.testnet.pulsechain.com"
@export var refresh_seconds: float = 60.0
@export var scan_ids: int = 12          # how far back through the registry to look for items
@export var auto_start: bool = true

# PulseChain testnet v4 (chainId 943) — scripts/testnet.js writes addresses.943.json.
const ADDRESSES := {
	"network": "PulseChain testnet v4",
	"UGC1155": "0xbD540C22F9FA1f070D0b85d8325baE809c3db40B",
	"Marketplace": "0x9f1A5C5d8328C863EDcDef8416745391A3Ef2968",
	"AssetStore": "0x0f9D08e13BE2345856026615d05F7251F07efAfA",
	"tokens": [
		"0x0C8c6E577E41D9308b5Ecc88848ab454E390CC4b",   # mUSD (6dp)
		"0x1E2a8eC2cB4Fd3C761E7b2037718e71483a0da95",   # mWPLS (18dp)
	],
}

var world: PulseBlockzWorld
var _http: HTTPRequest
var _busy := false
var _meta := {}          # token address -> {symbol, decimals}

func _ready() -> void:
	_http = HTTPRequest.new()
	add_child(_http)
	if world == null:
		world = get_parent().get_node_or_null("World")
	if auto_start:
		# The world mounts its tree first.
		await get_tree().create_timer(1.5).timeout
		while is_inside_tree():
			await refresh()
			await get_tree().create_timer(max(refresh_seconds, 5.0)).timeout

## Fetch everything and publish it into the world. A call during a fetch is dropped.
func refresh() -> void:
	if _busy or world == null:
		return
	_busy = true
	var data := await _collect()
	_busy = false
	if data.is_empty():
		return
	_publish(data)
	published.emit(data)
	print("[wallet] published: %s, %d token(s), %d item(s), %d listing(s)" % [data.address, data.tokens.size(), data.items.size(), data.listings.size()])

# ---- collection ----------------------------------------------------------------
func _collect() -> Dictionary:
	var addr := wallet_address
	var out := {
		"address": addr,
		"network": ADDRESSES.network,
		"registry": ADDRESSES.UGC1155,
		"marketplace": ADDRESSES.Marketplace,
		"gas": "?",
		"tokens": [],
		"items": [],
		"listings": [],
	}

	var bal := await _rpc("eth_getBalance", [addr, "latest"])
	if bal == "":
		push_warning("Wallet: RPC unreachable (%s); the teller will say the line is down" % rpc_url)
		return {}
	out.gas = _format_units(_hex_to_dec(bal), 18, 4)

	for token in ADDRESSES.tokens:
		var m := await _token_meta(token)
		var raw := await _call(token, _sel("balanceOf(address)") + _arg_addr(addr))
		if raw == "":
			continue
		out.tokens.append({
			"token": token,
			"symbol": m.symbol,
			"balance": _format_units(_hex_to_dec(raw), m.decimals, 2),
		})

	# What this wallet owns, walking the registry back from the newest id.
	var next_id := int(_hex_to_dec(await _call(ADDRESSES.UGC1155, _sel("nextId()"))))
	var lowest: int = max(1, next_id - scan_ids)
	for id in range(next_id - 1, lowest - 1, -1):
		var qty_hex := await _call(ADDRESSES.UGC1155, _sel("balanceOf(address,uint256)") + _arg_addr(addr) + _arg_uint(id))
		var qty := _hex_to_dec(qty_hex)
		if qty == "0" or qty == "":
			continue
		out.items.append({
			"id": id,
			"qty": qty,
			"uri": await _call_string(ADDRESSES.UGC1155, "uri(uint256)", _arg_uint(id)),
		})

	# Anything priced, and in which token.
	for id in range(next_id - 1, lowest - 1, -1):
		var toks := PulseBlockzChain.decode_addresses(await _call(ADDRESSES.Marketplace, _sel("tokensFor(uint256)") + _arg_uint(id)))
		for t in toks:
			var price_hex := await _call(ADDRESSES.Marketplace, _sel("price(uint256,address)") + _arg_uint(id) + _arg_addr(t))
			var m := await _token_meta(t)
			out.listings.append({
				"id": id,
				"symbol": m.symbol,
				"price": _format_units(_hex_to_dec(price_hex), m.decimals, 2),
			})
	return out

# ---- publishing into the world ---------------------------------------------------
func _publish(data: Dictionary) -> void:
	var json := JSON.stringify(data)
	# A Lua long bracket keeps the JSON verbatim; ]==] cannot appear in JSON output.
	var chunk := """
local rs = game:GetService("ReplicatedStorage")
local c = rs:FindFirstChild("Chain")
if not c then c = Instance.new("Configuration") c.Name = "Chain" c.Parent = rs end
c:SetAttribute("Json", [==[%s]==])
""" % json
	world.run_chunk("chain_publish", chunk)

# ---- JSON-RPC --------------------------------------------------------------------
## Returns the `result` string, or "" on any failure.
func _rpc(method: String, params: Array) -> String:
	var body := JSON.stringify({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})
	var err := _http.request(rpc_url, ["content-type: application/json"], HTTPClient.METHOD_POST, body)
	if err != OK:
		return ""
	var res: Array = await _http.request_completed
	if res[1] != 200:
		return ""
	var parsed = JSON.parse_string((res[3] as PackedByteArray).get_string_from_utf8())
	if typeof(parsed) != TYPE_DICTIONARY or not parsed.has("result"):
		return ""
	return String(parsed.result)

func _call(to: String, data: String) -> String:
	return await _rpc("eth_call", [{"to": to, "data": data}, "latest"])

## symbol() and decimals() never change; ask once per address.
func _token_meta(token: String) -> Dictionary:
	if _meta.has(token):
		return _meta[token]
	var sym := await _call_string(token, "symbol()")
	var dec := int(_hex_to_dec(await _call(token, _sel("decimals()"))))
	var m := {"symbol": sym if sym != "" else "?", "decimals": dec}
	_meta[token] = m
	return m

func _call_string(to: String, sig: String, extra: String = "") -> String:
	var hex := await _call(to, _sel(sig) + extra)
	if hex == "":
		return ""
	return PulseBlockzChain.decode_bytes(hex).get_string_from_utf8()

# ---- ABI helpers -------------------------------------------------------------------
func _sel(sig: String) -> String:
	return PulseBlockzCrypto.keccak256_hex(sig).substr(0, 10)

func _arg_addr(a: String) -> String:
	return "000000000000000000000000" + a.trim_prefix("0x").to_lower()

func _arg_uint(n: int) -> String:
	return ("%064x" % n)

## Hex (0x…) to a decimal string, big enough for uint256 (int64 would overflow).
func _hex_to_dec(hex: String) -> String:
	var h := hex.trim_prefix("0x").strip_edges()
	if h == "":
		return ""
	var digits: Array[int] = [0]           # little-endian base-10
	for ch in h:
		var v := "0123456789abcdef".find(ch.to_lower())
		if v < 0:
			continue
		var carry := v
		for i in digits.size():
			var cur: int = digits[i] * 16 + carry
			digits[i] = cur % 10
			carry = cur / 10
		while carry > 0:
			digits.append(carry % 10)
			carry /= 10
	var s := ""
	for i in range(digits.size() - 1, -1, -1):
		s += str(digits[i])
	return s

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
