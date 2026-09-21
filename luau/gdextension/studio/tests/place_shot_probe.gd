# Imports a place file, frames it from three angles and saves PNGs, so a "looks wrong" report
# can be held against a picture instead of a description.
#
# Must run WITHOUT --headless: the dummy renderer draws nothing, and a window flashes for a few
# seconds while it shoots. Not a suite -- it asserts nothing and takes a file outside the repo.
#
#   godot --path . -s res://tests/place_shot_probe.gd -- <file.rbxl> <out dir>
extends SceneTree

var world: PulseBlockzWorld
var cam: Camera3D
var t := 0.0
var shot := 0
var out_dir := "user://shots"
var said: Array[String] = []

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	var path: String = args[0] if args.size() > 0 else ""
	if args.size() > 1:
		out_dir = args[1]
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(out_dir))

	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.edit_mode = true            # the tree, with nothing of the place running
	world.auto_join = false
	world.data_store_path = ""
	world.default_camera = false
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_warn.connect(func(_n, l): said.append("WARN " + String(l)))
	root.add_child(world)

	cam = Camera3D.new()
	cam.name = "Shot"
	cam.fov = 70.0
	cam.far = 5000.0
	root.add_child(cam)
	cam.make_current()

	world.import_place(FileAccess.get_file_as_bytes(path))
	print("importing %s" % path)

## Every rendered part, so the camera can be put where the place actually is.
func _bounds() -> AABB:
	var box := AABB()
	var first := true
	var stack: Array[Node] = [world]
	while not stack.is_empty():
		var n: Node = stack.pop_back()
		for c in n.get_children():
			stack.append(c)
		if n is VisualInstance3D and n is not Camera3D:
			var v := n as VisualInstance3D
			var b: AABB = v.get_aabb()
			b.position += v.global_position
			if first:
				box = b
				first = false
			else:
				box = box.merge(b)
	return box

func _process(delta: float) -> bool:
	t += delta
	if t < 30.0:
		return false
	if shot == 0:
		shot = 1
		_shoot.call_deferred()
	return false

## Shots taken in a coroutine: _process must return a bool, so it cannot await.
func _shoot() -> void:
	var box := _bounds()
	var mid := box.position + box.size * 0.5
	var r: float = max(max(box.size.x, box.size.z), 16.0)
	print("BOUNDS at %s size %s" % [str(box.position.round()), str(box.size.round())])
	var angles := [Vector3(0.45, 0.35, 1), Vector3(-0.8, 0.25, -0.5), Vector3(1, 0.12, 0.2)]
	for i in angles.size():
		var dir: Vector3 = (angles[i] as Vector3).normalized()
		cam.global_position = mid + dir * (r * 0.42) + Vector3(0, r * 0.06, 0)
		cam.look_at(mid, Vector3.UP)
		for _f in 4:
			await process_frame
		var img := root.get_viewport().get_texture().get_image()
		var f := "%s/shot%d.png" % [out_dir, i]
		img.save_png(f)
		print("SHOT %d -> %s" % [i, ProjectSettings.globalize_path(f)])
	for l in said:
		print(l)
	quit(0)
