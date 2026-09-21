# COPY -- do not edit. The original is luau/gdextension/host/TypedData.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# EIP-712 typed data: the digest a wallet signs for eth_signTypedData_v4, computed from the same
# named fields the wallet shows. The domain binds a signature to one contract on one chain.
# Structs, nested structs, fixed and dynamic arrays, string, bytes, bytesN, bool, address, uintN,
# intN; tests/typed_data_test.gd checks every digest against ethers' TypedDataEncoder.
extends RefCounted

const Tx = preload("res://host/Tx.gd")

## EIP-712's fixed field order for the domain, used when the caller omits the EIP712Domain type.
const DOMAIN_FIELDS := [["name", "string"], ["version", "string"], ["chainId", "uint256"],
	["verifyingContract", "address"], ["salt", "bytes32"]]

## {ok, digest: PackedByteArray, message} for {domain, types, primaryType, message}.
static func digest(data: Dictionary) -> Dictionary:
	var types = data.get("types", {})
	var domain = data.get("domain", {})
	var message = data.get("message", {})
	var primary := String(data.get("primaryType", ""))
	if typeof(types) != TYPE_DICTIONARY or typeof(domain) != TYPE_DICTIONARY or typeof(message) != TYPE_DICTIONARY:
		return {"ok": false, "message": "typed data needs domain, types and message"}
	var all: Dictionary = (types as Dictionary).duplicate()
	if not all.has("EIP712Domain"):
		var fields := []
		for f in DOMAIN_FIELDS:
			if domain.has(f[0]):
				fields.append({"name": f[0], "type": f[1]})
		all["EIP712Domain"] = fields
	if primary == "" or not all.has(primary):
		return {"ok": false, "message": "the primary type \"%s\" is not among the types" % primary}
	var err := []
	var domain_hash := hash_struct("EIP712Domain", domain, all, err)
	var message_hash := hash_struct(primary, message, all, err)
	if not err.is_empty():
		return {"ok": false, "message": String(err[0])}
	var buf := PackedByteArray([0x19, 0x01])
	buf.append_array(domain_hash)
	buf.append_array(message_hash)
	return {"ok": true, "digest": PulseBlockzCrypto.keccak256(buf)}

## "Mail(Person from,Person to,string contents)Person(string name,address wallet)"
static func encode_type(primary: String, types: Dictionary) -> String:
	var deps := {}
	_dependencies(primary, types, deps)
	deps.erase(primary)
	var names := deps.keys()
	names.sort()
	var out := _one_type(primary, types)
	for n in names:
		out += _one_type(String(n), types)
	return out

static func type_hash(primary: String, types: Dictionary) -> PackedByteArray:
	return PulseBlockzCrypto.keccak256(encode_type(primary, types).to_utf8_buffer())

static func hash_struct(primary: String, value: Dictionary, types: Dictionary, err: Array) -> PackedByteArray:
	var buf := type_hash(primary, types)
	for field in types.get(primary, []):
		var name := String(field.get("name", ""))
		var type := String(field.get("type", ""))
		if not value.has(name):
			err.append("%s is missing %s" % [primary, name])
			buf.append_array(_zero())
			continue
		buf.append_array(_encode_value(type, value[name], types, err))
	return PulseBlockzCrypto.keccak256(buf)

static func _one_type(name: String, types: Dictionary) -> String:
	var parts := PackedStringArray()
	for field in types.get(name, []):
		parts.append("%s %s" % [String(field.get("type", "")), String(field.get("name", ""))])
	return "%s(%s)" % [name, ",".join(parts)]

static func _base(type: String) -> String:
	var at := type.find("[")
	return type if at < 0 else type.substr(0, at)

static func _dependencies(name: String, types: Dictionary, found: Dictionary) -> void:
	if found.has(name) or not types.has(name):
		return
	found[name] = true
	for field in types[name]:
		_dependencies(_base(String(field.get("type", ""))), types, found)

## One field, as its 32 bytes in the struct's encoding.
static func _encode_value(type: String, v, types: Dictionary, err: Array) -> PackedByteArray:
	if type.ends_with("]"):
		var inner := type.substr(0, type.rfind("["))
		var buf := PackedByteArray()
		if typeof(v) != TYPE_ARRAY:
			err.append("%s wants a list" % type)
			return _zero()
		for item in v:
			buf.append_array(_encode_value(inner, item, types, err))
		return PulseBlockzCrypto.keccak256(buf)
	if types.has(type):
		if typeof(v) != TYPE_DICTIONARY:
			err.append("%s wants a table" % type)
			return _zero()
		return hash_struct(type, v, types, err)
	if type == "string":
		return PulseBlockzCrypto.keccak256(String(v).to_utf8_buffer())
	if type == "bytes":
		return PulseBlockzCrypto.keccak256(_hex_bytes(String(v)))
	if type == "bool":
		return _word("%064x" % (1 if v else 0))
	if type == "address":
		var a := String(v).trim_prefix("0x").to_lower()
		if a.length() != 40 or not a.is_valid_hex_number():
			err.append("\"%s\" is not an address" % str(v))
			return _zero()
		return _word(a.lpad(64, "0"))
	if type.begins_with("bytes"):
		var b := String(v).trim_prefix("0x").to_lower()
		var n := int(type.substr(5))
		if n < 1 or n > 32 or b.length() > n * 2:
			err.append("\"%s\" is not a %s" % [str(v), type])
			return _zero()
		return _word(b.rpad(64, "0"))
	if type.begins_with("uint"):
		var dec := _decimal(v)
		if dec == "" or dec.begins_with("-"):
			err.append("\"%s\" is not a %s" % [str(v), type])
			return _zero()
		return _word(Tx.arg_uint_dec(dec))
	if type.begins_with("int"):
		var dec := _decimal(v)
		if dec == "":
			err.append("\"%s\" is not a %s" % [str(v), type])
			return _zero()
		if not dec.begins_with("-"):
			return _word(Tx.arg_uint_dec(dec))
		# Two's complement sign-extended to 32 bytes; negatives are limited to 64 bits.
		var eight := PackedByteArray()
		eight.resize(8)
		eight.encode_s64(0, int(dec))
		eight.reverse()
		return _word("f".repeat(48) + eight.hex_encode())
	err.append("unsupported type " + type)
	return _zero()

## A number as a decimal string. A place's numbers arrive as floats; ones past a float arrive as
## strings and are taken as written.
static func _decimal(v) -> String:
	if typeof(v) == TYPE_STRING:
		var s := String(v).strip_edges()
		if s.begins_with("0x"):
			return str(s.hex_to_int())
		# Digit scan rather than is_valid_int, which a uint256 overflows.
		var digits := s.trim_prefix("-")
		if digits == "":
			return ""
		for ch in digits:
			if not "0123456789".contains(ch):
				return ""
		return s
	if typeof(v) == TYPE_INT:
		return str(v)
	if typeof(v) == TYPE_FLOAT and v == floor(v):
		return str(int(v))
	return ""

static func _hex_bytes(h: String) -> PackedByteArray:
	return h.trim_prefix("0x").hex_decode()

static func _word(hex64: String) -> PackedByteArray:
	return hex64.hex_decode()

static func _zero() -> PackedByteArray:
	var z := PackedByteArray()
	z.resize(32)
	return z
