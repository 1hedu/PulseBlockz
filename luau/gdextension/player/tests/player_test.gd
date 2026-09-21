# The Player end to end, with no network: a small place published into ChainAssets' cache the
# way publish-experience.js would, then played, left, refused when its bytes do not match their
# hash, and joined on loopback.
#
#   godot --headless --path . -s res://tests/player_test.gd
extends SceneTree

const STORE := "0x0f9D08e13BE2345856026615d05F7251F07efAfA"
const PROFILE := "user://player-test-profile"
const PORT := 8817
const Uses = preload("res://host/Uses.gd")

var player: Node
var passed := 0
var failed := 0
var lines: Array[String] = []

func check(ok: bool, what: String) -> void:
	print(("  PASS " if ok else "  FAIL ") + what)
	if ok: passed += 1
	else: failed += 1

func _initialize() -> void:
	print("player: open, play, leave, refuse, join")
	OS.set_environment("PBLOCKZ_RPC_URL", "http://127.0.0.1:9")   # nothing uncached gets in
	OS.set_environment("PBLOCKZ_PLAYER_KEY", "")
	_wipe(ProjectSettings.globalize_path(PROFILE))
	_run.call_deferred()

func _wipe(dir: String) -> void:
	var d := DirAccess.open(dir)
	if d == null:
		return
	for f in d.get_files():
		d.remove(f)
	for sub in d.get_directories():
		_wipe(dir.path_join(sub))
		d.remove(sub)

# ---- publishing into the cache, the way publish-experience.js does -------------------------------
var _blob := 9000

func _store(bytes: PackedByteArray, mime: String) -> String:
	var hash: String = PulseBlockzChain.content_hash_of(bytes)
	var assets := root.get_node("ChainAssets")
	DirAccess.make_dir_recursive_absolute(assets.cache_dir)
	var f := FileAccess.open(assets.cache_dir.path_join(hash.trim_prefix("0x")), FileAccess.WRITE)
	f.store_buffer(bytes)
	f.close()
	_blob += 1
	return PulseBlockzChain.format_asset_uri({"content_hash": hash, "has_chain": true, "chain_id": 943,
		"store": STORE, "blob_id": _blob, "mime": mime})

func _publish(name: String, files: Dictionary, uses: Array, server: String = "", contracts: Dictionary = {}) -> String:
	var entries := []
	var paths := files.keys()
	paths.sort()
	for p in paths:
		var bytes: PackedByteArray = String(files[p]).to_utf8_buffer()
		entries.append({"path": p, "uri": _store(bytes, "text/x-lua"), "bytes": bytes.size()})
	var manifest := {"kind": "experience", "name": name, "description": "", "thumbnail": "", "splash": "",
		"uses": uses, "published": 1789000000, "publisher": "0x0000000000000000000000000000000000000001",
		"files": entries}
	if server != "":
		manifest["server"] = server
	if not contracts.is_empty():
		manifest["contracts"] = contracts
	return _store(JSON.stringify(manifest).to_utf8_buffer(), "application/json")

const SERVER_SCRIPT := """
local HttpService = game:GetService("HttpService")
local Shared = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"))
print("PLACE hello", Shared.answer)
local ok, err = pcall(function() return HttpService:GetAsync("http://example.com/") end)
print("PLACE http", ok, tostring(err))
local store = game:GetService("DataStoreService"):GetDataStore("PlayerTest")
store:SetAsync("visits", 1)
print("PLACE stored", store:GetAsync("visits"))
"""

# ---- the run ---------------------------------------------------------------------------------------
func _run() -> void:
	var good := _publish("Player Test Place", {
		"ServerScriptService/Hello.server.luau": SERVER_SCRIPT,
		"ReplicatedStorage/Shared.luau": "return { answer = 42 }",
	}, ["chain"])

	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = PROFILE
	root.add_child(player)
	await process_frame

	check(player.listing().is_empty(), "home lists nothing until something is opened: the Player ships no bookmark")

	# ---- the UI is authored at 1280 by 720 and scaled to whatever window it is drawn in
	var Player := load("res://Player.gd")
	check(Player.scale_for(Vector2(1280, 720)) == 1.0, "a window it was written for is drawn one for one")
	check(Player.scale_for(Vector2(2560, 1440)) == 2.0, "twice the window is drawn twice the size")
	check(Player.scale_for(Vector2(3840, 1440)) == 2.0, "a wide window follows the short edge, so nothing is cut off")
	check(Player.scale_for(Vector2(800, 600)) == 1.0, "and a small one is left alone rather than shrunk")
	check(Player.scale_for(Vector2(7680, 4320)) == 3.0, "with a ceiling on it")
	check(player._home_screen != null and player._ui.scale.x == Player.scale_for(Vector2(player.get_window().size)),
		"the home screen is scaled to this window: %s" % player._ui.scale)

	# ---- a place that names the server it is played on, so a link carries the address
	var with_server := _publish("Somewhere Live", {
		"ReplicatedStorage/Shared.luau": "return { answer = 7 }",
	}, ["chain"], "play.example.com:8800")
	var seen: Dictionary = await player._previewer.preview(with_server)
	check(String(seen.get("server", "")) == "play.example.com:8800",
		"a manifest's server reaches the preview: %s" % seen.get("server", "-"))
	var plain: Dictionary = await player._previewer.preview(good)
	check(String(plain.get("server", "")) == "", "and a place without one says nothing")

	# ---- and the contracts it declares it talks to on chain
	var with_contracts := _publish("Somewhere Wired", {
		"ReplicatedStorage/Shared.luau": "return { answer = 11 }",
	}, ["chain"], "", {"ChainId": 943, "Inventory": "0xF3aB3649d8abd385564952f99a2b8873E288eC0A"})
	var wired: Dictionary = await player._previewer.preview(with_contracts)
	var named: Dictionary = wired.get("contracts", {})
	check(int(named.get("ChainId", 0)) == 943, "a manifest's contracts reach the preview, with the chain they are on")
	check(String(named.get("Inventory", "")) == "0xF3aB3649d8abd385564952f99a2b8873E288eC0A",
		"each one by the name the place gives it: %s" % named.get("Inventory", "-"))
	check((plain.get("contracts", {}) as Dictionary).is_empty(), "and a place that names none has none")

	# ---- the preview, the screen where a stranger's code is accepted or refused
	# Capabilities and contracts make a long list, and all of it has to be reachable before that
	await player.open(with_contracts)
	var page: Control = player._page
	check(page != null, "the preview opens")

	# A GDScript error mid-draw does not fail a test, so count the drawn rows rather than trust
	# a quiet run: the addresses have to reach the page, not just the dictionary.
	var shown := 0
	var stack := [page]
	while not stack.is_empty():
		var node: Node = stack.pop_back()
		for kid in node.get_children():
			stack.append(kid)
			if kid is Label and String(kid.text).contains("0xF3aB") and String(kid.text).contains("eC0A"):
				shown += 1
	check(shown > 0, "and every contract it names is on the page, address and all")

	var scroll: ScrollContainer = null
	for c in (page.get_children() if page != null else []):
		if c is ScrollContainer:
			scroll = c
	check(scroll != null, "and it scrolls rather than running off the bottom")
	if scroll != null:
		# A ScrollContainer whose content fits reports max_value == its own height, so shrink it
		# to 120 first: only then does the bar answer whether there is somewhere to scroll to.
		scroll.custom_minimum_size = Vector2(0, 120)
		scroll.size = Vector2(scroll.size.x, 120)
		await process_frame
		await process_frame
		var bar := scroll.get_v_scroll_bar()
		check(bar.max_value > 120.0,
			"with more page than window there is somewhere to scroll to: %d past %d" % [int(bar.max_value), 120])
	player._close_page()
	await process_frame

	# ---- play
	# Connected as the session appears; the place prints in its first frames
	player.session_started.connect(func(_mode):
		player.session.get_node("World").script_print.connect(func(_n, s): lines.append(String(s))))
	var got: Dictionary = await player.play(good)
	check(got.get("ok", false), "play mounts the place off its hashes: %s" % got)
	check(player.session != null and player.session_mode == "solo", "and a solo session is running")
	var world: PulseBlockzWorld = player.session.get_node("World") if player.session else null
	await _until(func(): return _said("PLACE stored") != "", 10.0)
	check(_said("PLACE hello") == "PLACE hello\t42", "its server script runs and requires its module: %s" % _said("PLACE hello"))
	check(_said("PLACE http").begins_with("PLACE http\tfalse") and _said("PLACE http").contains("not enabled"),
		"it cannot reach the internet from this machine: %s" % _said("PLACE http"))
	var want_dir: String = PROFILE.path_join("places").path_join(String(got.get("hash", "")).substr(0, 16))
	check(world != null and world.data_store_path == want_dir.path_join("datastores.json"),
		"its DataStores are kept in a folder named for its hash: %s" % (world.data_store_path if world else "-"))
	check(Uses.permits("chain") and not Uses.permits("transact") and not Uses.permits("sign"),
		"it is held to what it declared: reads, no transactions, no signatures")
	var sync: Node = player.session.get_node("ScriptSync") if player.session else null
	check(sync != null and not sync.hot_reload and not sync.load_from_disk and not sync.is_processing(),
		"no hot reload and nothing off disk: only the hashed files run")
	var wallet: Node = player.session.get_node("Wallet") if player.session else null
	# The repo's creator key is the one the Player must never reach for; a key the player keeps
	# for itself is allowed, and with no key of its own the session signs nothing.
	var own_key := FileAccess.file_exists("user://player.key") or OS.get_environment("PBLOCKZ_PLAYER_KEY") != ""
	check(wallet != null and not wallet.dev_key_from_repo and (own_key or not wallet.can_buy()),
		"no creator key from the repo; only a key this machine keeps for itself" + (", and it has one" if own_key else ", and it has none"))

	# ---- leave
	player.end_session("")
	await process_frame
	await process_frame
	check(player.session == null and player._ui.visible, "leaving brings home back")
	check(not Uses.permits("chain"), "and between sessions nothing is allowed")
	var recent: Array = player.listing()
	check(not recent.is_empty() and recent[0].name == "Player Test Place" and recent[0].recent, "it is remembered as recently played")

	# ---- and can be taken off again
	check(player.forget(good), "a remembered one can be forgotten")
	var after: Array = player.listing()
	check(not after.any(func(e): return String(e.uri) == good), "and it is off the list")
	check(not player.forget(good), "forgetting it twice changes nothing")
	check(after.is_empty(), "nothing else was ever on it: %d left" % after.size())

	# ---- a file that is not what its hash says
	var bad := _publish("Tampered Place", {"ServerScriptService/Bad.server.luau": "print('PLACE tampered ran')"}, [])
	var manifest: Dictionary = JSON.parse_string((await root.get_node("ChainAssets").fetch(bad)).get_string_from_utf8())
	var file_hash := String(PulseBlockzChain.parse_asset_uri(manifest.files[0].uri).content_hash).trim_prefix("0x")
	var cache: String = root.get_node("ChainAssets").cache_dir
	var w := FileAccess.open(cache.path_join(file_hash), FileAccess.WRITE)
	w.store_string("print('PLACE tampered ran') -- not the published bytes")
	w.close()
	var refused: Dictionary = await player.play(bad)
	await process_frame
	check(not refused.get("ok", true) and player.session == null, "a file whose bytes do not match its hash: nothing runs: %s" % refused.get("error", ""))
	check(_said("PLACE tampered ran") == "", "not a line of it")

	# ---- join a server
	var host := PulseBlockzWorld.new()
	host.name = "TestServer"
	host.mode = PulseBlockzWorld.MODE_SERVER
	host.listen_port = PORT
	host.bind_address = "127.0.0.1"
	host.auto_join = false
	host.default_camera = false
	host.default_controls = false
	# The server names the place it runs, as a published server does, so the join can be
	# remembered by it: a place reached through a server belongs on the home list too.
	host.place_uri = good
	root.add_child(host)
	await process_frame
	var joined: Dictionary = await player.join("127.0.0.1", PORT, false)
	check(joined.get("ok", false) and player.session_mode == "join", "joins a server on loopback: %s" % joined)
	# This server names no place, so the host answers ready_for_place() inside the Welcome's own
	# signal -- the same moment a cached place answers it. The join must be on record by then.
	var jw: PulseBlockzWorld = player.session.get_node("World")
	jw.run_client_chunk("joined", "task.wait(0.5) print('JOINED ' .. tostring(game:GetService('Players').LocalPlayer ~= nil))")
	await _until(func(): return _said("JOINED ") != "", 10.0)
	check(_said("JOINED ") == "JOINED true", "and becomes the player: %s" % _said("JOINED "))
	var after_join: Array = player.listing()
	var row: Dictionary = {}
	for e in after_join:
		if String(e.uri) == good: row = e
	check(not row.is_empty() and String(row.get("server", "")) == "127.0.0.1:%d" % PORT,
		"a place joined through a server goes on the list, with the address joined: %s" % row)
	check(Uses.permits("chain") and not Uses.permits("transact"), "joined read-only unless the player allowed the wallet")
	var ended := [""]
	player.session_ended.connect(func(r): ended[0] = r)
	host.queue_free()
	await _until(func(): return player.session == null, 20.0)
	check(player.session == null and ended[0].contains("closed the connection"), "the server going away ends the session and says so: %s" % ended[0])

	print("%d passed, %d failed" % [passed, failed])
	quit(1 if failed > 0 else 0)

func _said(prefix: String) -> String:
	for l in lines:
		if l.begins_with(prefix):
			return l
	return ""

func _until(cond: Callable, seconds: float) -> void:
	var start := Time.get_ticks_msec()
	while not cond.call() and Time.get_ticks_msec() - start < seconds * 1000.0:
		await process_frame
