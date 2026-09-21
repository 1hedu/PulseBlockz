# The Publisher against an AssetStore it deploys on a throwaway chain: Check, Publish, read back
# file by file as a Player does, Check again, the listing. Signs with anvil's first account, a
# public key that holds nothing anywhere real. Needs anvil (ANVIL, or C:/tools/foundry/anvil.exe)
# and artifacts/AssetStore.json from `node scripts/compile.js`.
#
#   godot --headless --path . -s res://tests/publisher_test.gd
extends SceneTree

const ANVIL_PORT := 8547
const ANVIL_KEY := "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
const ANVIL_ACCOUNT := "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266"
const ROOT := "user://publisher-test"

var passed := 0
var failed := 0
var anvil_pid := 0
var rpc := "http://127.0.0.1:%d" % ANVIL_PORT

func check(ok: bool, what: String) -> void:
	print(("  PASS " if ok else "  FAIL ") + what)
	if ok: passed += 1
	else: failed += 1

func _initialize() -> void:
	print("publisher: check, publish, read back, keep")
	OS.set_environment("PBLOCKZ_RPC_URL", rpc)
	OS.set_environment("PBLOCKZ_PLAYER_KEY", ANVIL_KEY)
	_run.call_deferred()

func _finish() -> void:
	if anvil_pid > 0:
		OS.kill(anvil_pid)
	print("%d passed, %d failed" % [passed, failed])
	quit(1 if failed > 0 else 0)

func _rpc(method: String, params: Array):
	var http := HTTPRequest.new()
	root.add_child(http)
	var body := JSON.stringify({"jsonrpc": "2.0", "id": 1, "method": method, "params": params})
	if http.request(rpc, ["Content-Type: application/json"], HTTPClient.METHOD_POST, body) != OK:
		http.queue_free()
		return null
	var res: Array = await http.request_completed
	http.queue_free()
	if res[0] != HTTPRequest.RESULT_SUCCESS:
		return null
	var parsed = JSON.parse_string((res[3] as PackedByteArray).get_string_from_utf8())
	return parsed.get("result") if typeof(parsed) == TYPE_DICTIONARY else null

func _run() -> void:
	# ---- a chain
	var anvil := OS.get_environment("ANVIL")
	if anvil == "":
		anvil = "C:/tools/foundry/anvil.exe"
	var artifact := ProjectSettings.globalize_path("res://").path_join("../../../artifacts/AssetStore.json").simplify_path()
	if not FileAccess.file_exists(anvil) or not FileAccess.file_exists(artifact):
		check(false, "needs anvil (%s) and artifacts/AssetStore.json (node scripts/compile.js)" % anvil)
		_finish()
		return
	# 943 is PulseChain testnet's id, so the listing lands where a real Player reads it: experiences.943.json
	anvil_pid = OS.create_process(anvil, ["--port", str(ANVIL_PORT), "--chain-id", "943", "--silent"])
	var up = null
	for i in 60:
		up = await _rpc("eth_chainId", [])
		if up != null:
			break
		await create_timer(0.25).timeout
	check(up == "0x3af", "anvil is up as chain 943: %s" % up)
	if up == null:
		_finish()
		return
	var bytecode := String(JSON.parse_string(FileAccess.get_file_as_string(artifact)).bytecode)
	var deploy = await _rpc("eth_sendTransaction", [{"from": ANVIL_ACCOUNT, "data": bytecode, "gas": "0x1c9c380"}])
	var receipt = await _rpc("eth_getTransactionReceipt", [deploy])
	var store := String(receipt.get("contractAddress", "")) if typeof(receipt) == TYPE_DICTIONARY else ""
	check(store.begins_with("0x"), "AssetStore deployed at %s" % store)
	if store == "":
		_finish()
		return

	# ---- a place
	var dir := ProjectSettings.globalize_path(ROOT.path_join("place"))
	DirAccess.make_dir_recursive_absolute(dir.path_join("src/server"))
	DirAccess.make_dir_recursive_absolute(dir.path_join("src/shared"))
	_write(dir.path_join("default.project.json"), JSON.stringify({"name": "Publisher Test", "tree": {"$className": "DataModel",
		"ServerScriptService": {"$path": "src/server"}, "ReplicatedStorage": {"$path": "src/shared"}}}, "  "))
	_write(dir.path_join("src/server/Main.server.luau"), "print(\"hello from the published place\")\n")
	# 1100 repeats is past one transaction's worth: two chunks in one storeMany, one in the next.
	var big := "-- padding to make this file need more than one transaction\n".repeat(1100) + "return {}\n"
	_write(dir.path_join("src/shared/Big.luau"), big)
	var thumb := Image.create(32, 18, false, Image.FORMAT_RGB8)
	thumb.fill(Color(0.9, 0.3, 0.6))
	thumb.save_png(dir.path_join("thumbnail.png"))
	var listing := ROOT.path_join("experiences.json")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(listing))

	var app: Node = load("res://Publisher.tscn").instantiate()
	app.show_title = false
	app.listing_override = listing
	root.add_child(app)
	await process_frame
	app.publisher.store = store
	check(app.wallet.can_buy() and app.wallet.wallet_address == ANVIL_ACCOUNT, "the wallet signs as the test account: %s" % app.wallet.wallet_address)
	app._dir.text = dir
	app._fill_from_folder()
	check(app._name.text == "Publisher Test" and app._pictures.thumbnail.text.ends_with("thumbnail.png"),
		"the folder fills in its name and thumbnail: %s, %s" % [app._name.text, app._pictures.thumbnail.text])
	(app._uses["chain"] as CheckBox).button_pressed = true

	# ---- check
	var ok: bool = await app.check()
	check(ok and app.place.to_store.size() == 3 and app.place.kept.is_empty(), "Check: three things to store, nothing kept: %s" % [app.place.get("to_store", app._summary.text)])
	check(int(app.place.get("transactions", 0)) == 6, "and six transactions: one file, three for the big one, the thumbnail, the manifest: %s" % app.place.get("transactions", 0))

	# ---- publish
	var result: Dictionary = await app.publish_now()
	check(result.get("ok", false) and _is_uri(String(result.get("uri", ""))), "Publish hands back the experience's uri: %s" % result.get("uri", result.get("error", "")))
	check((result.get("txs", []) as Array).size() == 6, "in six transactions: %d" % (result.get("txs", []) as Array).size())
	if not result.get("ok", false):
		_finish()
		return

	# ---- read back off the chain, with the cache emptied so nothing answers from disk
	var assets := root.get_node("ChainAssets")
	for f in DirAccess.get_files_at(assets.cache_dir):
		DirAccess.remove_absolute(assets.cache_dir.path_join(f))
	var experience: Node = preload("res://host/Experience.gd").new()
	experience.confirm = false
	root.add_child(experience)
	var info: Dictionary = await experience.preview(String(result.uri))
	check(info.get("ok", false) and info.name == "Publisher Test" and Array(info.uses) == ["chain"] and info.count == 2,
		"the manifest reads back off the chain: %s, uses %s, %d files" % [info.get("name", info.get("error", "")), info.get("uses", []), info.get("count", 0)])
	check(info.get("publisher", "") == ANVIL_ACCOUNT and _is_uri(String(info.get("thumbnail", ""))), "with its publisher and its thumbnail")
	var same := 0
	for f in info.get("files", []):
		var got: PackedByteArray = await assets.fetch(String(f.uri))
		var disk_path := dir.path_join("src/server/Main.server.luau") if String(f.path).ends_with("Main.server.luau") else dir.path_join("src/shared/Big.luau")
		if got == FileAccess.get_file_as_bytes(disk_path) and String(f.path) in ["ServerScriptService/Main.server.luau", "ReplicatedStorage/Big.luau"]:
			same += 1
	check(same == 2, "every file comes back byte for byte under the path a Player mounts it at, the three-transaction one included: %d of 2" % same)

	# ---- again
	ok = await app.check()
	check(ok and app.place.to_store.is_empty() and app.place.kept.size() == 3, "Check again: all three already on chain and kept: %s" % [app.place.get("kept", {}).keys()])
	var listed = JSON.parse_string(FileAccess.get_file_as_string(listing)) if FileAccess.file_exists(listing) else null
	check(typeof(listed) == TYPE_DICTIONARY and listed.has("Publisher Test") and listed["Publisher Test"].uri == result.uri, "the listing names it")
	_finish()

func _write(path: String, text: String) -> void:
	var f := FileAccess.open(path, FileAccess.WRITE)
	f.store_string(text)
	f.close()

func _is_uri(s: String) -> bool:
	return s.begins_with("pblockz://")
