# COPY -- do not edit. The original is luau/gdextension/host/ScriptSync.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Hot reload for creator scripts (client side of luau/tools/sync.js). A saved file restarts its
# Script, or refreshes the module cache for a ModuleScript; a deleted one removes the instance.
# Script output goes back up the socket, so the creator's terminal is the console. _start loads
# off disk before it connects, so a place plays with no sync server running.
#
# scripts_dir is a Rojo project: a file's path under the DataModel decides its class
# (*.server.luau -> Script, *.client.luau -> LocalScript, bare *.luau -> ModuleScript,
# intermediate directories -> Folder), and a default.project.json `tree` maps directories onto
# services. Without one, scripts_dir itself is the DataModel.
#
# In the shared host layer because every app that runs a place mounts this one node: the town
# off its own files, the Player off the chain -- which is what the exports below select between.
#
# No class_name: a project run with -s has no class cache, and two projects declaring it collide.
extends Node

@export var world_path: NodePath = ^"../World"
@export var url := "ws://127.0.0.1:8790"
@export var scripts_dir := "res://scripts"
## False when the place arrives by content hash and is mounted through load_source instead.
@export var load_from_disk := true
## Off in the Player: anything listening on that port could swap the scripts after their
## hashes were checked.
@export var hot_reload := true
@export var reconnect_seconds := 2.0
## Resolves a pblockz:// hash to a file on disk. See load_source.
@export var wallet_path: NodePath = ^"../Wallet"

signal script_loaded(name: String, reloaded: bool)
signal script_unloaded(name: String)

var _world: PulseBlockzWorld
var _sources := {}      # relative path -> loaded source
var _ws := WebSocketPeer.new()
var _retry_at := 0.0
var _was_open := false
var _kills := {}        # script -> budget kills reported, reset on reload
var _wallet            # resolves content hashes, or null

func _ready():
	_world = get_node(world_path)
	_wallet = get_node_or_null(wallet_path)
	_world.script_print.connect(func(n, t): _log("print", n, t))
	_world.script_warn.connect(func(n, t): _log("warn", n, t))
	_world.script_error.connect(func(n, e): _log("error", n, e))
	_world.script_killed.connect(func(n, r): _log("killed", n, r))
	# No server sends code (rbx_net.cpp): a client runs what it already has, never what a server
	# handed it -- the published place fetched from the chain and checked file by file against
	# its hash, or, with nothing published, those same files off disk (load_place). It loads
	# once -- reloading under a live player is the hot reload SAFETY.md rules out.
	if _world.mode == PulseBlockzWorld.MODE_CLIENT:
		set_process(false)
		return
	# _ready runs child-first: defer so the scene's owner connects the world's signals first.
	_start.call_deferred()

## Loads the place's source over the tree a server sent: loadSourceFile reloads the script
## already standing at that path instead of making a second one (a client's DataModel hands out
## negative ids, so nothing it makes could pass for the server's). Call it before
## PulseBlockzWorld.ready_for_place, which copies StarterPlayerScripts into PlayerScripts.
func load_place() -> void:
	if load_from_disk:
		_load_dir(true)

func _start() -> void:
	if load_from_disk:
		_load_dir()
	if hot_reload:
		_connect()
	else:
		set_process(false)

## Puts one file of a place into the world: a script, a model, a folder's meta. It suspends only
## when there is something to fetch -- a model naming its meshes by content hash, which are
## pulled, checked and rewritten to local files first, since the engine never opens a
## "pblockz://" MeshId. The fetch is keyed by hash, so a second start comes off the disk.
func load_source(name: String, source: String) -> void:
	if _sources.get(name) == source:
		return
	var reloaded := _sources.has(name)
	# Recorded first, so a second call for the same file returns at the guard above.
	_sources[name] = source
	_kills.clear()
	_world.load_file(name, source)
	script_loaded.emit(name, reloaded)

func unload(name: String) -> void:
	if not _sources.has(name):
		return
	_sources.erase(name)
	_world.unload_file(name)
	script_unloaded.emit(name)

func names() -> Array:
	return _sources.keys()

## sources_only takes the code and skips models and meta -- see load_place().
func _load_dir(sources_only := false) -> void:
	var files := {}   # instance path -> disk path
	var project := scripts_dir.path_join("default.project.json")
	if FileAccess.file_exists(project):
		var tree = JSON.parse_string(FileAccess.get_file_as_string(project))
		if typeof(tree) != TYPE_DICTIONARY or not tree.has("tree"):
			push_error("ScriptSync: %s has no tree" % project)
			return
		_mount(tree.tree, "", files)
	else:
		for rel in _walk(scripts_dir, ""):
			files[rel] = scripts_dir.path_join(rel)
	var names := files.keys()
	names.sort()
	# One batch: no script runs until every file is in the tree, so require() needs no WaitForChild.
	for name in names:
		# A model file replaces rather than fills in: loadSourceFile destroys whatever stands
		# at that path, which on a client throws away the server's instances.
		if sources_only and not (name.ends_with(".luau") or name.ends_with(".lua")):
			continue
		if files[name].begins_with("{"):   # a project node's meta, not a file
			await load_source(name, files[name])
			continue
		if name.ends_with(".rbxm"):   # binary: the source is its base64
			await load_source(name, Marshalls.raw_to_base64(FileAccess.get_file_as_bytes(files[name])))
			continue
		var f := FileAccess.open(files[name], FileAccess.READ)
		if f:
			await load_source(name, f.get_as_text())
	if names.is_empty():
		push_warning("ScriptSync: no *.luau under %s" % scripts_dir)

# A Rojo project tree node: $path mounts a directory or a single file at the node's place in
# the DataModel, a file keeping its class suffix and taking the node's name. $className /
# $properties / $attributes become the instance's meta.json unless the walk above already found
# one on disk; $ignoreUnknownInstances is ignored.
func _mount(node, inst: String, files: Dictionary) -> void:
	if typeof(node) != TYPE_DICTIONARY:
		return
	var is_file := false
	if node.has("$path") and typeof(node["$path"]) == TYPE_STRING:
		var disk: String = scripts_dir.path_join(node["$path"])
		if FileAccess.file_exists(disk):
			files[inst + _suffix(disk)] = disk
			is_file = true
		else:
			for rel in _walk(disk, ""):
				files[inst.path_join(rel) if inst != "" else rel] = disk.path_join(rel)
	if inst != "":
		var meta := node_meta(node)
		if meta != "":
			var name := inst + ".meta.json" if is_file else inst.path_join("init.meta.json")
			if not files.has(name):
				files[name] = meta
	for k in node.keys():
		if not str(k).begins_with("$"):
			_mount(node[k], inst.path_join(k) if inst != "" else k, files)

static func node_meta(node: Dictionary) -> String:
	var meta := {}
	if node.has("$className"): meta["className"] = node["$className"]
	if node.has("$properties"): meta["properties"] = node["$properties"]
	if node.has("$attributes"): meta["attributes"] = node["$attributes"]
	return JSON.stringify(meta) if not meta.is_empty() else ""

static func _suffix(file: String) -> String:
	for s in [".server.luau", ".server.lua", ".client.luau", ".client.lua", ".luau", ".lua", ".model.json", ".meta.json", ".rbxmx", ".rbxm"]:
		if file.ends_with(s):
			return s
	return ""

func _walk(base: String, rel: String) -> Array:
	var out := []
	var d := DirAccess.open(base.path_join(rel) if rel != "" else base)
	if d == null:
		return out
	d.list_dir_begin()
	var n := d.get_next()
	while n != "":
		var r := rel.path_join(n) if rel != "" else n
		if d.current_is_dir():
			if not n.begins_with("."):
				out.append_array(_walk(base, r))
		elif n.ends_with(".luau") or n.ends_with(".lua") or n.ends_with(".model.json") or n.ends_with(".meta.json") or n.ends_with(".rbxmx") or n.ends_with(".rbxm"):
			out.append(r)
		n = d.get_next()
	return out

# ---- sync connection --------------------------------------------------------------
func _connect() -> void:
	_ws = WebSocketPeer.new()
	_ws.inbound_buffer_size = 4 << 20   # a script bigger than the 64 KB default would be dropped
	_ws.outbound_buffer_size = 1 << 20
	var err := _ws.connect_to_url(url)
	if err != OK:
		_retry_at = Time.get_ticks_msec() / 1000.0 + reconnect_seconds

func _process(_delta):
	_ws.poll()
	match _ws.get_ready_state():
		WebSocketPeer.STATE_OPEN:
			if not _was_open:
				_was_open = true
				print("ScriptSync: connected to %s" % url)
			while _ws.get_available_packet_count() > 0:
				var pkt := _ws.get_packet()
				if _ws.was_string_packet():
					_on_message(pkt.get_string_from_utf8())
		WebSocketPeer.STATE_CLOSED:
			if _was_open:
				_was_open = false
				print("ScriptSync: disconnected; keeping loaded scripts, retrying %s" % url)
			var now := Time.get_ticks_msec() / 1000.0
			if now >= _retry_at:
				_retry_at = now + reconnect_seconds
				_connect()

func _on_message(text: String) -> void:
	var msg = JSON.parse_string(text)
	if typeof(msg) != TYPE_DICTIONARY or not msg.has("op"):
		return
	match msg.op:
		"load":
			if msg.has("name") and msg.has("source"):
				await load_source(str(msg.name), str(msg.source))
		"unload":
			if msg.has("name"):
				unload(str(msg.name))

func _log(level: String, name: String, text: String) -> void:
	if _ws.get_ready_state() != WebSocketPeer.STATE_OPEN:
		return
	# A runaway script is killed every frame: only the first report carries the reason, the rest
	# would do nothing but flood the terminal.
	if level == "killed":
		var n: int = _kills.get(name, 0) + 1
		_kills[name] = n
		if n > 1 and n % 300 != 0:
			return
		if n > 1:
			text = "killed %d times so far (still over budget)" % n
	_ws.send_text(JSON.stringify({"op": "log", "level": level, "name": name, "text": text}))
