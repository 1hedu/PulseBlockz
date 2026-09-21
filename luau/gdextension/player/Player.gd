# PulseBlockz: the Player. Title and wallet doors (host/Title.gd), a home screen of what this
# machine has opened, a preview, then a session -- a place fetched by content hash, every file
# checked, played alone, hosted, or a server joined. It holds no place of its own and no one's
# bookmark; SAFETY.md sets its rules.
extends Node

signal session_started(mode: String)
signal session_ended(reason: String)

const Luau = preload("res://host/Luau.gd")
const Uses = preload("res://host/Uses.gd")
const WalletScript = preload("res://host/Wallet.gd")
const ExperienceScript = preload("res://host/Experience.gd")
const Keyfile = preload("res://host/Keyfile.gd")
const SESSION := preload("res://Session.tscn")

## The home list is what this machine opened, and nothing else: the Player ships no one's game and
## no one's bookmark. A pasted link or a server address is the way in.
const RECENT := "recent.json"
const SETTINGS := "settings.json"
const DEFAULT_PORT := 8800
const TESTNET_RPC := "https://rpc.v4.testnet.pulsechain.com"
## What a joined server's place may use when the player has not allowed the wallet: reads only.
const JOIN_READ_ONLY := ["chain", "scan", "market"]

## Luau heap a place's scripts may hold, in MB -- the town peaks near 5; meshes and textures are
## Godot's and not counted. pulseblockz_world.cpp leaves maxSteps, maxMemory and maxFrameMillis at
## 0 under a ten second wall clock, so memory is the only budget play() and join() put back: step
## count and frame time stay uncapped. Past it Luau raises a catchable "not enough memory" in the
## script that asked, not a crash.
const PLACE_MEMORY_MB := 64


## The title card before home. Tests turn it off: nothing there presses Start.
@export var show_title := true
## Where this machine's recents, settings and each place's data live. Not keys: Title.gd and
## Wallet.gd keep those.
@export var profile_dir := "user://"

var session: Node = null
var session_mode := ""          # "solo", "host" or "join"
var session_uri := ""
var last_error := ""

var _ui: CanvasLayer
var _home_screen: Control       # stands in for the window inside _ui, so anchors mean its edges
var _home: Control
var _cards: VBoxContainer
var _status: Label
var _who: Label
var _name_edit: LineEdit
var _link: LineEdit
var _server: LineEdit
var _page: Control              # the preview or join page on top of home, or null
var _previewer: Node            # an Experience used for previews only: it never mounts anything
var _scan: Node                 # the explorer, for looking up a place's contracts before it runs

# ---- start ----------------------------------------------------------------------------------------
func _ready() -> void:
	# Home previews manifests before any session Wallet exists, so ChainAssets is pointed at the
	# chain here instead.
	var assets := get_node_or_null("/root/ChainAssets")
	if assets != null:
		var rpc := WalletScript.setting("RPC_URL")
		assets.rpc_url = rpc if rpc != "" else TESTNET_RPC
		assets.chain_id = int(WalletScript.ADDRESSES.chain_id)
		assets.asset_store = String(WalletScript.ADDRESSES.AssetStore)
	_previewer = ExperienceScript.new()
	_previewer.name = "Previewer"
	_previewer.confirm = false
	add_child(_previewer)
	# Master-bus limiter, in before any place can play: a place mixes as many voices as it likes
	# and the sum clips without one.
	add_child(preload("res://host/Audio.gd").new())
	Uses.declare([])
	_build_ui()
	var args := _args()
	if show_title and not args.has("place") and not args.has("join"):
		_ui.visible = false
		var title := preload("res://host/Title.gd").new()
		title.name = "Title"
		title.chosen.connect(func(): _ui.visible = true; _refresh_home())
		add_child(title)
	else:
		_refresh_home()
	if args.has("place"):
		open(String(args.place))
	elif args.has("join"):
		var hp := _split_server(String(args.join))
		if hp.ok:
			join_page(hp.host, hp.port)

func _args() -> Dictionary:
	var out := {}
	var a := OS.get_cmdline_user_args()
	for i in a.size():
		for flag in ["place", "join"]:
			if a[i] == "--" + flag and i + 1 < a.size():
				out[flag] = a[i + 1]
			elif a[i].begins_with("--%s=" % flag):
				out[flag] = a[i].substr(flag.length() + 3)
	return out

# ---- sessions --------------------------------------------------------------------------------------
## Plays a published experience, alone or hosting it on `host_port`. Returns {ok, error?, name?,
## hash?, place_dir?}. Call it only after the player has pressed Play on the preview.
func play(uri: String, host_port: int = 0) -> Dictionary:
	if session != null:
		return _fail("already playing something; leave it first")
	var info: Dictionary = await _previewer.preview(uri)
	if not info.get("ok", false):
		return _fail(String(info.get("error", "that experience could not be read")))
	if info.get("blocked", false):
		return _fail("a curation list you follow blocks this experience")
	var hash := String(info.hash).trim_prefix("0x")
	var place_dir := profile_dir.path_join("places").path_join(hash.substr(0, 16))
	DirAccess.make_dir_recursive_absolute(place_dir)

	var s: Node = SESSION.instantiate()
	var world: PulseBlockzWorld = s.get_node("World")
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.max_memory_mb = PLACE_MEMORY_MB
	world.http_enabled = false
	# An empty world has no ground: a body seated before the place arrives falls until it does.
	# Whatever Players.CharacterAutoLoads the place sets arrives with it and rules from then on.
	world.auto_join = false
	world.data_store_path = place_dir.path_join("datastores.json")
	world.player_name = player_name()
	if host_port > 0:
		world.listen_port = host_port
	var experience: Node = ExperienceScript.new()
	experience.name = "Experience"
	experience.confirm = false
	experience.sync_path = ^"../ScriptSync"
	experience.fetching.connect(_on_fetching)
	s.add_child(experience)
	# The manifest is fetched before any of the place's code, so the name is known here. The
	# splash is deliberately not shown: the place puts its own up the moment it starts, and a
	# card carrying the same picture makes it appear, go black, and appear again. Black, with
	# what is happening written on it, is the whole of what the player is waiting through.
	_loading(info.name, 0, int(info.count))
	_add_readers(s)
	_begin(s, "host" if host_port > 0 else "solo", uri)

	var got: Dictionary = await experience.mount(uri)
	if session != s:
		return _fail("left before it finished loading")
	if not got.get("ok", false):
		_loading("", 0, 0)
		end_session(String(got.get("error", "the experience could not be mounted")))
		return _fail(last_error)
	world.set_property(0, "PlaceVersion", int(got.get("published", 0)))
	var place_assets: Dictionary = got.get("assets", {})
	if not place_assets.is_empty():
		s.get_node("Wallet").publish_place_assets(place_assets)
	var splash := String(got.get("splash", ""))
	if splash != "":
		world.run_chunk("splash", """
local first = game:GetService("ReplicatedFirst")
local v = first:FindFirstChild("SplashImage")
if not v then
	v = Instance.new("StringValue")
	v.Name = "SplashImage"
	v.Parent = first
end
v.Value = %s
""" % Luau.quote(splash))
	_remember(uri, String(got.get("name", info.name)), String(info.get("server", "")),
		String(info.get("thumbnail", "")))
	# After the mount, so the place's own scripts decide where the player stands.
	world.add_player(player_name(), 0)
	# Time for the place's own loading screen to go up; dropping this card first shows bare world.
	await get_tree().create_timer(1.5).timeout
	if session != s:
		return _fail("left before it finished loading")
	_loading("", 0, 0)
	return {"ok": true, "name": got.get("name", ""), "hash": hash, "place_dir": place_dir}

## Joins a server. The server sends the world and what happens in it, never code: that comes off
## the chain by the hash the server names on arrival, checked file by file as `play` does.
## `allow_wallet` lets it put transactions and signatures in front of the player, each still
## shown first; off, it may only read.
func join(host: String, port: int, allow_wallet: bool = false) -> Dictionary:
	if session != null:
		return _fail("already playing something; leave it first")
	var s: Node = SESSION.instantiate()
	var world: PulseBlockzWorld = s.get_node("World")
	world.mode = PulseBlockzWorld.MODE_CLIENT
	world.max_memory_mb = PLACE_MEMORY_MB
	world.http_enabled = false
	world.server_address = host
	world.server_port = port
	world.player_name = player_name()
	# Becoming the player copies StarterPlayerScripts into PlayerScripts, so it has to wait for the
	# chain's copy to load over the instances the server sent: until then those are empty.
	world.hold_for_place = true
	_add_readers(s)
	Uses.declare(Uses.ALL.keys() if allow_wallet else JOIN_READ_ONLY)
	_begin(s, "join", "%s:%d" % [host, port])
	# A joined client has nothing of its own to draw until the server has sent the place's loading
	# screen, a quarter of a minute on a town this size.
	_loading("Joining %s" % host, 0, 0, "", JOIN_NOTE)
	var connected := [false]
	world.server_place_named.connect(func(uri): _take_the_code(s, world, String(uri), "%s:%d" % [host, port]))
	world.server_connected.connect(func(_id): connected[0] = true)
	world.server_disconnected.connect(func():
		if session == s:
			end_session("the server at %s:%d closed the connection" % [host, port]))
	var waited := 0.0
	while not connected[0] and session == s and waited < 10.0:
		await get_tree().process_frame
		waited += get_process_delta_time()
	if session != s:
		_loading("", 0, 0)
		return _fail(last_error if last_error != "" else "left before it connected")
	if not connected[0]:
		_loading("", 0, 0)
		end_session("could not reach %s:%d" % [host, port])
		return _fail(last_error)
	await _wait_for_the_place(s, world)
	if session != s:
		return _fail(last_error if last_error != "" else "left while it was arriving")
	return {"ok": true}

## The code for the place a server names, off the chain and checked, loaded over the tree the
## server sent. ready_for_place() then makes the player, which is what starts it running.
## `where` is the address joined: a place reached through a server goes on the home list with it,
## so the next run offers both Join and Play alone without the place having had to name a server.
func _take_the_code(s: Node, world: PulseBlockzWorld, uri: String, where: String = "") -> void:
	if session != s:
		return
	if uri == "":
		_say("This server is not running a published place, so none of its code will run here.")
		world.ready_for_place()
		return
	# What it is, before any of it runs. A join used to fetch and run a server's place with the
	# player never told whose it was or what it could ask for; the same page a link opens is put
	# in front of them first, and nothing of the place is fetched until they answer it.
	var known: Dictionary = await _previewer.preview(uri)
	if session != s:
		return
	_loading(String(known.get("name", "Fetching its code from the chain")), 0, 0, "", JOIN_NOTE)
	var experience: Node = ExperienceScript.new()
	experience.name = "JoinedCode"
	experience.confirm = false
	experience.sync_path = ^"../ScriptSync"
	experience.fetching.connect(_on_fetching)
	s.add_child(experience)
	var got: Dictionary = await experience.mount(uri, true)
	if session != s:
		return
	if not got.get("ok", false):
		# Not fatal: the world still arrives, with none of the code that failed the check.
		_say("The place's code did not check out (%s), so none of it is running." % String(got.get("error", "")))
	else:
		_remember(uri, String(got.get("name", "")), where, String(known.get("thumbnail", "")))
	world.ready_for_place()

## Holds the card until the place is really here -- a PlayerGui with something in it, not merely
## an open socket, since the tree arrives piecemeal. The card stays black: the place puts its own
## picture up as it starts, and a card wearing the same one makes it appear, go black, appear.
func _wait_for_the_place(s: Node, world: PulseBlockzWorld) -> void:
	var waited := 0.0
	while session == s and waited < 45.0:
		await get_tree().create_timer(0.25).timeout
		waited += 0.25
		if _wait_line != null and is_instance_valid(_wait_line):
			_wait_line.text = "waiting for the server's world"
		if _place_is_up(world):
			break
	# As in play(): the place's own loading screen goes up before this card comes down.
	await get_tree().create_timer(1.5).timeout
	if session == s:
		_loading("", 0, 0)

## Either a PlayerGui with something in it or a Character: a server running no place at all has
## the body and no screen, and its card should still come down.
func _place_is_up(world: PulseBlockzWorld) -> bool:
	var me: int = world.get_local_player_id()
	if me == 0:
		return false
	for id in world.get_child_ids(me):
		if String(world.get_instance(id).get("name", "")) == "PlayerGui" and world.get_child_ids(id).size() > 0:
			return true
	for p in world.get_properties(me, true):
		if p.name == "Character":
			return int(p.value) != 0
	return false

## ReplicatedFirst.SplashImage first -- where a loading screen's things live, and what a server
## running a published place names -- then the place's own assets.
func _server_splash(world: PulseBlockzWorld) -> String:
	var from_first := _value_of(world, "ReplicatedFirst", "SplashImage")
	if from_first != "":
		return from_first
	return _value_of(world, "ReplicatedStorage", "PlaceAssets", "Splash")

## A StringValue's Value, found by service and name. Only a pblockz:// uri is any use: a path is
## a path on the machine that wrote it, and that machine is not this one.
func _value_of(world: PulseBlockzWorld, service: String, first: String, second: String = "") -> String:
	for sid in world.get_child_ids(0):
		if String(world.get_instance(sid).get("class_name", "")) != service:
			continue
		for id in world.get_child_ids(sid):
			if String(world.get_instance(id).get("name", "")) != first:
				continue
			var at := id
			if second != "":
				at = 0
				for cid in world.get_child_ids(id):
					if String(world.get_instance(cid).get("name", "")) == second:
						at = cid
						break
				if at == 0:
					continue
			for p in world.get_properties(at, true):
				if p.name == "Value":
					var v := String(p.value)
					return v if v.begins_with("pblockz://") else ""
	return ""

## Back to the home screen. `reason` is shown there when it is not empty.
func end_session(reason: String = "") -> void:
	if session == null:
		return
	_loading("", 0, 0)
	var s := session
	session = null
	session_mode = ""
	session_uri = ""
	s.queue_free()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	Uses.declare([])
	last_error = reason
	_ui.visible = true
	_close_page()
	_refresh_home()
	_say(reason)
	session_ended.emit(reason)

func _begin(s: Node, mode: String, what: String) -> void:
	session = s
	session_mode = mode
	session_uri = what
	last_error = ""
	_ui.visible = false
	add_child(s)
	var world: PulseBlockzWorld = s.get_node("World")
	world.leave_game.connect(func():
		if session == s:
			end_session(""))
	world.script_print.connect(func(n, t): print("[%s] %s" % [n, t]))
	world.script_warn.connect(func(n, t): push_warning("[%s] %s" % [n, t]))
	world.script_error.connect(func(n, e): push_error("[%s] %s" % [n, e]))
	session_started.emit(mode)

## The explorer and the market: reads a place may ask for by name, held to its declared uses.
func _add_readers(s: Node) -> void:
	var scan := preload("res://host/Scan.gd").new()
	scan.name = "Scan"
	s.add_child(scan)
	var market := preload("res://host/Market.gd").new()
	market.name = "Market"
	s.add_child(market)


# ---- what a place talks to ---------------------------------------------------------------------
## The contracts the place names in its own code, each looked up on the explorer so the answer
## comes from somebody other than the place. The client holds no fixed list of its own.
func _contracts_section(box: VBoxContainer, declared) -> void:
	if typeof(declared) != TYPE_DICTIONARY or declared.is_empty():
		return
	var chain := int(declared.get("ChainId", 0))
	box.add_child(_section("What it talks to", ("on chain %d" % chain) if chain > 0 else "", SOLO_TINT,
		"The contracts this place names in its own code. The client checks none of them: they are what the place says it uses, and the explorer is asked what is actually at each address."))
	var names: Array = declared.keys()
	names.sort()
	for name in names:
		# str(), not String(): ChainId is a key in here too, and String(943) is not a constructor
		# GDScript has.
		var address := str(declared[name])
		if not address.begins_with("0x"):
			continue
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", 10)
		var text := VBoxContainer.new()
		text.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		text.add_child(_label(String(name), 15, INK))
		var note := _label("%s   looking it up" % _short(address), 12, MUTED)
		text.add_child(note)
		row.add_child(text)
		var site: String = _scan_node().explorer_site
		row.add_child(_button("Explorer", func(): OS.shell_open("https://%s/address/%s" % [site, address])))
		box.add_child(row)
		_look_up(address, note)

## The explorer reader for this client's own screens, made once. Not a place's channel.
func _scan_node() -> Node:
	if _scan == null or not is_instance_valid(_scan):
		_scan = preload("res://host/Scan.gd").new()
		_scan.name = "PreviewScan"
		add_child(_scan)
	return _scan

## The explorer's own answer about one address, written into the line under its name.
func _look_up(address: String, note: Label) -> void:
	var page: Dictionary = await _scan_node().scan_view("address", address)
	if not is_instance_valid(note):
		return
	if not page.get("ok", false):
		note.text = "%s   the explorer has nothing at this address" % _short(address)
		return
	var says := {}
	for r in page.get("rows", []):
		says[String(r.get("label", ""))] = String(r.get("value", ""))
	var what := ""
	if says.get("Contract", "no") == "yes":
		what = "a contract, source published" if says.get("Verified", "no") == "yes" else "a contract, source not published"
	else:
		# Not a fault: a donations address is a plain wallet on purpose.
		what = "a wallet, not a contract -- no code to read"
	var named := String(page.get("subtitle", ""))
	if named != "" and not named.begins_with("0x"):
		what = "%s, %s" % [named, what]
	var sent := String(says.get("Transactions", ""))
	note.text = "%s   %s%s" % [_short(address), what, ("   %s transactions" % sent) if sent != "" and sent != "-" else ""]

# ---- the wait --------------------------------------------------------------------------------------
var _wait: CanvasLayer
var _wait_line: Label
var _wait_card: TextureRect

## The note under the name on the wait card. JOIN_NOTE says where each half comes from: the code
## off the chain, checked file by file in join(); the world -- the tree and what happens in it --
## from the server, which is live state and is checked against nothing.
const HASH_NOTE := "every file is checked against its own hash before any of it runs"
const JOIN_NOTE := "its code comes off the chain, checked file by file; the server sends only where things are; it runs on this computer, in the sandbox"

func _on_fetching(done: int, total: int) -> void:
	if _wait_line != null and is_instance_valid(_wait_line):
		_wait_line.text = "%d of %d files" % [done, total] if done > 0 else "reading the chain"

## The card over an empty world while a place is fetched. An empty `name` takes it down.
func _loading(name: String, done: int, total: int, splash: String = "", note: String = HASH_NOTE) -> void:
	if _wait != null and is_instance_valid(_wait):
		_wait.queue_free()
	_wait = null
	_wait_line = null
	_wait_card = null
	if name == "":
		return
	var screen := _screen_layer("Wait", 90)
	_wait = screen.get_parent()
	var back := ColorRect.new()
	back.color = GROUND
	back.set_anchors_preset(Control.PRESET_FULL_RECT)
	screen.add_child(back)
	# Made even with no splash yet: a joined client learns the picture from the server later, and
	# it needs somewhere to land.
	_wait_card = TextureRect.new()
	_wait_card.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_wait_card.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_wait_card.set_anchors_preset(Control.PRESET_FULL_RECT)
	_wait_card.mouse_filter = Control.MOUSE_FILTER_IGNORE
	screen.add_child(_wait_card)
	if splash != "":
		_load_thumbnail(splash, _wait_card)
	# Across the whole width, so the lines inside are centred by the window and not by a width
	# guessed here: anchored at the middle and given a minimum size instead, the box sits where
	# that guess lands it, which is off to one side on a window it did not expect.
	var box := VBoxContainer.new()
	box.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	box.offset_left = 40
	box.offset_right = -40
	box.offset_top = -110
	box.offset_bottom = -30
	box.add_theme_constant_override("separation", 10)
	screen.add_child(box)
	var title := _label(name, 26, INK)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	box.add_child(title)
	_wait_line = _label("reading the chain", 15, MUTED)
	_wait_line.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	box.add_child(_wait_line)
	var note_label := _label(note, 13, MUTED)
	note_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	box.add_child(note_label)
	_on_fetching(done, total)

func _fail(why: String) -> Dictionary:
	last_error = why
	_say(why)
	return {"ok": false, "error": why}

# ---- what this machine remembers ------------------------------------------------------------------
func player_name() -> String:
	var n := String(_settings().get("name", "")).strip_edges()
	return n if n != "" else "Player"

func set_player_name(n: String) -> void:
	var s := _settings()
	s["name"] = n.strip_edges().substr(0, 20)
	_write_json(profile_dir.path_join(SETTINGS), s)

func _settings() -> Dictionary:
	return _read_json(profile_dir.path_join(SETTINGS), {})

## What this machine opened, newest first.
func listing() -> Array:
	var out := []
	var seen := {}
	for entry in _read_json(profile_dir.path_join(RECENT), []):
		if typeof(entry) == TYPE_DICTIONARY and String(entry.get("uri", "")) != "" and not seen.has(entry.uri):
			seen[entry.uri] = true
			out.append({"name": String(entry.get("name", "")), "uri": String(entry.uri), "recent": true,
				"played": int(entry.get("played", 0)), "server": String(entry.get("server", "")),
				"thumb": String(entry.get("thumb", ""))})
	return out

func _remember(uri: String, name: String, server: String = "", thumb: String = "") -> void:
	var recent: Array = _read_json(profile_dir.path_join(RECENT), [])
	recent = recent.filter(func(e): return typeof(e) == TYPE_DICTIONARY and e.get("uri", "") != uri)
	recent.push_front({"uri": uri, "name": name, "played": int(Time.get_unix_time_from_system()),
		"server": server, "thumb": thumb})
	_write_json(profile_dir.path_join(RECENT), recent.slice(0, 12))

## Takes one off the list. False when it was not there.
func forget(uri: String) -> bool:
	var path := profile_dir.path_join(RECENT)
	var recent: Array = _read_json(path, [])
	var left := recent.filter(func(e): return typeof(e) == TYPE_DICTIONARY and String(e.get("uri", "")) != uri)
	if left.size() == recent.size():
		return false
	_write_json(path, left)
	return true

static func _read_json(path: String, fallback):
	if not FileAccess.file_exists(path):
		return fallback
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(path))
	return parsed if typeof(parsed) == typeof(fallback) else fallback

static func _write_json(path: String, value) -> void:
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var f := FileAccess.open(path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(value, "  "))
		f.close()

## Who this machine signs as, from the same places Wallet.gd looks; no key is loaded into anything.
func wallet_line() -> String:
	var key := WalletScript.setting("PLAYER_KEY")
	if key == "":
		key = WalletScript.unlocked
	if key != "":
		var address := PulseBlockzCrypto.address_from_key(key)
		if address != "":
			return "Wallet %s" % _short(address)
	var locked := Keyfile.address("user://player.key")
	if locked != "":
		return "Wallet %s, locked" % _short(locked)
	if FileAccess.file_exists("user://watch.address"):
		return "Watching %s (reads only)" % _short(FileAccess.get_file_as_string("user://watch.address").strip_edges())
	return "No wallet: places read the chain and sign nothing"

static func _short(a: String) -> String:
	return a if a.length() < 12 else a.substr(0, 6) + "…" + a.substr(a.length() - 4)

static func _split_server(text: String) -> Dictionary:
	var t := text.strip_edges()
	var colon := t.rfind(":")
	var host := t if colon < 0 else t.substr(0, colon)
	var port := DEFAULT_PORT if colon < 0 else int(t.substr(colon + 1))
	if host.begins_with("[") and host.ends_with("]"):
		host = host.substr(1, host.length() - 2)
	if host == "" or port <= 0 or port > 65535:
		return {"ok": false}
	return {"ok": true, "host": host, "port": port}

# ---- the screens -----------------------------------------------------------------------------------
const INK := Color(0.93, 0.91, 0.96)
const MUTED := Color(0.66, 0.62, 0.72)
const ACCENT := Color(0.94, 0.42, 0.66)
const GROUND := Color(0.075, 0.063, 0.094)
const SHEET := Color(0.118, 0.102, 0.145)
const RULE := Color(0.2, 0.18, 0.24)
const WARN := Color(1.0, 0.72, 0.38)

## The window this client's own screens are written against: every size in this file is in these
## units, and a bigger window draws them bigger.
const BASE := Vector2(1280, 720)
## Never below 1, so text is never drawn smaller than written, and never past 3, where these
## screens read as a poster. A small window scrolls instead.
const SCALE_RANGE := Vector2(1.0, 3.0)

## The smaller of the two ratios, so nothing is cut off on either edge.
static func scale_for(window: Vector2) -> float:
	return clampf(minf(window.x / BASE.x, window.y / BASE.y), SCALE_RANGE.x, SCALE_RANGE.y)

## Scales one of this client's layers to the window, sizing the stand-in Control inversely so
## anchors under it still mean the window's edges. Not the project stretch setting: that would
## scale a place's own GUI too.
func _fit(layer: CanvasLayer, screen: Control) -> void:
	if layer == null or not is_instance_valid(layer) or screen == null or not is_instance_valid(screen):
		return
	var window := Vector2(get_window().size)
	var f := scale_for(window)
	layer.scale = Vector2(f, f)
	screen.size = window / f

## A layer of this client's own, returning the stand-in screen to hang things on.
func _screen_layer(name: String, order: int) -> Control:
	var layer := CanvasLayer.new()
	layer.name = name
	layer.layer = order
	add_child(layer)
	var screen := Control.new()
	screen.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(screen)
	_fit(layer, screen)
	get_window().size_changed.connect(func(): _fit(layer, screen))
	return screen

func _build_ui() -> void:
	var screen := _screen_layer("Home", 50)
	_ui = screen.get_parent()
	_home_screen = screen
	var back := ColorRect.new()
	back.color = GROUND
	back.set_anchors_preset(Control.PRESET_FULL_RECT)
	screen.add_child(back)
	var scroll := ScrollContainer.new()
	scroll.set_anchors_preset(Control.PRESET_FULL_RECT)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	screen.add_child(scroll)
	var center := CenterContainer.new()
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(center)
	var margin := MarginContainer.new()
	for side in ["left", "right"]:
		margin.add_theme_constant_override("margin_" + side, 24)
	for side in ["top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 36)
	center.add_child(margin)
	_home = VBoxContainer.new()
	_home.custom_minimum_size = Vector2(760, 0)
	_home.add_theme_constant_override("separation", 22)
	margin.add_child(_home)

	var top := HBoxContainer.new()
	top.add_theme_constant_override("separation", 16)
	_home.add_child(top)
	var brand := _label("PulseBlockz", 34, INK)
	brand.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(brand)
	var name_box := HBoxContainer.new()
	name_box.add_theme_constant_override("separation", 8)
	top.add_child(name_box)
	name_box.add_child(_label("Name", 14, MUTED))
	_name_edit = LineEdit.new()
	_name_edit.custom_minimum_size = Vector2(170, 0)
	_name_edit.max_length = 20
	_name_edit.placeholder_text = "the name others see"
	_name_edit.text_changed.connect(set_player_name)
	name_box.add_child(_name_edit)

	_who = _label("", 14, MUTED)
	_home.add_child(_who)
	_status = _label("", 15, WARN)
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.visible = false
	_home.add_child(_status)

	_home.add_child(_section("Experiences", "", SOLO_TINT,
		"Games this build knows the link to. Opening one shows what it is, who published it and what it will ask for, before any of it runs."))
	_cards = VBoxContainer.new()
	_cards.add_theme_constant_override("separation", 8)
	_home.add_child(_cards)

	_home.add_child(_section("Open a link", "", SOLO_TINT,
		"The same, for a game somebody sent you the link to."))
	var link_row := HBoxContainer.new()
	link_row.add_theme_constant_override("separation", 8)
	_home.add_child(link_row)
	_link = LineEdit.new()
	_link.placeholder_text = "pblockz://…  the link a creator shares for their game"
	_link.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_link.text_submitted.connect(func(t): if t.strip_edges() != "": open(t.strip_edges()))
	link_row.add_child(_link)
	link_row.add_child(_button("Open", func(): if _link.text.strip_edges() != "": open(_link.text.strip_edges())))

	_home.add_child(_section("Join a server", "live", LIVE_TINT,
		"An address somebody gave you. A server runs the game for everybody on it at once: one set of records, and other people in there with you."))
	var join_row := HBoxContainer.new()
	join_row.add_theme_constant_override("separation", 8)
	_home.add_child(join_row)
	_server = LineEdit.new()
	_server.placeholder_text = "host:port, for example play.safewrap.xyz:8800"
	_server.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	join_row.add_child(_server)
	join_row.add_child(_button("Join", func():
		var hp := _split_server(_server.text)
		if hp.ok:
			join_page(hp.host, hp.port)
		else:
			_say("That is not a server address. Write it as host:port.")))

func _refresh_home() -> void:
	if _home == null:
		return
	_name_edit.text = String(_settings().get("name", ""))
	_who.text = wallet_line()
	for c in _cards.get_children():
		c.queue_free()
	var entries := listing()
	if entries.is_empty():
		_cards.add_child(_label("Nothing opened yet. Paste a link below, or join a server by address.", 15, MUTED))
	for e in entries:
		_cards.add_child(_card(e))

func _card(e: Dictionary) -> Control:
	var panel := _sheet()
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 14)
	panel.get_child(0).add_child(row)
	var thumb := String(e.get("thumb", ""))
	if thumb != "":
		var shot := TextureRect.new()
		shot.custom_minimum_size = Vector2(96, 54)
		shot.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		shot.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
		shot.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		shot.clip_contents = true
		row.add_child(shot)
		_load_thumbnail(thumb, shot)
	var text := VBoxContainer.new()
	text.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(text)
	var where := String(e.get("server", ""))
	var heading := HBoxContainer.new()
	heading.add_theme_constant_override("separation", 10)
	heading.add_child(_label(e.name if e.name != "" else "(unnamed)", 19, INK))
	# The row's only chip. Whether a place runs alone or on a server is the button pressed, not a
	# property of the row -- the same link does either -- so SOLO_TINT gets no matching chip here.
	if where != "":
		heading.add_child(_chip("live at " + where, LIVE_TINT))
	text.add_child(heading)
	text.add_child(_label("you opened this %s" % Time.get_date_string_from_unix_time(int(e.played)), 13, MUTED))
	if where != "":
		var hp := _split_server(where)
		if hp.ok:
			var join_button := _button("Join", func(): join_page(String(hp.host), int(hp.port), String(e.name)))
			join_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
			row.add_child(join_button)
	var open_button := _button("Open" if where == "" else "Play alone", func(): open(String(e.uri)))
	open_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(open_button)
	var forget_button := _button("Forget", func():
		if forget(String(e.uri)):
			_say("Forgotten. Its link still opens it.")
			_refresh_home())
	forget_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	forget_button.tooltip_text = "Takes it off this list. It is not deleted from anywhere else, and the link still opens it."
	row.add_child(forget_button)
	return panel

## What a place is, drawn into `box`: its name, its picture, what it weighs, what it may ask
## of a wallet and which contracts it names. The buttons underneath are the caller's.
func _preview_body(box: VBoxContainer, info: Dictionary) -> void:
	box.add_child(_label(info.name, 26, INK))
	var thumb := TextureRect.new()
	thumb.custom_minimum_size = Vector2(0, 220)
	thumb.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	thumb.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	thumb.visible = false
	box.add_child(thumb)
	if String(info.thumbnail) != "":
		_load_thumbnail(String(info.thumbnail), thumb)

	# Only the assets the manifest names; ones fetched later, because the player owns them, are not.
	var assets_named := (info.get("assets", {}) as Dictionary).size()
	var facts := "%d scripts and models%s, %s to download. Published by %s%s." % [
		info.count,
		" and %d assets" % assets_named if assets_named > 0 else "",
		String.humanize_size(int(info.get("download_bytes", info.bytes))),
		info.publisher if info.publisher != "" else "someone who did not say",
		(" on " + Time.get_date_string_from_unix_time(int(info.published))) if int(info.published) > 0 else ""]
	box.add_child(_wrapped(facts, 15, MUTED))
	var hash_label := _label("Its hash: " + String(info.hash), 12, MUTED)
	hash_label.clip_text = true
	box.add_child(hash_label)

	var lines := PackedStringArray()
	if info.get("blocked", false):
		lines.append("A curation list you follow BLOCKS this experience. It will not run.")
	elif (info.get("curation", []) as Array).is_empty():
		lines.append("No list you follow has anything to say about this one.")
	else:
		for entry in info.curation:
			lines.append("%s: %s" % [String(entry.get("maintainer", "?")).substr(0, 10), entry.get("mode", "?")])
	if not info.seen_before:
		lines.append("You have not run this before.")
	elif info.changed:
		lines.append("CHANGED since you last ran it. The code is not what it was.")
	else:
		lines.append("Byte for byte the same code you ran last time.")
	if not Uses.touches_wallet(info.uses):
		lines.append("It cannot ask your wallet for anything.")
	else:
		lines.append("It can ask your wallet for things. Read every prompt: approving or signing the wrong one can lose you your tokens.")
	var ways := Uses.describe(info.uses)
	for one in ways:
		lines.append("   " + one)
	lines.append("Those are the only ways out. The place makes no web requests of its own, so nothing else on the internet is reachable from this computer."
		if not ways.is_empty()
		else "It asks for nothing, so it reaches nothing: no chain, no explorer, no web request of its own.")
	var verdict := _wrapped("\n".join(lines), 15, WARN if (info.changed or info.get("blocked", false)) else INK)
	box.add_child(verdict)
	_contracts_section(box, info.get("contracts", {}))

## The preview page for a link: what the experience is, before any of it runs.
func open(uri: String) -> void:
	_close_page()
	_say("")
	var page := _page_shell("Reading the manifest…")
	var info: Dictionary = await _previewer.preview(uri)
	if page != _page:
		return
	var box: VBoxContainer = page.get_meta("box")
	for c in box.get_children():
		c.queue_free()
	if not info.get("ok", false):
		box.add_child(_label("That link did not open.", 22, INK))
		box.add_child(_wrapped(String(info.get("error", "")), 15, MUTED))
		box.add_child(_row([_button("Back", _close_page)]))
		return

	_preview_body(box, info)

	var port_edit := LineEdit.new()
	port_edit.text = str(DEFAULT_PORT)
	port_edit.custom_minimum_size = Vector2(80, 0)
	var play_button := _button("Play alone", func(): play(uri))
	var host_button := _button("Host it", func(): play(uri, int(port_edit.text)))
	play_button.tooltip_text = "Runs here, in this window, with nobody else in it and records of its own."
	host_button.tooltip_text = "Runs here and opens the port below, so people you give the address to can join you."
	play_button.disabled = info.get("blocked", false)
	host_button.disabled = info.get("blocked", false)
	var buttons := [play_button, host_button, port_edit]
	var where := String(info.get("server", ""))
	if where != "":
		var hp := _split_server(where)
		if hp.ok:
			var join_button := _button("Join the server", func(): join_page(String(hp.host), int(hp.port), String(info.name)))
			join_button.tooltip_text = "Everybody who is playing is in there, on one set of records."
			buttons.push_front(join_button)
	box.add_child(_wrapped(_where_it_runs(where), 14, MUTED))
	buttons.append(_spacer())
	buttons.append(_button("Back", _close_page))
	box.add_child(_row(buttons))
	play_button.grab_focus()

## The join page: where the code comes from, and what it may ask of the wallet. `called_it` is the
## place's name when the join came off a card rather than a typed address.
func join_page(host: String, port: int, called_it: String = "") -> void:
	_close_page()
	_say("")
	var page := _page_shell("")
	var box: VBoxContainer = page.get_meta("box")
	for c in box.get_children():
		c.queue_free()
	var join_title := HBoxContainer.new()
	join_title.add_theme_constant_override("separation", 12)
	join_title.add_child(_label(called_it if called_it != "" else "Join %s:%d" % [host, port], 26, INK))
	join_title.add_child(_chip("live", LIVE_TINT))
	box.add_child(join_title)
	box.add_child(_label("%s:%d%s" % [host, port,
		", where its creator says it is played" if called_it != "" else ""], 14, MUTED))
	box.add_child(_wrapped("Everybody playing here is in the same game, on the server's records rather than yours. It decides which place you are in and sends its code, which runs on this computer, in the sandbox: it cannot read your files, and it makes no web requests of its own -- reading the chain and the explorer goes through this client, and only if you allow the wallet. What it sends has not been checked against any published hash.", 15, MUTED))
	var allow := CheckBox.new()
	allow.text = "Let it ask my wallet to send transactions and sign messages"
	allow.button_pressed = false
	box.add_child(allow)
	box.add_child(_wrapped("Every request is still shown to you before anything happens, and you can say no to each one. Left off, this place can only read the chain: no purchase, no swap and no signature, and it is told so rather than left waiting.", 13, MUTED))
	var go := _button("Join", func(): join(host, port, allow.button_pressed))
	go.tooltip_text = "Puts you in the game running at %s:%d." % [host, port]
	box.add_child(_row([go, _spacer(), _button("Back", _close_page)]))

## The line above the buttons: which of them puts you where, in the words the buttons use.
func _where_it_runs(server: String) -> String:
	if server == "":
		return "Play alone runs it here, in this window, with nobody else in it. Host it does the same and opens the port beside it, so people you give your address to can join you."
	return ("Join the server puts you in the game at %s, where everybody else is and there is one set of records. "
		+ "Play alone runs this same code here instead, with nobody in it and records of its own. What the server sends "
		+ "is not checked against the hash above: that hash is this copy of the code, not whatever the "
		+ "server is running.") % server


func _page_shell(waiting: String) -> Control:
	_home.visible = false
	var page := MarginContainer.new()
	page.set_anchors_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		page.add_theme_constant_override("margin_" + side, 40)
	var back := ColorRect.new()
	back.color = GROUND
	back.set_anchors_preset(Control.PRESET_FULL_RECT)
	_home_screen.add_child(back)
	page.set_meta("backdrop", back)
	_home_screen.add_child(page)
	# A preview's list of capabilities and contracts can be taller than the window, and all of it
	# has to be reachable before anything runs.
	var scroll := ScrollContainer.new()
	scroll.set_anchors_preset(Control.PRESET_FULL_RECT)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	page.add_child(scroll)
	var center := CenterContainer.new()
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(center)
	var sheet := _sheet()
	sheet.custom_minimum_size = Vector2(640, 0)
	center.add_child(sheet)
	var box: VBoxContainer = sheet.get_child(0)
	box.add_theme_constant_override("separation", 12)
	page.set_meta("box", box)
	if waiting != "":
		box.add_child(_label(waiting, 18, MUTED))
	_page = page
	return page

func _close_page() -> void:
	if _page != null and is_instance_valid(_page):
		var back = _page.get_meta("backdrop")
		if back != null and is_instance_valid(back):
			back.queue_free()
		_page.queue_free()
	_page = null
	if _home != null:
		_home.visible = true

func _load_thumbnail(uri: String, into: TextureRect) -> void:
	var assets := get_node_or_null("/root/ChainAssets")
	if assets == null:
		return
	var bytes: PackedByteArray = await assets.fetch(uri)
	if bytes.is_empty() or not is_instance_valid(into):
		return
	var img := Image.new()
	var err := ERR_FILE_UNRECOGNIZED
	if bytes.size() > 3 and bytes[0] == 0x89 and bytes[1] == 0x50:
		err = img.load_png_from_buffer(bytes)
	elif bytes.size() > 3 and bytes[0] == 0xFF and bytes[1] == 0xD8:
		err = img.load_jpg_from_buffer(bytes)
	elif bytes.size() > 12 and bytes.slice(8, 12).get_string_from_ascii() == "WEBP":
		err = img.load_webp_from_buffer(bytes)
	if err != OK:
		return
	into.texture = ImageTexture.create_from_image(img)
	into.visible = true

func _say(text: String) -> void:
	if _status == null:
		return
	_status.text = text
	_status.visible = text != ""

# ---- small parts -----------------------------------------------------------------------------------
func _label(text: String, size: int, color: Color) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	return l

func _wrapped(text: String, size: int, color: Color) -> Label:
	var l := _label(text, size, color)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size = Vector2(560, 0)
	return l

func _section(text: String, chip: String = "", tint: Color = SOLO_TINT, note: String = "") -> Control:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 6)
	var head := HBoxContainer.new()
	head.add_theme_constant_override("separation", 10)
	head.add_child(_label(text.to_upper(), 12, ACCENT))
	if chip != "":
		head.add_child(_chip(chip, tint))
	box.add_child(head)
	var rule := ColorRect.new()
	rule.color = RULE
	rule.custom_minimum_size = Vector2(0, 1)
	box.add_child(rule)
	if note != "":
		box.add_child(_label(note, 12, MUTED))
	return box

const SOLO_TINT := Color(0.62, 0.58, 0.70)
const LIVE_TINT := Color(0.38, 0.82, 0.52)

func _chip(text: String, tint: Color) -> Control:
	var panel := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = Color(tint.r, tint.g, tint.b, 0.14)
	style.border_color = Color(tint.r, tint.g, tint.b, 0.55)
	style.set_border_width_all(1)
	style.set_corner_radius_all(3)
	style.content_margin_left = 7
	style.content_margin_right = 7
	style.content_margin_top = 2
	style.content_margin_bottom = 2
	panel.add_theme_stylebox_override("panel", style)
	panel.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var l := _label(text.to_upper(), 11, tint)
	panel.add_child(l)
	return panel

func _sheet() -> PanelContainer:
	var panel := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = SHEET
	style.border_color = RULE
	style.set_border_width_all(1)
	style.set_corner_radius_all(4)
	style.content_margin_left = 18
	style.content_margin_right = 18
	style.content_margin_top = 14
	style.content_margin_bottom = 14
	panel.add_theme_stylebox_override("panel", style)
	var box := VBoxContainer.new()
	panel.add_child(box)
	return panel

func _button(text: String, on_press: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Vector2(96, 34)
	b.pressed.connect(on_press)
	return b

func _row(items: Array) -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 8)
	for item in items:
		row.add_child(item)
	return row

func _spacer() -> Control:
	var c := Control.new()
	c.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return c
