# Whole numbers as decimal strings: a uint256 amount exceeds what a double holds exactly
# (2^53 is about 9.0e15; one PLS is 1e18 wei), so nothing here takes or returns a float.
# Pure and static -- no state, nothing to construct: `Decimal.mul(a, b)`.
class_name PulseBlockzDecimal
extends RefCounted

static func less_than(a: String, b: String) -> bool:
	var x := a.lstrip("0")
	var y := b.lstrip("0")
	if x.length() != y.length():
		return x.length() < y.length()
	return x < y

static func add(a: String, b: String) -> String:
	var x := a.lstrip("0"); var y := b.lstrip("0")
	if x == "": x = "0"
	if y == "": y = "0"
	var out := ""
	var carry := 0
	var i := x.length() - 1
	var j := y.length() - 1
	while i >= 0 or j >= 0 or carry > 0:
		var sum := carry
		if i >= 0: sum += x.unicode_at(i) - 48; i -= 1
		if j >= 0: sum += y.unicode_at(j) - 48; j -= 1
		out = String.chr(48 + (sum % 10)) + out
		carry = sum / 10
	return out if out != "" else "0"

## a - b for decimal strings, where a >= b.
static func subtract(a: String, b: String) -> String:
	var x := a.reverse()
	var y := b.reverse()
	var out := ""
	var borrow := 0
	for i in x.length():
		var d := x[i].to_int() - borrow - (y[i].to_int() if i < y.length() else 0)
		borrow = 0
		if d < 0:
			d += 10
			borrow = 1
		out += str(d)
	out = out.reverse()
	while out.length() > 1 and out.begins_with("0"):
		out = out.substr(1)
	return out

## a * m, where m is small enough to be an ordinary int.
static func mul_small(a: String, m: int) -> String:
	var x := a.lstrip("0")
	if x == "" or m == 0:
		return "0"
	var out := ""
	var carry := 0
	for i in range(x.length() - 1, -1, -1):
		var prod := (x.unicode_at(i) - 48) * m + carry
		out = String.chr(48 + (prod % 10)) + out
		carry = prod / 10
	while carry > 0:
		out = String.chr(48 + (carry % 10)) + out
		carry /= 10
	return out

static func mul(a: String, b: String) -> String:
	var x := a.lstrip("0")
	var y := b.lstrip("0")
	if x == "" or y == "":
		return "0"
	var acc := PackedInt64Array()
	acc.resize(x.length() + y.length())
	for i in range(x.length() - 1, -1, -1):
		var xi := x.unicode_at(i) - 48
		if xi == 0:
			continue
		var carry := 0
		for j in range(y.length() - 1, -1, -1):
			var at := i + j + 1
			var cur: int = acc[at] + xi * (y.unicode_at(j) - 48) + carry
			acc[at] = cur % 10
			carry = cur / 10
		var k := i
		while carry > 0:
			var cur2: int = acc[k] + carry
			acc[k] = cur2 % 10
			carry = cur2 / 10
			k -= 1
	var out := ""
	for d in acc:
		out += String.chr(48 + d)
	var trimmed := out.lstrip("0")
	return trimmed if trimmed != "" else "0"

## floor(a / b), both arbitrarily large. Long division keeps every quotient digit: a ratio of
## two thirty-digit numbers carries twenty-nine leading zeros before its first significant one.
static func div(a: String, b: String) -> String:
	var x := a.lstrip("0")
	var d := b.lstrip("0")
	if d == "":
		return "0"
	if x == "":
		return "0"
	var rem := "0"
	var quotient := ""
	for i in x.length():
		rem = add(mul_small(rem, 10), x[i])
		var q := 0
		while not less_than(rem, d):
			rem = subtract(rem, d)
			q += 1
		quotient += str(q)
	var trimmed := quotient.lstrip("0")
	return trimmed if trimmed != "" else "0"

## floor(a * b / c), without the intermediate overflowing anything.
static func mul_div(a: String, b: String, c: String) -> String:
	return div(mul(a, b), c)

## num / den in basis points, capped at 10000; taken to int only once known to fit one.
static func ratio_bps(num: String, den: String) -> int:
	var scaled := mul_div(num, "10000", den)
	if scaled.length() > 9:
		return 10000
	return mini(int(scaled), 10000)

## Hex (0x…) to a decimal string, big enough for uint256 (int64 would overflow).
static func from_hex(hex: String) -> String:
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

## "1.5" with 6 decimals -> "1500000". Returns "" if it is not a number.
static func to_units(amount: String, decimals: int) -> String:
	var t := amount.strip_edges()
	if t == "":
		return ""
	var dot := t.find(".")
	var whole := t if dot < 0 else t.substr(0, dot)
	var frac := "" if dot < 0 else t.substr(dot + 1)
	if whole == "":
		whole = "0"
	for ch in whole + frac:
		if not ch.is_valid_int():
			return ""
	while frac.length() < decimals:
		frac += "0"
	frac = frac.substr(0, decimals)
	var out := whole + frac
	while out.length() > 1 and out.begins_with("0"):
		out = out.substr(1)
	return out

## "1234500" with 6 decimals -> "1.23"
static func format(dec: String, decimals: int, places: int) -> String:
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
