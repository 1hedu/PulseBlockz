# Plays a place file the way the Studio's Play does -- scripts live, a player spawned, the
# default camera -- and saves what that camera sees at intervals, so "it looks wrong" can be
# looked at rather than reasoned about.
#
#   godot --path . -s res://tests/place_play_probe.gd -- <file.rbxl> <out dir>
#
# Must run WITHOUT --headless: the dummy renderer draws nothing.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var out_dir := "user://shots_play"
var said: Array[String] = []
var respawned := false
var shots_at := [1.0, 3.0, 6.0, 10.0]
var taken := 0
var t0 := -1.0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var path: String = args[0] if args.size() > 0 else ""
	if args.size() > 1:
		out_dir = args[1]
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(out_dir))
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.default_camera = true
	world.default_controls = true
	world.data_store_path = ""
	world.script_print.connect(func(n, l): said.append("%s|%s" % [String(n), String(l)]))
	world.script_warn.connect(func(n, l): said.append("%s|%s" % [String(n), String(l)]))
	world.script_error.connect(func(n, l): said.append("ERROR %s|%s" % [String(n), String(l)]))
	root.add_child(world)
	world.import_place(FileAccess.get_file_as_bytes(path))
	print("playing %s" % path)

func _process(delta: float) -> bool:
	t += delta
	# HexSpheres installs its StarterCharacter on start and says so; the character in play
	# then is the default one, so respawn into the installed rig, as its message asks.
	if not respawned:
		for l in said:
			if l.find("Installed articulated") != -1:
				respawned = true
				world.run_chunk("respawn", "for _, p in ipairs(game:GetService('Players'):GetPlayers()) do p:LoadCharacter() end print('RESPAWNED')")
				t0 = t
				print("installed at %.1fs; respawning" % t)
				break
		if not respawned and t > 40.0:
			print("never saw the install message; giving up")
			_dump()
			quit(1)
			return true
		return false
	if taken < shots_at.size() and t - t0 >= float(shots_at[taken]):
		var img := root.get_viewport().get_texture().get_image()
		var f := "%s/play_%02d_t%.0f.png" % [out_dir, taken, shots_at[taken]]
		img.save_png(f)
		print("SHOT %s" % ProjectSettings.globalize_path(f))
		taken += 1
	if taken >= shots_at.size():
		_dump()
		quit(0)
		return true
	return false

func _dump() -> void:
	for l in said:
		if l.begins_with("ERROR") or l.find("HexSpheres") != -1 or l.find("RESPAWN") != -1:
			print(l)
