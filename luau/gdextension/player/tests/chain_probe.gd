# The wallet channel, watched from outside: what the place asks on ServerStorage.Chain and what
# is written back, every few seconds, beside what the place is allowed to ask for.
#
#   godot --headless --path . -s res://tests/chain_probe.gd -- <pblockz uri | host:port>
extends SceneTree
const Uses = preload("res://host/Uses.gd")
var player: Node
var t := 0.0
var last := 0.0

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://chain-probe"
	root.add_child(player)
	_run.call_deferred()

func _run() -> void:
	var where := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://") or (a.contains(":") and not a.begins_with("--")):
			where = a
	var got: Dictionary
	if where.begins_with("pblockz://"):
		got = await player.play(where)
	else:
		var hp: Dictionary = player._split_server(where)
		got = await player.join(String(hp.host), int(hp.port), true)
	print("CHAIN started ok=%s %s" % [got.get("ok", false), got.get("error", "")])
	var says := PackedStringArray()
	for cap in ["chain", "transact", "sign", "pulsex", "scan", "market", "mirror"]:
		says.append("%s=%s" % [cap, Uses.permits(cap)])
	print("CHAIN permits ", " ".join(says))

func _find(world, path: String) -> int:
	var id := 0
	for want in path.split("/"):
		var found := 0
		for cid in world.get_child_ids(id):
			if String((world.get_instance(cid) as Dictionary).get("name", "")) == want:
				found = cid
				break
		if found == 0:
			return 0
		id = found
	return id

func _process(delta: float) -> bool:
	t += delta
	if player.session == null or t - last < 5.0:
		return false
	last = t
	var w: PulseBlockzWorld = player.session.get_node("World")
	for path in ["ServerStorage/Chain", "ReplicatedStorage/Chain"]:
		var id := _find(w, path)
		if id == 0:
			print("CHAIN t=%.0f %s: no such instance" % [t, path])
			continue
		var at: Dictionary = w.get_attributes(id)
		var keys: Array = at.keys()
		keys.sort()
		var shown := PackedStringArray()
		for k in keys:
			shown.append("%s=%s" % [k, str(at[k]).substr(0, 90)])
		print("CHAIN t=%.0f %s: %s" % [t, path, " | ".join(shown)])
	if t > 200.0:
		quit(0)
	return false
