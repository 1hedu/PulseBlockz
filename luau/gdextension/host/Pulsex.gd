# PulseX: quotes, swaps and liquidity. Chain reads through Rpc.gd, encoding through Abi.gd,
# 18-digit maths through Decimal.gd; signing and the confirm prompt stay in Wallet.gd.
# No verb here takes a URL.
class_name PulseBlockzPulsex
extends Node

const Abi = preload("res://host/Abi.gd")
const Decimal = preload("res://host/Decimal.gd")
const Tx = preload("res://host/Tx.gd")

## Handed in by the host: the Rpc.gd client and the Wallet.gd that signs.
var rpc
var wallet

## Tokens added here by address. Local to this install, never on chain.
const CUSTOM_TOKENS_FILE := "user://pulsex-tokens.json"
var _custom_tokens: Array = []

const PULSEX := {
	"chain_id": 943,
	"router": "0xDaE9dd3d1A52CfCe9d5F2fAC7fDe164D500E50f7",
	"factory": "0xFf0538782D122d3112F75dc7121F61562261c0f7",
	"wpls": "0x70499adEBB11Efd915E3b69E700c331778628707",
	"site": "app.pulsex.com",
	"screener": "dexscreener.com/pulsechain",
}

## Testnet addresses, each checked against the factory for a real WPLS pool rather than taken
## from mainnet: PLSX and INC live elsewhere here, and a decoy "Test Incentive" shares the INC
## ticker. The mainnet INC address holds no code on this chain and the decoy has no pair; the
## INC below is the one with 4.9 billion PLS pooled behind it. The empty address is the native
## coin.
const PULSEX_TOKENS := [
	{"symbol": "PLS",  "address": "", "decimals": 18, "name": "Pulse"},
	{"symbol": "PLSX", "address": "0x8a810ea8B121d08342E9e7696f4a9915cBE494B7", "decimals": 18, "name": "PulseX"},
	{"symbol": "INC",  "address": "0x6eFAfcb715F385c71d8AF763E8478FeEA6faDF63", "decimals": 18, "name": "Incentive"},
	{"symbol": "HEX",  "address": "0x2b591e99afE9f32eAA6214f7B7629768c40Eeb39", "decimals": 8,  "name": "HEX"},
	{"symbol": "USDC", "address": "0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48", "decimals": 6,  "name": "USD Coin"},
	{"symbol": "DAI",  "address": "0x6B175474E89094C44Da98b954EedeAC495271d0F", "decimals": 18, "name": "Dai Stablecoin"},
	{"symbol": "USDT", "address": "0xdAC17F958D2ee523a2206206994597C13D831ec7", "decimals": 6,  "name": "Tether USD"},
	{"symbol": "WETH", "address": "0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2", "decimals": 18, "name": "Wrapped Ether"},
]

func _call(to: String, data: String) -> String:
	return await rpc.one("eth_call", [{"to": to, "data": data}, "latest"])
## The built-in tokens plus any added by address, deduped on lowercase address.
func _pulsex_tokens() -> Array:
	var out: Array = []
	for t in PULSEX_TOKENS:
		out.append(t.duplicate())
	var seen := {}
	for t in out:
		seen[String(t.address).to_lower()] = true
	for t in _custom_tokens:
		if not seen.has(String(t.get("address", "")).to_lower()):
			var c: Dictionary = t.duplicate()
			c["custom"] = true
			out.append(c)
	return out

func _load_custom_tokens() -> void:
	_custom_tokens = []
	if not FileAccess.file_exists(CUSTOM_TOKENS_FILE):
		return
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(CUSTOM_TOKENS_FILE))
	if typeof(parsed) == TYPE_ARRAY:
		for t in parsed:
			if typeof(t) == TYPE_DICTIONARY and String(t.get("address", "")) != "":
				_custom_tokens.append(t)

func _save_custom_tokens() -> void:
	var f := FileAccess.open(CUSTOM_TOKENS_FILE, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(_custom_tokens))

## Adds a token by address. Symbol, name and decimals are read off the contract rather than
## trusted from the caller; a token with no PulseX pool is added anyway and reported as such.
func add_token(address: String) -> Dictionary:
	var addr := address.strip_edges()
	if not addr.begins_with("0x") or addr.length() != 42:
		return {"ok": false, "message": "That is not an address. It should be 0x and forty hex characters."}
	if addr.to_lower() == PULSEX.wpls.to_lower():
		return {"ok": false, "message": "That is WPLS. Trade PLS instead; the router wraps it for you."}
	for t in _pulsex_tokens():
		if String(t.get("address", "")).to_lower() == addr.to_lower():
			return {"ok": false, "message": "%s is already in your list." % t.get("symbol", "That token")}

	var code: String = await rpc.one("eth_getCode", [addr, "latest"])
	if code == "" or code == "0x":
		return {"ok": false, "message": "Nothing is deployed at that address on this chain."}

	var sym_raw := await _call(addr, Abi.selector("symbol()"))
	var dec_raw := await _call(addr, Abi.selector("decimals()"))
	if sym_raw == "" or dec_raw == "":
		return {"ok": false, "message": "That contract does not answer symbol() and decimals(), so it is not an ERC-20."}
	var meta: Dictionary = await wallet._tx_token_meta(addr)
	var symbol := String(meta.get("symbol", ""))
	if symbol == "":
		symbol = "?"

	var pair := await _pulsex_pair(addr, PULSEX.wpls)
	var note := "No PulseX pool against PLS, so it can be held but not swapped yet."
	if pair != "":
		note = "Pooled against PLS on PulseX."

	_custom_tokens.append({"symbol": symbol, "address": addr,
		"decimals": int(meta.get("decimals", 18)), "name": String(meta.get("name", symbol))})
	_save_custom_tokens()
	return {"ok": true, "message": "%s added. %s" % [symbol, note],
		"symbol": symbol, "address": addr, "pooled": pair != ""}

func forget_token(address: String) -> Dictionary:
	var addr := address.strip_edges().to_lower()
	for i in _custom_tokens.size():
		if String(_custom_tokens[i].get("address", "")).to_lower() == addr:
			var gone: Dictionary = _custom_tokens[i]
			_custom_tokens.remove_at(i)
			_save_custom_tokens()
			return {"ok": true, "message": "%s removed from your list." % gone.get("symbol", "It")}
	return {"ok": false, "message": "That one is not in your list. The tokens it ships with cannot be removed."}

## Factory lookups, keyed "a|b" lowercase. A pool's address never changes once it exists, so a
## hit is kept for good; a miss is asked again after PAIR_RETRY_SEC, in case one was made since.
const PAIR_RETRY_SEC := 300
var _pairs := {}
var _pairs_missing_at := {}

func _pair_key(a: String, b: String) -> String:
	return a.to_lower() + "|" + b.to_lower()

func _pair_known(a: String, b: String) -> bool:
	var key := _pair_key(a, b)
	if not _pairs.has(key):
		return false
	if String(_pairs[key]) != "":
		return true
	return Time.get_unix_time_from_system() - float(_pairs_missing_at.get(key, 0)) < PAIR_RETRY_SEC

func _pair_remember(a: String, b: String, pair: String) -> void:
	var key := _pair_key(a, b)
	_pairs[key] = pair
	_pairs[_pair_key(b, a)] = pair
	if pair == "":
		_pairs_missing_at[key] = Time.get_unix_time_from_system()
		_pairs_missing_at[_pair_key(b, a)] = _pairs_missing_at[key]

## The pool for two tokens, or "" if the factory has never made one.
func _pulsex_pair(a: String, b: String) -> String:
	if _pair_known(a, b):
		return String(_pairs[_pair_key(a, b)])
	var raw := await _call(PULSEX.factory,
		Abi.selector("getPair(address,address)") + Abi.arg_address(a) + Abi.arg_address(b))
	if raw == "":
		return ""
	var addr := "0x" + Abi.word(raw, 0).substr(24)
	var pair := "" if addr == "0x" + "0".repeat(40) else addr
	_pair_remember(a, b, pair)
	return pair

## The swap path. PLS on either side becomes WPLS -- PulseX holds no pair for the native coin,
## and the router's ETH entry points do the wrapping. Tokens with no direct pair hop via WPLS.
func _pulsex_path(from_addr: String, to_addr: String) -> Array:
	var a := from_addr if from_addr != "" else PULSEX.wpls
	var b := to_addr if to_addr != "" else PULSEX.wpls
	if a.to_lower() == b.to_lower():
		return []
	if a.to_lower() == PULSEX.wpls.to_lower() or b.to_lower() == PULSEX.wpls.to_lower():
		return [a, b]
	var direct := await _pulsex_pair(a, b)
	if direct != "":
		return [a, b]
	return [a, PULSEX.wpls, b]

## Whether the router's allowance is short of `units`. Read before the prompt goes up, so it can
## say whether this is one signature or two. The native coin travels with the call and needs none.
func _needs_approval(token: String, units: String) -> bool:
	if token == "":
		return false
	var standing := Decimal.from_hex(await _call(token,
		Abi.selector("allowance(address,address)") + Abi.arg_address(wallet.wallet_address) + Abi.arg_address(PULSEX.router)))
	return Decimal.less_than(standing, units)

func _token_by_address(addr: String) -> Dictionary:
	for t in _pulsex_tokens():
		if String(t.get("address", "")).to_lower() == addr.to_lower():
			return t
	return {}

## A router quote. Price impact is the V2 identity: for reserve R and input x, execution price
## over mid price is (3R + 997x) / (1000R + 997x), the 0.3% fee included, as PulseX shows it.
func pulsex_quote(from_addr: String, to_addr: String, amount: String, slippage_bps: int = 50) -> Dictionary:
	var from_t := _token_by_address(from_addr) if from_addr != "" else PULSEX_TOKENS[0]
	var to_t := _token_by_address(to_addr) if to_addr != "" else PULSEX_TOKENS[0]
	if from_t.is_empty() or to_t.is_empty():
		return {"ok": false, "message": "I do not know one of those tokens."}
	var path := await _pulsex_path(from_addr, to_addr)
	if path.is_empty():
		return {"ok": false, "message": "Those are the same token."}
	var units := Decimal.to_units(amount, int(from_t.decimals))
	if units == "" or units == "0":
		return {"ok": false, "message": "Enter an amount"}

	var raw := await _call(PULSEX.router,
		Abi.selector("getAmountsOut(uint256,address[])") + Abi.encode(["uint256", "address[]"], [units, path]))
	if raw == "":
		return {"ok": false, "message": "Insufficient liquidity for this trade."}
	# uint256[] comes back as offset, length, then n amounts; the last is the output.
	var n := int(("0x" + Abi.word(raw, 1).lstrip("0")).hex_to_int()) if Abi.word(raw, 1).lstrip("0") != "" else 0
	if n < 2:
		return {"ok": false, "message": "Insufficient liquidity for this trade."}
	var out_units := Decimal.from_hex("0x" + Abi.word(raw, 1 + n))
	if out_units == "" or out_units == "0":
		return {"ok": false, "message": "Insufficient liquidity for this trade."}

	# Impact is measured on the first hop only.
	var impact_bps := 0
	var first := await _pulsex_pair(path[0], path[1])
	if first != "":
		var res := await _call(first, Abi.selector("getReserves()"))
		var t0 := await _call(first, Abi.selector("token0()"))
		if res != "" and t0 != "":
			var r0 := Decimal.from_hex("0x" + Abi.word(res, 0))
			var r1 := Decimal.from_hex("0x" + Abi.word(res, 1))
			var token0 := "0x" + Abi.word(t0, 0).substr(24)
			var reserve_in := r0 if token0.to_lower() == String(path[0]).to_lower() else r1
			var num := Decimal.add(Decimal.mul_small(reserve_in, 3), Decimal.mul_small(units, 997))
			var den := Decimal.add(Decimal.mul_small(reserve_in, 1000), Decimal.mul_small(units, 997))
			impact_bps = Decimal.ratio_bps(num, den)

	var min_out := Decimal.mul_div(out_units, str(10000 - slippage_bps), "10000")
	var names := []
	for step in path:
		var t := _token_by_address(String(step))
		names.append(String(t.get("symbol", "?")) if not t.is_empty() else "WPLS")
	return {
		"ok": true,
		"out": Decimal.format(out_units, int(to_t.decimals), 6),
		"out_units": out_units,
		"min_received": Decimal.format(min_out, int(to_t.decimals), 6),
		"min_units": min_out,
		"impact_bps": impact_bps,
		"impact": "%.2f%%" % (impact_bps / 100.0),
		"route": " > ".join(names),
		"path": path,
		"from_symbol": from_t.symbol, "to_symbol": to_t.symbol,
	}

## Approves the router for exactly `units`, never the unlimited amount: a forgotten approval is
## standing permission to empty that token. Costs a transaction on every trade.
func _pulsex_approve(token: String, units: String, fee: Dictionary = {}) -> Dictionary:
	var allowance := Decimal.from_hex(await _call(token,
		Abi.selector("allowance(address,address)") + Abi.arg_address(wallet.wallet_address) + Abi.arg_address(PULSEX.router)))
	if not Decimal.less_than(allowance, units):
		return {"ok": true, "message": "already approved"}
	var data := Abi.selector("approve(address,uint256)") + Abi.encode(["address", "uint256"], [PULSEX.router, units])
	# Nothing has to land before this one, so the chain can estimate it rather than guess.
	var limit: Dictionary = await wallet.gas_for(token, data, "0x0", 120000)
	var known := _token_by_address(token)
	return await wallet._send(token, data, int(limit.gas), "0x0", fee,
		"Approval: %s for PulseX" % String(known.get("symbol", "a token")))

func pulsex_swap(from_addr: String, to_addr: String, amount: String, slippage_bps: int = 50) -> Dictionary:
	if wallet._key == "":
		return {"ok": false, "message": "No key loaded, so I can't sign anything for you."}
	var quote := await pulsex_quote(from_addr, to_addr, amount, slippage_bps)
	if not quote.get("ok", false):
		return quote
	var from_t := _token_by_address(from_addr) if from_addr != "" else PULSEX_TOKENS[0]
	var units := Decimal.to_units(amount, int(from_t.decimals))

	if from_addr == "":
		var bal := Decimal.from_hex(await rpc.one("eth_getBalance", [wallet.wallet_address, "latest"]))
		if Decimal.less_than(bal, units):
			return {"ok": false, "message": "Insufficient PLS balance."}
	else:
		var held := Decimal.from_hex(await _call(from_addr, Abi.selector("balanceOf(address)") + Abi.arg_address(wallet.wallet_address)))
		if Decimal.less_than(held, units):
			return {"ok": false, "message": "Insufficient %s balance." % from_t.symbol}

	# Settled before the prompt, which has to say whether this is one transaction or two.
	var needs_approve := false
	if from_addr != "":
		var standing := Decimal.from_hex(await _call(from_addr,
			Abi.selector("allowance(address,address)") + Abi.arg_address(wallet.wallet_address) + Abi.arg_address(PULSEX.router)))
		needs_approve = Decimal.less_than(standing, units)

	var warning := ""
	if int(quote.impact_bps) >= 500:
		warning = "\nPrice impact %s -- the pool is thin for a trade this size." % quote.impact
	var fee: Dictionary = await wallet._fee_quote("", "", "", 0)
	if not await wallet._confirm("Swap on PulseX?",
			"%s %s for about %s %s\nAt least %s %s after %.2f%% slippage\nRoute %s%s" %
			[amount, quote.from_symbol, quote.out, quote.to_symbol,
			 quote.min_received, quote.to_symbol, slippage_bps / 100.0, quote.route,
			 warning + ("\nTwo transactions: letting the router take the %s, then the swap." % quote.from_symbol
				if needs_approve else "\nOne transaction.")], PackedStringArray(), fee):
		return {"ok": false, "message": "You waved that one off. Nothing was sent."}

	var deadline := int(Time.get_unix_time_from_system()) + 1200
	var path: Array = quote.path
	var data := ""
	var value := "0x0"
	var gas := 300000

	if from_addr == "":
		data = Abi.selector("swapExactETHForTokensSupportingFeeOnTransferTokens(uint256,address[],address,uint256)") \
			+ Abi.encode(["uint256", "address[]", "address", "uint256"],
				[quote.min_units, path, wallet.wallet_address, str(deadline)])
		value = "0x" + Tx.arg_uint_dec(units).lstrip("0")
		if value == "0x": value = "0x0"
	else:
		var approved := await _pulsex_approve(from_addr, units, fee)
		if not approved.get("ok", false):
			return {"ok": false, "message": "The approval failed: " + String(approved.get("message", ""))}
		if to_addr == "":
			data = Abi.selector("swapExactTokensForETHSupportingFeeOnTransferTokens(uint256,uint256,address[],address,uint256)") \
				+ Abi.encode(["uint256", "uint256", "address[]", "address", "uint256"],
					[units, quote.min_units, path, wallet.wallet_address, str(deadline)])
		else:
			data = Abi.selector("swapExactTokensForTokensSupportingFeeOnTransferTokens(uint256,uint256,address[],address,uint256)") \
				+ Abi.encode(["uint256", "uint256", "address[]", "address", "uint256"],
					[units, quote.min_units, path, wallet.wallet_address, str(deadline)])

	var limit: Dictionary = await wallet.gas_for(PULSEX.router, data, value, gas)
	if String(limit.refused) != "":
		return {"ok": false, "message": "The chain says that swap would fail: %s. Nothing was sent." % limit.refused}
	var sent: Dictionary = await wallet._send_value(PULSEX.router, data, int(limit.gas), value, fee, "Swap")
	if not sent.get("ok", false):
		return {"ok": false, "message": "The swap didn't go through: " + String(sent.get("message", "")),
			"hash": sent.get("hash", "")}
	return {"ok": true, "hash": sent.get("hash", ""),
		"message": "Swapped %s %s for %s. It settled on PulseX, on this chain." %
			[amount, quote.from_symbol, quote.to_symbol]}

## Every pool with a non-zero LP balance, over all pairs among the known tokens rather than the
## PLS ones alone. The lookups are quadratic in the token list, so they go in one batch.
func pulsex_positions() -> Array:
	var out := []
	var tokens := _pulsex_tokens()
	var combos := []
	var calls := []
	for i in tokens.size():
		for j in range(i + 1, tokens.size()):
			var a: Dictionary = tokens[i]
			var b: Dictionary = tokens[j]
			# PLS folds into WPLS, so the PLS/WPLS combination drops out as a self-pair.
			var ax := PULSEX.wpls if String(a.address) == "" else String(a.address)
			var bx := PULSEX.wpls if String(b.address) == "" else String(b.address)
			if ax.to_lower() == bx.to_lower():
				continue
			var combo := {"a": a, "b": b, "ax": ax, "bx": bx}
			if _pair_known(ax, bx):
				combo["pair"] = String(_pairs[_pair_key(ax, bx)])
			else:
				combo["ask"] = calls.size()
				calls.append(Abi.call_spec(PULSEX.factory,
					Abi.selector("getPair(address,address)") + Abi.arg_address(ax) + Abi.arg_address(bx)))
			combos.append(combo)
	if combos.is_empty():
		return out
	var found: Array = []
	if not calls.is_empty():
		found = await rpc.many(calls)
		if found.is_empty():
			return out

	var live := []
	for c in combos:
		if c.has("ask"):
			var i: int = c["ask"]
			var raw := String(found[i]) if i < found.size() else ""
			if raw == "":
				continue
			var pair := "0x" + Abi.word(raw, 0).substr(24)
			if pair == "0x" + "0".repeat(40):
				pair = ""
			_pair_remember(String(c.ax), String(c.bx), pair)
			c["pair"] = pair
		if String(c.get("pair", "")) == "":
			continue
		live.append(c)
	if live.is_empty():
		return out

	calls = []
	for c in live:
		calls.append(Abi.call_spec(c.pair, Abi.selector("balanceOf(address)") + Abi.arg_address(wallet.wallet_address)))
		calls.append(Abi.call_spec(c.pair, Abi.selector("totalSupply()")))
		calls.append(Abi.call_spec(c.pair, Abi.selector("getReserves()")))
		calls.append(Abi.call_spec(c.pair, Abi.selector("token0()")))
	var r: Array = await rpc.many(calls)
	if r.is_empty():
		return out

	for i in live.size():
		var bal := Decimal.from_hex(String(r[i * 4]))
		if bal == "" or bal == "0":
			continue
		var supply := Decimal.from_hex(String(r[i * 4 + 1]))
		var res := String(r[i * 4 + 2])
		var t0raw := String(r[i * 4 + 3])
		if supply == "0" or res == "" or t0raw == "":
			continue
		var c: Dictionary = live[i]
		var r0 := Decimal.from_hex("0x" + Abi.word(res, 0))
		var r1 := Decimal.from_hex("0x" + Abi.word(res, 1))
		var token0 := "0x" + Abi.word(t0raw, 0).substr(24)
		var a_is_0 := token0.to_lower() == String(c.ax).to_lower()
		# The amounts below go through mul_div, not share_bps: a share under 0.01% rounds to
		# zero in basis points and would report as holding nothing.
		var share_bps := Decimal.ratio_bps(bal, supply)
		out.append({
			"symbol": c.a.symbol, "symbol_b": c.b.symbol,
			"address": String(c.a.address), "address_b": String(c.b.address),
			"pair": c.pair,
			"lp": Decimal.format(bal, 18, 6), "lp_units": bal,
			"share": "%.2f%%" % (share_bps / 100.0),
			"token_amount": Decimal.format(Decimal.mul_div(r0 if a_is_0 else r1, bal, supply), int(c.a.decimals), 4),
			"pls_amount": Decimal.format(Decimal.mul_div(r1 if a_is_0 else r0, bal, supply), int(c.b.decimals), 4),
		})
	return out

## Everything the Add Liquidity card shows before anyone signs. `side` names which amount was
## typed, "a" or "b"; the other comes back computed from the pool's ratio. An empty amount still
## answers the balances and the prices. Reads only.
func pulsex_pool_quote(a_addr: String, b_addr: String, amount: String, side: String = "a") -> Dictionary:
	var ta := _token_by_address(a_addr)
	var tb := _token_by_address(b_addr)
	if ta.is_empty() or tb.is_empty():
		return {"ok": false, "message": "Select a token"}
	var ax := PULSEX.wpls if a_addr == "" else a_addr
	var bx := PULSEX.wpls if b_addr == "" else b_addr
	if ax.to_lower() == bx.to_lower():
		return {"ok": false, "message": "Those are the same token."}
	var out := {"ok": true, "a_symbol": ta.symbol, "b_symbol": tb.symbol, "a_amount": "", "b_amount": "",
		"a_balance": "", "b_balance": "", "price_ab": "", "price_ba": "", "share": "", "lp": "", "pool": true}

	# With no address signed in there are no balances to read; the card still shows the pool.
	var me := String(wallet.wallet_address)
	var calls := []
	if a_addr != "" and me != "":
		calls.append(Abi.call_spec(a_addr, Abi.selector("balanceOf(address)") + Abi.arg_address(me)))
	if b_addr != "" and me != "":
		calls.append(Abi.call_spec(b_addr, Abi.selector("balanceOf(address)") + Abi.arg_address(me)))
	var pair := await _pair_for(a_addr, b_addr)
	if pair != "":
		calls.append(Abi.call_spec(pair, Abi.selector("getReserves()")))
		calls.append(Abi.call_spec(pair, Abi.selector("token0()")))
		calls.append(Abi.call_spec(pair, Abi.selector("totalSupply()")))
	var r: Array = []
	if not calls.is_empty():
		r = await rpc.many(calls)
	var at := 0
	var a_bal := ""
	var b_bal := ""
	if (a_addr == "" or b_addr == "") and me != "":
		var pls := Decimal.from_hex(await rpc.one("eth_getBalance", [me, "latest"]))
		if a_addr == "":
			a_bal = pls
		else:
			b_bal = pls
	if a_addr != "" and me != "":
		a_bal = Decimal.from_hex(String(r[at])) if at < r.size() else ""
		at += 1
	if b_addr != "" and me != "":
		b_bal = Decimal.from_hex(String(r[at])) if at < r.size() else ""
		at += 1
	out["a_balance"] = Decimal.format(a_bal, int(ta.decimals), 4) if a_bal != "" else ""
	out["b_balance"] = Decimal.format(b_bal, int(tb.decimals), 4) if b_bal != "" else ""
	# Raw units as well: the shown figure is rounded to four places, and a Max button filling
	# an input from that would leave the rest behind as dust.
	out["a_balance_units"] = a_bal
	out["b_balance_units"] = b_bal
	out["a_decimals"] = int(ta.decimals)
	out["b_decimals"] = int(tb.decimals)
	if pair == "":
		out["pool"] = false
		out["message"] = "There is no %s/%s pool on PulseX yet." % [ta.symbol, tb.symbol]
		return out

	var res := String(r[at]) if at < r.size() else ""
	var t0 := String(r[at + 1]) if at + 1 < r.size() else ""
	var supply := Decimal.from_hex(String(r[at + 2])) if at + 2 < r.size() else ""
	if res == "" or t0 == "" or supply == "":
		return {"ok": false, "message": "The pool did not answer."}
	var r0 := Decimal.from_hex("0x" + Abi.word(res, 0))
	var r1 := Decimal.from_hex("0x" + Abi.word(res, 1))
	var token0 := "0x" + Abi.word(t0, 0).substr(24)
	var a_is_0 := token0.to_lower() == ax.to_lower()
	var ra := r0 if a_is_0 else r1
	var rb := r1 if a_is_0 else r0
	if ra == "0" or rb == "0" or supply == "0":
		out["pool"] = false
		out["message"] = "The %s/%s pool is empty." % [ta.symbol, tb.symbol]
		return out

	# Both directions, as PulseX prints them under "Prices and pool share".
	var one_a := "1" + "0".repeat(int(ta.decimals))
	var one_b := "1" + "0".repeat(int(tb.decimals))
	out["price_ab"] = Decimal.format(Decimal.mul_div(rb, one_a, ra), int(tb.decimals), 6)   # B per one A
	out["price_ba"] = Decimal.format(Decimal.mul_div(ra, one_b, rb), int(ta.decimals), 6)   # A per one B

	var typed_t := tb if side == "b" else ta
	var units := Decimal.to_units(amount, int(typed_t.decimals))
	if units == "" or units == "0":
		return out
	var a_units := units if side != "b" else Decimal.mul_div(units, ra, rb)
	var b_units := units if side == "b" else Decimal.mul_div(units, rb, ra)
	out["a_amount"] = amount if side != "b" else Decimal.format(a_units, int(ta.decimals), 6)
	out["b_amount"] = amount if side == "b" else Decimal.format(b_units, int(tb.decimals), 6)
	out["a_short"] = a_bal != "" and Decimal.less_than(a_bal, a_units)
	out["b_short"] = b_bal != "" and Decimal.less_than(b_bal, b_units)
	# The pair contract mints the smaller of the two sides' claims; the share is against the
	# supply after that mint.
	var lp_a := Decimal.mul_div(a_units, supply, ra)
	var lp_b := Decimal.mul_div(b_units, supply, rb)
	var lp := lp_a if Decimal.less_than(lp_a, lp_b) else lp_b
	out["lp"] = Decimal.format(lp, 18, 6)
	var share_bps := Decimal.ratio_bps(lp, Decimal.add(supply, lp))
	if share_bps == 0:
		out["share"] = "<0.01%" if lp != "0" else "0%"
	else:
		out["share"] = "%.2f%%" % (share_bps / 100.0)
	return out

## The pool for two tokens, with PLS folded into WPLS.
func _pair_for(a_addr: String, b_addr: String) -> String:
	var ax := PULSEX.wpls if a_addr == "" else a_addr
	var bx := PULSEX.wpls if b_addr == "" else b_addr
	if ax.to_lower() == bx.to_lower():
		return ""
	return await _pulsex_pair(ax, bx)

## Adds liquidity to any pair. The second amount comes from the pool's ratio rather than being
## asked for: the router refunds a lopsided pair anyway, at the cost of the gas. A native side
## goes through addLiquidityETH with the coin as the call's value; two ERC-20s through
## addLiquidity.
func pulsex_add_liquidity(a_addr: String, b_addr: String, a_amount: String, slippage_bps: int = 100) -> Dictionary:
	if wallet._key == "":
		return {"ok": false, "message": "No key loaded, so I can't sign anything for you."}
	var ta := _token_by_address(a_addr)
	var tb := _token_by_address(b_addr)
	if ta.is_empty() or tb.is_empty():
		return {"ok": false, "message": "I do not know one of those tokens."}
	var pair := await _pair_for(a_addr, b_addr)
	if pair == "":
		return {"ok": false, "message": "There is no %s/%s pool to add to." % [ta.symbol, tb.symbol]}

	var a_units := Decimal.to_units(a_amount, int(ta.decimals))
	if a_units == "" or a_units == "0":
		return {"ok": false, "message": "Enter an amount"}

	var res := await _call(pair, Abi.selector("getReserves()"))
	var t0 := await _call(pair, Abi.selector("token0()"))
	if res == "" or t0 == "":
		return {"ok": false, "message": "The pool did not answer."}
	var ax := PULSEX.wpls if a_addr == "" else a_addr
	var r0 := Decimal.from_hex("0x" + Abi.word(res, 0))
	var r1 := Decimal.from_hex("0x" + Abi.word(res, 1))
	var token0 := "0x" + Abi.word(t0, 0).substr(24)
	var a_is_0 := token0.to_lower() == ax.to_lower()
	var b_units := Decimal.mul_div(a_units, r1 if a_is_0 else r0, r0 if a_is_0 else r1)
	if b_units == "0":
		return {"ok": false, "message": "That is too little %s to pair against %s." % [ta.symbol, tb.symbol]}

	if b_addr == "":
		var gas_bal := Decimal.from_hex(await rpc.one("eth_getBalance", [wallet.wallet_address, "latest"]))
		if Decimal.less_than(gas_bal, b_units):
			return {"ok": false, "message": "You need about %s PLS to pair with that." % Decimal.format(b_units, 18, 4)}
	else:
		var held := Decimal.from_hex(await _call(b_addr, Abi.selector("balanceOf(address)") + Abi.arg_address(wallet.wallet_address)))
		if Decimal.less_than(held, b_units):
			return {"ok": false, "message": "You need about %s %s to pair with that, and you have %s." %
				[Decimal.format(b_units, int(tb.decimals), 4), tb.symbol, Decimal.format(held, int(tb.decimals), 4)]}

	var supply := Decimal.from_hex(await _call(pair, Abi.selector("totalSupply()")))
	var got := "LP tokens for the pool"
	if supply != "" and supply != "0":
		var lp_a := Decimal.mul_div(a_units, supply, r0 if a_is_0 else r1)
		var lp_b := Decimal.mul_div(b_units, supply, r1 if a_is_0 else r0)
		var lp := lp_a if Decimal.less_than(lp_a, lp_b) else lp_b
		var share_bps := Decimal.ratio_bps(lp, Decimal.add(supply, lp))
		var share := "under 0.01%" if share_bps == 0 else "%.2f%%" % (share_bps / 100.0)
		got = "about %s %s/%s LP tokens, %s of the pool" % [Decimal.format(lp, 18, 6), ta.symbol, tb.symbol, share]
	# Each side needing an approval is one more signature and one more lot of gas.
	var first := PackedStringArray()
	for side in [[a_addr, a_units, ta.symbol], [b_addr, b_units, tb.symbol]]:
		if await _needs_approval(String(side[0]), String(side[1])):
			first.append(String(side[2]))
	var how_many := "\nOne transaction." if first.is_empty() \
		else "\n%d transactions: letting the router take the %s, then adding the liquidity." % [
			first.size() + 1, " and the ".join(first)]
	var fee: Dictionary = await wallet._fee_quote("", "", "", 0)
	if not await wallet._confirm("Add liquidity on PulseX?",
			"%s %s and about %s %s\nYou get %s, and the fees it earns.\nYou can take it out again at any time.%s" %
			[a_amount, ta.symbol, Decimal.format(b_units, int(tb.decimals), 4), tb.symbol, got, how_many], PackedStringArray(), fee):
		return {"ok": false, "message": "You waved that one off. Nothing was sent."}

	var keep := str(10000 - clampi(slippage_bps, 1, 5000))
	var a_min := Decimal.mul_div(a_units, keep, "10000")
	var b_min := Decimal.mul_div(b_units, keep, "10000")
	var deadline := int(Time.get_unix_time_from_system()) + 1200

	for side in [[a_addr, a_units], [b_addr, b_units]]:
		if String(side[0]) == "":
			continue
		var approved := await _pulsex_approve(String(side[0]), String(side[1]), fee)
		if not approved.get("ok", false):
			return {"ok": false, "message": "The approval failed: " + String(approved.get("message", ""))}

	var sent := {}
	if a_addr == "" or b_addr == "":
		var tok := b_addr if a_addr == "" else a_addr
		var tok_units := b_units if a_addr == "" else a_units
		var tok_min := b_min if a_addr == "" else a_min
		var pls_units := a_units if a_addr == "" else b_units
		var pls_min := a_min if a_addr == "" else b_min
		var data := Abi.selector("addLiquidityETH(address,uint256,uint256,uint256,address,uint256)") + Abi.encode(
			["address", "uint256", "uint256", "uint256", "address", "uint256"],
			[tok, tok_units, tok_min, pls_min, wallet.wallet_address, str(deadline)])
		var value := "0x" + Tx.arg_uint_dec(pls_units).lstrip("0")
		if value == "0x":
			value = "0x0"
		var limit: Dictionary = await wallet.gas_for(PULSEX.router, data, value, 400000)
		if String(limit.refused) != "":
			return {"ok": false, "message": "The chain says that would fail: %s. Nothing was sent." % limit.refused}
		sent = await wallet._send_value(PULSEX.router, data, int(limit.gas), value, fee, "Liquidity in")
	else:
		var data2 := Abi.selector("addLiquidity(address,address,uint256,uint256,uint256,uint256,address,uint256)") + Abi.encode(
			["address", "address", "uint256", "uint256", "uint256", "uint256", "address", "uint256"],
			[a_addr, b_addr, a_units, b_units, a_min, b_min, wallet.wallet_address, str(deadline)])
		var limit2: Dictionary = await wallet.gas_for(PULSEX.router, data2, "0x0", 420000)
		if String(limit2.refused) != "":
			return {"ok": false, "message": "The chain says that would fail: %s. Nothing was sent." % limit2.refused}
		sent = await wallet._send(PULSEX.router, data2, int(limit2.gas), "0x0", fee, "Liquidity in")
	if not sent.get("ok", false):
		return {"ok": false, "message": "Adding liquidity didn't go through: " + String(sent.get("message", ""))}
	return {"ok": true, "hash": sent.get("hash", ""),
		"message": "You are in the %s/%s pool. Your share earns a cut of every trade through it." % [ta.symbol, tb.symbol]}

## Takes a share back out, given as a percent of the LP balance rather than in LP tokens.
func pulsex_remove_liquidity(a_addr: String, b_addr: String, percent: int, slippage_bps: int = 100) -> Dictionary:
	if wallet._key == "":
		return {"ok": false, "message": "No key loaded, so I can't sign anything for you."}
	var ta := _token_by_address(a_addr)
	var tb := _token_by_address(b_addr)
	if ta.is_empty() or tb.is_empty():
		return {"ok": false, "message": "I do not know one of those tokens."}
	var pair := await _pair_for(a_addr, b_addr)
	if pair == "":
		return {"ok": false, "message": "There is no %s/%s pool." % [ta.symbol, tb.symbol]}
	percent = clampi(percent, 1, 100)
	var bal := Decimal.from_hex(await _call(pair, Abi.selector("balanceOf(address)") + Abi.arg_address(wallet.wallet_address)))
	if bal == "" or bal == "0":
		return {"ok": false, "message": "You have no share of the %s/%s pool." % [ta.symbol, tb.symbol]}
	var lp_units := bal if percent == 100 else Decimal.mul_div(bal, str(percent), "100")

	# Minimums the router reverts below: this position's share of the reserves, less slippage_bps.
	# Empty pair reads leave both at "0", and the router then checks no price at all.
	var supply := Decimal.from_hex(await _call(pair, Abi.selector("totalSupply()")))
	var res := await _call(pair, Abi.selector("getReserves()"))
	var t0 := await _call(pair, Abi.selector("token0()"))
	var a_min := "0"
	var b_min := "0"
	var back := "both sides back"
	if supply != "" and supply != "0" and res != "" and t0 != "":
		var ax := PULSEX.wpls if a_addr == "" else a_addr
		var r0 := Decimal.from_hex("0x" + Abi.word(res, 0))
		var r1 := Decimal.from_hex("0x" + Abi.word(res, 1))
		var token0 := "0x" + Abi.word(t0, 0).substr(24)
		var a_is_0 := token0.to_lower() == ax.to_lower()
		var a_out := Decimal.mul_div(r0 if a_is_0 else r1, lp_units, supply)
		var b_out := Decimal.mul_div(r1 if a_is_0 else r0, lp_units, supply)
		var keep := str(10000 - clampi(slippage_bps, 1, 5000))
		a_min = Decimal.mul_div(a_out, keep, "10000")
		b_min = Decimal.mul_div(b_out, keep, "10000")
		back = "about %s %s and %s %s back" % [Decimal.format(a_out, int(ta.decimals), 4), ta.symbol,
			Decimal.format(b_out, int(tb.decimals), 4), tb.symbol]

	# LP tokens are an ERC-20 too, so the router needs its own allowance on the pair.
	var allowance := Decimal.from_hex(await _call(pair,
		Abi.selector("allowance(address,address)") + Abi.arg_address(wallet.wallet_address) + Abi.arg_address(PULSEX.router)))
	var approve_first := Decimal.less_than(allowance, lp_units)
	var fee: Dictionary = await wallet._fee_quote("", "", "", 0)
	if not await wallet._confirm("Take liquidity out of PulseX?",
			"%d%% of your %s/%s position: %s LP tokens\nYou get %s, plus the fees it has earned.\nThe LP tokens are burned.%s" %
			[percent, ta.symbol, tb.symbol, Decimal.format(lp_units, 18, 6), back,
			 "\nTwo transactions: letting the router take the LP tokens, then taking the liquidity out."
				if approve_first else "\nOne transaction."], PackedStringArray(), fee):
		return {"ok": false, "message": "You waved that one off. Nothing was sent."}

	if approve_first:
		var approve_data := Abi.selector("approve(address,uint256)") + Abi.encode(["address", "uint256"], [PULSEX.router, lp_units])
		var approve_gas: Dictionary = await wallet.gas_for(pair, approve_data, "0x0", 120000)
		var ok: Dictionary = await wallet._send(pair, approve_data, int(approve_gas.gas), "0x0", fee, "Approval: LP tokens for PulseX")
		if not ok.get("ok", false):
			return {"ok": false, "message": "The LP approval failed: " + String(ok.get("message", ""))}

	var deadline := int(Time.get_unix_time_from_system()) + 1200
	var sent := {}
	if a_addr == "" or b_addr == "":
		var tok := b_addr if a_addr == "" else a_addr
		var tok_min := b_min if a_addr == "" else a_min
		var pls_min := a_min if a_addr == "" else b_min
		# The fee-on-transfer variant: a token that taxes transfers makes the plain call revert
		# on its own minimum check.
		var data := Abi.selector("removeLiquidityETHSupportingFeeOnTransferTokens(address,uint256,uint256,uint256,address,uint256)") + Abi.encode(
			["address", "uint256", "uint256", "uint256", "address", "uint256"],
			[tok, lp_units, tok_min, pls_min, wallet.wallet_address, str(deadline)])
		# 560,000: the call measures about 480,000 here, and running out of gas still pays the
		# whole limit.
		var limit: Dictionary = await wallet.gas_for(PULSEX.router, data, "0x0", 560000)
		if String(limit.refused) != "":
			return {"ok": false, "message": "The chain says that would fail: %s. Nothing was sent." % limit.refused}
		sent = await wallet._send(PULSEX.router, data, int(limit.gas), "0x0", fee, "Liquidity out")
	else:
		var data2 := Abi.selector("removeLiquidity(address,address,uint256,uint256,uint256,address,uint256)") + Abi.encode(
			["address", "address", "uint256", "uint256", "uint256", "address", "uint256"],
			[a_addr, b_addr, lp_units, a_min, b_min, wallet.wallet_address, str(deadline)])
		var limit2: Dictionary = await wallet.gas_for(PULSEX.router, data2, "0x0", 560000)
		if String(limit2.refused) != "":
			return {"ok": false, "message": "The chain says that would fail: %s. Nothing was sent." % limit2.refused}
		sent = await wallet._send(PULSEX.router, data2, int(limit2.gas), "0x0", fee, "Liquidity out")
	if not sent.get("ok", false):
		return {"ok": false, "message": "Removing liquidity didn't go through: " + String(sent.get("message", ""))}
	return {"ok": true, "hash": sent.get("hash", ""),
		"message": "Took %d%% of your %s/%s position back out." % [percent, ta.symbol, tb.symbol]}
