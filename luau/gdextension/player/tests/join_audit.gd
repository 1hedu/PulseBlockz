# Audit: godot --headless --path . -s res://tests/join_audit.gd -- [host:port]
# Joins as the Player does, wallet allowed, and reports the place's warnings and errors and
# whether the splash card, the place's assets, its sounds and the chain node are in the tree.
extends SceneTree

var player: Node
var t := 0.0
var warns: Array[String] = []
var errs: Array[String] = []
var prints: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://join-audit"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_warn.connect(func(n, s): warns.append("%s: %s" % [n, s]))
		w.script_error.connect(func(n, s): errs.append("%s: %s" % [n, s]))
		w.script_print.connect(func(n, s): prints.append("%s: %s" % [n, s])))
	_run.call_deferred()

func _run() -> void:
	var where := "play.safewrap.xyz:8800"
	for a in OS.get_cmdline_user_args():
		if a.contains(":") and not a.begins_with("--"):
			where = a
	var hp: Dictionary = player._split_server(where)
	var got: Dictionary = await player.join(String(hp.host), int(hp.port), true)
	print("AUDIT join ", got)

func _service(w: PulseBlockzWorld, cls: String) -> int:
	for id in w.get_child_ids(0, true):
		if w.get_instance(id).get("class_name", "") == cls:
			return id
	for id in w.get_child_ids(0):
		if w.get_instance(id).get("class_name", "") == cls:
			return id
	return 0

func _child(w: PulseBlockzWorld, parent: int, name: String) -> int:
	if parent == 0:
		return 0
	for id in w.get_child_ids(parent):
		if w.get_instance(id).get("name", "") == name:
			return id
	return 0

func _prop(w: PulseBlockzWorld, id: int, name: String):
	for p in w.get_properties(id, true):
		if p.name == name:
			return p.value
	return null

func _process(delta: float) -> bool:
	t += delta
	if t < 100.0 or player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")

	var first := _service(w, "ReplicatedFirst")
	var splash := _child(w, first, "SplashImage")
	print("AUDIT splash: %s" % ("none" if splash == 0 else str(_prop(w, splash, "Value")).substr(0, 40)))

	var rs := _service(w, "ReplicatedStorage")
	var pa := _child(w, rs, "PlaceAssets")
	var named := 0
	var resolved := 0
	if pa != 0:
		for id in w.get_child_ids(pa):
			named += 1
			if _is_uri(String(_prop(w, id, "Value"))):
				resolved += 1
	print("AUDIT place assets: folder=%s values=%d uris=%d complete=%s" % [pa != 0, named, resolved, w.get_attributes(pa).get("Complete", "no") if pa != 0 else "-"])

	var chain := _child(w, rs, "Chain")
	var attrs: Dictionary = w.get_attributes(chain) if chain != 0 else {}
	print("AUDIT chain node: %s attributes=%s" % [chain != 0, attrs.keys()])

	var sounds := 0
	var loaded := 0
	var ids := 0
	for id in w.get_child_ids(0) + w.get_child_ids(0, true):
		pass
	for id in range(0, 0):
		pass
	var stack := [_service(w, "Workspace"), rs, _service(w, "SoundService")]
	while not stack.is_empty():
		var at: int = stack.pop_back()
		if at == 0:
			continue
		for id in w.get_child_ids(at):
			stack.append(id)
			if w.get_instance(id).get("class_name", "") == "Sound":
				sounds += 1
				if String(_prop(w, id, "SoundId")) != "":
					ids += 1
				if _prop(w, id, "IsLoaded") == true:
					loaded += 1
	print("AUDIT sounds: %d with a SoundId %d, loaded %d" % [sounds, ids, loaded])

	var failed := warns.filter(func(l): return l.to_lower().contains("could not") or l.to_lower().contains("not enabled") or l.to_lower().contains("missing"))
	print("AUDIT warnings: %d (%d look like failures)" % [warns.size(), failed.size()])
	for l in failed.slice(0, 14):
		print("   WARN ", l.substr(0, 150))
	print("AUDIT errors: %d" % errs.size())
	for l in errs.slice(0, 14):
		print("   ERR  ", l.substr(0, 150))
	quit(0)
	return true

## Whether a PlaceAssets value resolved to a chain uri.
func _is_uri(s: String) -> bool:
	return s.begins_with("pblockz://")
