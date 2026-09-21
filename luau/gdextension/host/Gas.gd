# Gas-bid arithmetic behind the wallet prompt (Low / Normal / Fast, tip, max fee, gas limit),
# free of the network so it can be checked alone; Wallet.gd reads the chain and draws the prompt.
# PulseChain nodes suggest a flat 10,000 gwei tip, ignored here: blocks are rarely full.
extends RefCounted

const Decimal = preload("res://host/Decimal.gd")

const SPEEDS := ["Low", "Normal", "Fast"]
const DEFAULT_SPEED := "Normal"

## Tips in gwei by chain id. On 943 a gwei lands in the next block and anything under it waits on
## a validator that will take it (0.002 gwei: 78 seconds), for 0.0002 PLS on a swap-sized call.
const PRESETS_GWEI := {943: {"Low": "0.5", "Normal": "1", "Fast": "2"}}
## For a chain with no measurement and no usable fee history.
const FALLBACK_GWEI := {"Low": "0.5", "Normal": "1", "Fast": "2"}
## 0.001 gwei, the smallest tip testnet v4 validators mine; bridge/src/fees.js holds the same floor.
const FLOOR_WEI := 1_000_000

## Estimate plus a quarter, so drift between estimating and mining does not run out of gas.
const HEADROOM_PERCENT := 125

## Wei for a gwei amount typed by a person, or -1 if it is not one.
static func gwei_to_wei(text: String) -> int:
	var t := text.strip_edges()
	var re := RegEx.create_from_string("^\\d{1,12}(\\.\\d{0,9})?$")
	if re.search(t) == null:
		return -1
	return int(Decimal.to_units(t, 9))

## A wei amount as gwei, without trailing zeros.
static func wei_to_gwei(wei: int) -> String:
	var whole := wei / 1_000_000_000
	var frac := wei % 1_000_000_000
	if frac == 0:
		return str(whole)
	return ("%d.%09d" % [whole, frac]).rstrip("0")

## A ceiling, not a price: the transaction pays base fee + tip. Twice the base leaves room for
## it to climb over several blocks, and holds on chains whose base fee is far above a gwei.
static func max_fee_for(base_wei: int, tip_wei: int) -> int:
	return base_wei * 2 + tip_wei

static func limit_from_estimate(estimate: int) -> int:
	return (estimate * HEADROOM_PERCENT + 99) / 100

## Low, Normal and Fast in wei: the chain's presets, else the median over recent blocks of each of
## eth_feeHistory's three `reward` columns (hex; 10th, 50th, 90th percentile), else the fallback.
static func presets(chain_id: int, reward_rows: Array = []) -> Dictionary:
	var out := {}
	if PRESETS_GWEI.has(chain_id):
		for s in SPEEDS:
			out[s] = gwei_to_wei(String(PRESETS_GWEI[chain_id][s]))
		return out
	var seen := [[], [], []]
	for row in reward_rows:
		if typeof(row) != TYPE_ARRAY or row.size() < 3:
			continue
		var any := false
		for i in 3:
			if String(row[i]).hex_to_int() > 0: any = true
		if not any:
			continue
		for i in 3:
			seen[i].append(String(row[i]).hex_to_int())
	for i in 3:
		var s: String = SPEEDS[i]
		if seen[i].is_empty():
			out[s] = gwei_to_wei(String(FALLBACK_GWEI[s]))
		else:
			var sorted: Array = seen[i].duplicate()
			sorted.sort()
			out[s] = int(sorted[sorted.size() / 2])
	for s in SPEEDS:
		out[s] = maxi(int(out[s]), FLOOR_WEI)
	# Faster is never cheaper than slower, whatever the history says.
	out["Normal"] = maxi(out["Normal"], out["Low"])
	out["Fast"] = maxi(out["Fast"], out["Normal"])
	return out

## The most a transaction can cost, in PLS: three significant digits however small, so a cost in
## millionths of a PLS does not read as nothing.
static func max_cost_pls(gas_limit: int, max_fee_wei: int) -> String:
	var wei := Decimal.mul(str(gas_limit), str(max_fee_wei))
	var full := Decimal.format(wei, 18, 18)
	var dot := full.find(".")
	if dot < 0 or not full.begins_with("0"):
		return Decimal.format(wei, 18, 6)
	var first := dot + 1
	while first < full.length() and full[first] == "0":
		first += 1
	return full.substr(0, mini(full.length(), first + 3))

## An integer as the 0x-hex a transaction field wants.
static func hexq(v: int) -> String:
	return "0x%x" % v

## Checks what a person typed. Returns {ok, tip_wei, max_fee_wei, gas_limit, message}.
static func read_choice(tip_text: String, max_text: String, limit_text: String, needs_limit: bool) -> Dictionary:
	var tip := gwei_to_wei(tip_text)
	var max_fee := gwei_to_wei(max_text)
	if tip < 0 or max_fee < 0:
		return {"ok": false, "message": "The tip and the max fee are amounts in gwei, like 0.01."}
	if tip < FLOOR_WEI:
		return {"ok": false, "message": "A tip under %s gwei is not mined." % wei_to_gwei(FLOOR_WEI)}
	if max_fee < tip:
		return {"ok": false, "message": "The max fee has to be at least the tip."}
	var limit := 0
	if needs_limit:
		var lt := limit_text.strip_edges()
		if not lt.is_valid_int() or int(lt) < 21000 or int(lt) > 30_000_000:
			return {"ok": false, "message": "The gas limit is a whole number from 21000 to 30000000."}
		limit = int(lt)
	return {"ok": true, "tip_wei": tip, "max_fee_wei": max_fee, "gas_limit": limit, "message": ""}
