# COPY -- do not edit. The original is luau/gdextension/host/Luau.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Luau string literals for text the host injects into a chunk. The text is not ours -- a
# token name off the chain, a manifest field, an explorer page -- and a quote or a ]==] in it
# would close the literal and run the remainder as code in the player's client.
extends RefCounted

static var _controls: RegEx

## A double-quoted Luau string literal for `text`, control characters escaped. Use it in place
## of "%s" or [==[%s]==]; it carries its own quotes.
static func quote(text: String) -> String:
	var s := text.replace("\\", "\\\\").replace("\"", "\\\"") \
		.replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")
	if _controls == null:
		_controls = RegEx.create_from_string("[\\x00-\\x1f\\x7f]")
	if _controls.search(s) != null:
		# Three digits always: a digit following the escape is not absorbed into it.
		var out := ""
		for ch in s:
			var code := ch.unicode_at(0)
			out += ("\\%03d" % code) if (code < 32 or code == 127) else ch
		s = out
	return "\"" + s + "\""
