# Solidity ABI encoding for uintN, address, bool, bytes32, string, bytes, bytes[] and arrays of
# the static types. The list is fixed by the three calls it serves -- AssetStore.storeAndPublish,
# UGC1155.register, ERC1155.safeTransferFrom -- and stops there; this is not a general encoder.
# Dynamic values are a head of offsets plus a tail of data; a wrong offset is accepted by the node
# and misread by the contract, so tests/abi_test.gd cross-checks every encoding against ethers.js.
extends RefCounted
class_name PulseBlockzAbi

# Preloaded, not reached by class_name: global classes are not always registered under `godot -s`.
const Tx = preload("res://host/Tx.gd")

const WORD := 32

## The 4-byte selector for a signature like "store(bytes)".
static func selector(signature: String) -> String:
	return PulseBlockzCrypto.keccak256_hex(signature).substr(0, 10)

## An address as one ABI word: twelve zero bytes, then the twenty of the address. Lowercased,
## because checksum casing is not part of the value and a mixed-case address compares unequal.
static func arg_address(a: String) -> String:
	return "000000000000000000000000" + a.trim_prefix("0x").to_lower()

## An unsigned number as one ABI word; 64-bit range only, Tx.arg_uint_dec takes the rest.
static func arg_uint(n: int) -> String:
	return "%064x" % n

## One eth_call request object, shaped for a batch. Pinned to "latest": every caller asks about
## current state, never a past block.
static func call_spec(to: String, data: String) -> Dictionary:
	return {"method": "eth_call", "params": [{"to": to, "data": data}, "latest"]}

## Encodes `values` against `types` as hex **without** a leading 0x, ready to append to a selector.
static func encode(types: Array, values: Array) -> String:
	var head := ""
	var tail := ""
	# Offsets are measured from the start of the head, which is one word per argument.
	var head_size := types.size() * WORD
	for i in types.size():
		var t := String(types[i])
		if _is_dynamic(t):
			head += _uint_word(head_size + tail.length() / 2)
			tail += _encode_dynamic(t, values[i])
		else:
			head += _encode_static(t, values[i])
	return head + tail

static func _is_dynamic(t: String) -> bool:
	return t == "string" or t == "bytes" or t.ends_with("[]")

static func _encode_static(t: String, v) -> String:
	if t == "address":
		return "000000000000000000000000" + String(v).trim_prefix("0x").to_lower()
	if t == "bool":
		return _uint_word(1 if v else 0)
	if t == "bytes32":
		# A bytes32 fills the word as it stands; the pad only restores stripped leading zeros.
		return String(v).trim_prefix("0x").to_lower().lpad(64, "0")
	if t.begins_with("uint"):
		# A string value is a decimal number too big for a 64-bit int.
		return Tx.arg_uint_dec(str(v)) if typeof(v) == TYPE_STRING else _uint_word(int(v))
	push_error("Abi: unsupported static type " + t)
	return _uint_word(0)

static func _encode_dynamic(t: String, v) -> String:
	if t == "string":
		return _bytes_body(String(v).to_utf8_buffer())
	if t == "bytes":
		# From Luau, bytes arrive as a "0x..." string: there is no byte array to pass.
		if typeof(v) == TYPE_STRING:
			return _bytes_body(String(v).trim_prefix("0x").hex_decode())
		return _bytes_body(v as PackedByteArray)
	# An array of a static type: a count word, then one word per element.
	if t.ends_with("[]") and not _is_dynamic(t.substr(0, t.length() - 2)):
		var inner := t.substr(0, t.length() - 2)
		var out := _uint_word((v as Array).size())
		for a in v:
			out += _encode_static(inner, a)
		return out
	if t == "bytes[]":
		# A count, then one offset per element measured from just after the count, then the bodies.
		var items: Array = v
		var offsets := ""
		var bodies := ""
		for item in items:
			offsets += _uint_word(items.size() * WORD + bodies.length() / 2)
			bodies += _bytes_body(item as PackedByteArray)
		return _uint_word(items.size()) + offsets + bodies
	push_error("Abi: unsupported dynamic type " + t)
	return ""

## A length word followed by the bytes, right-padded to a whole number of words.
static func _bytes_body(data: PackedByteArray) -> String:
	var hex := ""
	for b in data:
		hex += "%02x" % b
	var pad := (WORD - (data.size() % WORD)) % WORD
	return _uint_word(data.size()) + hex + "0".repeat(pad * 2)

static func _uint_word(n: int) -> String:
	return "%064x" % n

## Reads the i-th 32-byte word of a return value or a log's data, as 64 hex characters.
static func word(hex: String, i: int) -> String:
	var h := hex.trim_prefix("0x")
	return h.substr(i * WORD * 2, WORD * 2) if h.length() >= (i + 1) * WORD * 2 else ""

## The keccak topic0 for an event signature -- here always AssetStore's
## "BlobPublished(uint256,address,bytes32,uint32,string,uint256)". The parameter list has to match
## the contract character for character, or topic0 matches nothing.
static func topic(signature: String) -> String:
	return PulseBlockzCrypto.keccak256_hex(signature)

## Topics of the first log from `address` whose topic0 matches; empty when there is none.
static func find_log(receipt: Dictionary, address: String, signature: String) -> Array:
	var want := topic(signature).to_lower()
	for log in receipt.get("logs", []):
		if typeof(log) != TYPE_DICTIONARY:
			continue
		if String(log.get("address", "")).to_lower() != address.to_lower():
			continue
		var topics: Array = log.get("topics", [])
		if topics.size() > 0 and String(topics[0]).to_lower() == want:
			return topics
	return []
