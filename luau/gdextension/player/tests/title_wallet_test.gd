# The title screen names the wallet this machine has with no wallet node to ask: in the Player a
# wallet belongs to a session, and no session has started while the title is up.
#   godot --headless --path . -s res://tests/title_wallet_test.gd
extends SceneTree

const Title = preload("res://host/Title.gd")
const KEY := "0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d"   # anvil #1, public
var ok := 0
var bad := 0

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	print("TITLE %-50s %-14s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	var was := OS.get_environment("PBLOCKZ_PLAYER_KEY")
	var address: String = PulseBlockzCrypto.address_from_key(KEY)

	OS.set_environment("PBLOCKZ_PLAYER_KEY", KEY)
	var title = Title.new()
	root.add_child(title)
	check("it names the wallet with no wallet node to ask", title._greeting(), "Signed in as %s." % address)

	OS.set_environment("PBLOCKZ_PLAYER_KEY", "")
	var keyless = Title.new()
	root.add_child(keyless)
	var said: String = keyless._greeting()
	var has_key_here := FileAccess.file_exists("user://player.key") or FileAccess.file_exists("user://watch.address")
	check("with nothing to sign with it is one short line",
		said == "No wallet." or has_key_here, true)

	if was == "": OS.unset_environment("PBLOCKZ_PLAYER_KEY")
	else: OS.set_environment("PBLOCKZ_PLAYER_KEY", was)
	print("TITLE %d passed, %d failed" % [ok, bad])
	print("title wallet: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
