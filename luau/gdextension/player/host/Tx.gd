# COPY -- do not edit. The original is luau/gdextension/host/Tx.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# EIP-1559 transactions: RLP, envelope and ABI in GDScript; the curve work is one call into the
# GDExtension (PulseBlockzCrypto.sign_digest). Type 2 only: PulseChain is post-London and the fee
# overrides in bridge/src/fees.js assume that envelope. Luau asks the host; the key stays here.
extends RefCounted
class_name PulseBlockzTx

# ---- RLP ---------------------------------------------------------------------------
## A byte string: one byte below 0x80 is itself, anything else takes a length prefix.
static func rlp_bytes(b: PackedByteArray) -> PackedByteArray:
	if b.size() == 1 and b[0] < 0x80:
		return b
	var out := _prefix(0x80, b.size())
	out.append_array(b)
	return out

## A list whose items are **already** RLP-encoded.
static func rlp_list(items: Array) -> PackedByteArray:
	var payload := PackedByteArray()
	for item in items:
		payload.append_array(item)
	var out := _prefix(0xc0, payload.size())
	out.append_array(payload)
	return out

static func _prefix(base: int, n: int) -> PackedByteArray:
	var out := PackedByteArray()
	if n <= 55:
		out.append(base + n)
	else:
		var lb := int_bytes(n)
		out.append(base + 55 + lb.size())
		out.append_array(lb)
	return out

## Minimal big-endian; zero is the empty string. A leading zero byte is a different, invalid
## encoding under RLP.
static func int_bytes(n: int) -> PackedByteArray:
	var out := PackedByteArray()
	while n > 0:
		out.insert(0, n & 0xff)
		n >>= 8
	return out

## Same, from a 0x-hex quantity (an RPC result, or a uint256 too large for a 64-bit int).
static func hex_bytes(h: String) -> PackedByteArray:
	var s := h.trim_prefix("0x").strip_edges()
	while s.length() > 1 and s.begins_with("0"):
		s = s.substr(1)
	if s == "" or s == "0":
		return PackedByteArray()
	if s.length() % 2 == 1:
		s = "0" + s
	var out := PackedByteArray()
	for i in range(0, s.length(), 2):
		out.append(("0x" + s.substr(i, 2)).hex_to_int())
	return out

static func to_hex(b: PackedByteArray) -> String:
	var s := "0x"
	for byte in b:
		s += "%02x" % byte
	return s

# ---- transactions ------------------------------------------------------------------
## The nine fields of an unsigned type-2 transaction, RLP-encoded: hash them, or append the
## signature to them.
static func _body(tx: Dictionary) -> Array:
	var to := String(tx.get("to", "")).trim_prefix("0x")
	return [
		rlp_bytes(int_bytes(int(tx.get("chain_id", 0)))),
		rlp_bytes(int_bytes(int(tx.get("nonce", 0)))),
		rlp_bytes(hex_bytes(String(tx.get("max_priority_fee", "0x0")))),
		rlp_bytes(hex_bytes(String(tx.get("max_fee", "0x0")))),
		rlp_bytes(int_bytes(int(tx.get("gas", 0)))),
		rlp_bytes(hex_bytes(to) if to != "" else PackedByteArray()),
		rlp_bytes(hex_bytes(String(tx.get("value", "0x0")))),
		rlp_bytes(hex_bytes(String(tx.get("data", "0x")))),
		PackedByteArray([0xc0]),   # an empty access list
	]

## The digest a type-2 transaction is signed over: keccak(0x02 || rlp(body)).
static func signing_digest(tx: Dictionary) -> PackedByteArray:
	var payload := PackedByteArray([0x02])
	payload.append_array(rlp_list(_body(tx)))
	return PulseBlockzCrypto.keccak256(payload)

## Signs and returns the raw transaction as 0x-hex, ready for eth_sendRawTransaction.
## Returns "" if the key is unusable.
static func sign(tx: Dictionary, secret_key_hex: String) -> String:
	var sig_hex: String = PulseBlockzCrypto.sign_digest(signing_digest(tx), secret_key_hex)
	if sig_hex == "":
		push_error("Tx: signing failed (is the key 32 bytes of hex?)")
		return ""
	var sig := hex_bytes(sig_hex)
	# hex_bytes drops leading zeros, so r, s and v are cut from sign_digest's fixed 65-byte hex.
	var raw := sig_hex.trim_prefix("0x")
	var r := _fixed(raw.substr(0, 64))
	var s := _fixed(raw.substr(64, 64))
	var y := ("0x" + raw.substr(128, 2)).hex_to_int()
	var items := _body(tx)
	items.append(rlp_bytes(int_bytes(y)))
	items.append(rlp_bytes(r))
	items.append(rlp_bytes(s))
	var out := PackedByteArray([0x02])
	out.append_array(rlp_list(items))
	return to_hex(out)

## r and s are 32-byte quantities, RLP-encoded minimally (leading zeros dropped).
static func _fixed(hex64: String) -> PackedByteArray:
	return hex_bytes(hex64)

# ---- ABI ----------------------------------------------------------------------------
static func selector(signature: String) -> String:
	return PulseBlockzCrypto.keccak256_hex(signature).substr(0, 10)

static func arg_addr(a: String) -> String:
	return "000000000000000000000000" + a.trim_prefix("0x").to_lower()

static func arg_uint(n: int) -> String:
	return "%064x" % n

## A uint256 that will not fit in a 64-bit int, given as a decimal string.
static func arg_uint_dec(dec: String) -> String:
	var hex := ""
	var digits: Array[int] = []
	for ch in dec:
		var d := "0123456789".find(ch)
		if d >= 0:
			digits.append(d)
	# Repeated division by 16, most-significant digit first.
	while not digits.is_empty():
		var rem := 0
		var next: Array[int] = []
		for d in digits:
			var cur := rem * 10 + d
			if not next.is_empty() or cur / 16 > 0:
				next.append(cur / 16)
			rem = cur % 16
		hex = "0123456789abcdef"[rem] + hex
		digits = next
	if hex == "":
		hex = "0"
	return hex.lpad(64, "0")
