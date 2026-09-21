# Hot reload for creator scripts: the client half of luau/tools/sync.js.
# A file's path under scripts_dir is its instance path -- first segment a service, each directory
# under it a Folder -- and its suffix the class: .server.luau Script, .client.luau LocalScript,
# bare .luau ModuleScript. A default.project.json in scripts_dir, if present, gives the mapping.
class_name ScriptSync
extends Node

@export var world_path: NodePath = ^"../World"
@export var url := "ws://127.0.0.1:8790"
@export var scripts_dir := "res://scripts"
## False when the place arrives by content hash and is mounted through load_source instead.
## The sync connection is made either way.
@export var load_from_disk := true
@export var reconnect_seconds := 2.0
## Node that turns a pblockz:// content hash into a file on disk.
@export var wallet_path: NodePath = ^"../Wallet"

signal script_loaded(name: String, reloaded: bool)
signal script_unloaded(name: String)

var _world: PulseBlockzWorld
var _sources := {}      # instance path -> source currently loaded
var _ws := WebSocketPeer.new()
var _retry_at := 0.0
var _was_open := false
var _kills := {}        # script name -> budget kills counted, for the throttle in _log()
var _wallet            # the wallet_path node, or null

func _ready():
	_world = get_node(world_path)
	_wallet = get_node_or_null(wallet_path)
	_world.script_print.connect(func(n, t): _log("print", n, t))
	_world.script_warn.connect(func(n, t): _log("warn", n, t))
	_world.script_error.connect(func(n, e): _log("error", n, e))
	_world.script_killed.connect(func(n, r): _log("killed", n, r))
	# A client's code comes from load_place(): rbx_net.cpp sends instances and no source. Loaded
	# once and never watched -- SAFETY.md rules out swapping a place under a player mid-session.
	if _world.mode == PulseBlockzWorld.MODE_CLIENT:
		set_process(false)
		return
	# _ready runs child-first: defer so the scene owner connects to the world's signals first.
	_start.call_deferred()

## Fills in the source of a tree a server sent: loadSourceFile reloads the script already
## standing at that path instead of making a second one.
##
## Call before the local player is made (PulseBlockzWorld.ready_for_place), which is what
## copies StarterPlayerScripts into PlayerScripts.
func load_place() -> void:
	if load_from_disk:
		_load_dir(true)

func _start() -> void:
	if load_from_disk:
		_load_dir()
	_connect()

## Puts one file of a place into the world: a script, a model, a folder's meta.
## Loading over a script already there restarts it; over a ModuleScript it drops the cached
## return value, so the next require runs the new source.
func load_source(name: String, source: String) -> void:
	if _sources.get(name) == source:
		return
	var reloaded := _sources.has(name)
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

## sources_only: load the place's code and skip every other kind of file -- see load_place().
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
	# Every file is in the tree before any script starts, so require(script.Parent.Util) works
	# without WaitForChild at startup.
	for name in names:
		# Instances are the server's; source is the chain's. A model or meta file does not fill
		# in, it replaces: loadSourceFile destroys what stands at that path and parents its own
		# (at a client's negative id) there, orphaning every server reference to the old one.
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

# A Rojo project tree node: $path mounts a directory, or one file that keeps its class suffix
# and takes the node's name. $className / $properties / $attributes become the instance's
# meta.json, and an init.meta.json already on disk wins.
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
	_ws.inbound_buffer_size = 4 << 20   # the 64 KB default drops any larger script
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
	# A runaway script is killed every frame. The first report carries the kill reason; the rest
	# would only flood the terminal, so they are thinned to a running count.
	if level == "killed":
		var n: int = _kills.get(name, 0) + 1
		_kills[name] = n
		if n > 1 and n % 300 != 0:
			return
		if n > 1:
			text = "killed %d times so far (still over budget)" % n
	_ws.send_text(JSON.stringify({"op": "log", "level": level, "name": name, "text": text}))
