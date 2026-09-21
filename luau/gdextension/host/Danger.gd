# Plain-language lines above the wallet prompt for the call and signature shapes that hand
# tokens over. Shapes, not intentions: a transfer meant to happen reads the same as one that
# is not, and a drain behind an innocent name is not caught. Nothing here decides anything.
extends RefCounted

## A uint256 amount of this many digits is as good as "everything", whatever the decimals.
const UNLIMITED_DIGITS := 39

## Warnings for a contract call: `fn` is the signature, `args` as given, `value` in PLS.
static func of_call(to: String, fn: String, args: Array, value: String) -> PackedStringArray:
	var out := PackedStringArray()
	if value != "" and value != "0":
		out.append("%s PLS leaves your account. It does not come back." % value)
	var name := fn.get_slice("(", 0).strip_edges()
	var a := func(i: int) -> String: return str(args[i]) if i < args.size() else "?"
	match name:
		"approve", "increaseAllowance":
			out.append("This lets %s take %s of the token at %s out of your wallet, whenever they like, until you revoke it."
				% [a.call(0), _amount(a.call(1)), to])
		"setApprovalForAll":
			if args.size() > 1 and (args[1] == true or str(args[1]).to_lower() == "true"):
				out.append("This lets %s move EVERY item you hold in %s, whenever they like, until you revoke it."
					% [a.call(0), to])
		"transfer":
			out.append("This sends %s of the token at %s to %s. It does not come back." % [_amount(a.call(1)), to, a.call(0)])
		"transferFrom", "safeTransferFrom":
			out.append("This moves tokens or items from %s to %s. It does not come back." % [a.call(0), a.call(1)])
		"safeBatchTransferFrom":
			out.append("This moves several items from %s to %s at once. They do not come back." % [a.call(0), a.call(1)])
	return out

## Warnings for EIP-712 typed data.
static func of_typed(typed: Dictionary) -> PackedStringArray:
	var out := PackedStringArray()
	var primary := String(typed.get("primaryType", ""))
	var message = typed.get("message", {})
	if typeof(message) != TYPE_DICTIONARY:
		message = {}
	var lower := primary.to_lower()
	if lower.contains("permit"):
		out.append("This is a PERMIT. Signing it lets somebody else give themselves permission to take your tokens -- they send the transaction, not you, and it costs you nothing to lose them.")
	elif message.has("spender") or message.has("operator"):
		out.append("This names %s as allowed to move your tokens or items. Signing can be enough for them to take them, with no transaction from you."
			% str(message.get("spender", message.get("operator", "?"))))
	if lower.contains("order"):
		out.append("This looks like a marketplace order. Signing it can sell or give away what you hold, at whatever price it names.")
	for key in ["value", "amount"]:
		if message.has(key) and str(message[key]).length() >= UNLIMITED_DIGITS:
			out.append("The amount is effectively unlimited.")
			break
	return out

static func _amount(raw: String) -> String:
	return "ALL (an unlimited amount)" if raw.length() >= UNLIMITED_DIGITS else "up to %s units" % raw
