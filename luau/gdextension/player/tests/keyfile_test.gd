# The wallet key at rest: an Ethereum keystore, locked with a passphrase and readable anywhere.
#
#   godot --headless --path . -s res://tests/keyfile_test.gd
extends SceneTree

const Keyfile = preload("res://host/Keyfile.gd")
const KEY := "0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d"   # anvil #1, a test key
const ADDRESS := "0x70997970C51812dc3A010C7d01b50e0d17dc79C8"
const PASS := "a passphrase with spaces and a £"
const DIR := "user://keyfile-test"

# The Web3 Secret Storage definition's own pbkdf2 vector: passphrase "testpassword" opens it to
# 7a28b5ba...fe9d. Ours reads it, so what this writes is the format and not a lookalike.
const SPEC_JSON := """{
	"crypto": {
		"cipher": "aes-128-ctr",
		"cipherparams": { "iv": "6087dab2f9fdbbfaddc31a909735c1e6" },
		"ciphertext": "5318b4d5bcd28de64ee5559e671353e16f075ecae9f99c7a79a38af5f869aa46",
		"kdf": "pbkdf2",
		"kdfparams": {
			"c": 262144,
			"dklen": 32,
			"prf": "hmac-sha256",
			"salt": "ae3cd4e7013836a3df6bd7241b12db061dbe2c6785853cce422d148a624ce0bd"
		},
		"mac": "517ead924a9d0dc3124507e3393d175ce3ff7c1e96529c6c555ce9e51205e9b2"
	},
	"id": "3198bc9c-6672-5ab3-d995-4942343ae5b6",
	"version": 3
}"""
const SPEC_KEY := "0x7a28b5ba57c53603b0b07b56bba752f7784bf506fa95edc395f5cf6c7514fe9d"

var ok := 0
var bad := 0

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	print("KEYFILE %-56s %-6s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	DirAccess.make_dir_recursive_absolute(DIR)
	for stale in DirAccess.get_files_at(DIR):
		DirAccess.remove_absolute(DIR.path_join(stale))
	var path := DIR.path_join("player.key")

	# ---- the standard, read by us
	check("the spec's own keystore opens", PulseBlockzCrypto.keystore_read(SPEC_JSON, "testpassword"), SPEC_KEY)
	check("and not with the wrong words", PulseBlockzCrypto.keystore_read(SPEC_JSON, "testpassword "), "")

	# ---- written and read back
	check("written", Keyfile.write(path, KEY, PASS), true)
	var on_disk := FileAccess.get_file_as_string(path)
	check("the file is not the key", on_disk.find(KEY.substr(2)) == -1, true)
	check("it is a version 3 keystore", on_disk.find("\"version\": 3") != -1, true)
	check("it names the wallet", Keyfile.address(path), ADDRESS)
	check("and knows what it is", Keyfile.kind(path), Keyfile.KEYSTORE)
	check("the passphrase opens it", Keyfile.unlock(path, PASS), KEY)
	check("a near miss does not", Keyfile.unlock(path, PASS + "!"), "")
	check("nor an empty one", Keyfile.unlock(path, ""), "")
	check("a passphrase is required to write", Keyfile.write(path, KEY, "  "), false)

	# ---- altered on disk
	var parsed: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(path))
	var crypto: Dictionary = parsed["crypto"]
	crypto["ciphertext"] = "00" + String(crypto["ciphertext"]).substr(2)
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(JSON.stringify(parsed))
	f.close()
	check("a changed ciphertext is refused by the mac", Keyfile.unlock(path, PASS), "")

	# ---- a key from before the keystore
	var g := FileAccess.open(path, FileAccess.WRITE)
	g.store_string(KEY)
	g.close()
	check("a bare key is seen for what it is", Keyfile.kind(path), Keyfile.BARE)
	check("it still opens, with no passphrase", Keyfile.unlock(path, ""), KEY)
	check("and still names its wallet", Keyfile.address(path), ADDRESS)
	check("putting it away keeps the same key", Keyfile.write(path, Keyfile.unlock(path, ""), PASS) and Keyfile.unlock(path, PASS) == KEY, true)

	# ---- something else entirely, which is not a key however it is spelled
	var h := FileAccess.open(path, FileAccess.WRITE)
	h.store_string("PBKEY1:AQAAANCMnd8BFdERjHoAwE/Cl+sBAAAA")
	h.close()
	check("a file that is neither is not taken for a key", Keyfile.kind(path), Keyfile.NONE)
	check("and hands out nothing", Keyfile.unlock(path, PASS), "")

	# ---- nothing there
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	check("no file is no wallet", Keyfile.kind(path), Keyfile.NONE)
	check("and no key", Keyfile.unlock(path, PASS), "")

	print("KEYFILE %d passed, %d failed" % [ok, bad])
	print("keyfile: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
