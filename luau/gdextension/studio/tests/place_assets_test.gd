# The Studio stages a place's assets off disk, having no wallet to fetch them from the chain:
# ReplicatedStorage.PlaceAssets arrives (scripts wait on it for ever otherwise) and the props
# built from published meshes are drawn.
#
#   godot --headless --path . -s res://tests/place_assets_test.gd
extends SceneTree

var studio: Node3D
var failed := 0
var t := 0.0
const TOWN := "../demo2/scripts"

func _initialize() -> void:
	root.size = Vector2i(1280, 720)
	studio = load("res://Studio.tscn").instantiate()
	root.add_child(studio)
	_run.call_deferred()

func _process(delta: float) -> bool:
	t += delta
	if t > 180.0:
		print("place assets: TIMED OUT")
		return true
	return false

func check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok: failed += 1

## The id of a named child, or 0. Name comes off the instance, not a property lookup.
func _child(w: PulseBlockzWorld, parent: int, name: String) -> int:
	for id in w.get_child_ids(parent):
		if w.get_instance(id).name == name:
			return id
	return 0

## A named descendant at any depth, or 0: the props sit under Workspace.Map.
func _find(w: PulseBlockzWorld, parent: int, name: String) -> int:
	for id in w.get_child_ids(parent):
		if w.get_instance(id).name == name:
			return id
		var deeper := _find(w, id, name)
		if deeper != 0:
			return deeper
	return 0

## Drawn: the part has surfaces, so its pblockz:// mesh was staged and handed over.
func _drawn(w: PulseBlockzWorld, id: int) -> bool:
	if id == 0:
		return false
	var m: MeshInstance3D = w.get_part_mesh(id)
	return m != null and m.mesh != null and m.mesh.get_surface_count() > 0

## A service by class name; 0 is the DataModel.
func _service(w: PulseBlockzWorld, cls: String) -> int:
	for id in w.get_child_ids(0):
		if w.get_instance(id).class_name == cls:
			return id
	return 0

func _run() -> void:
	var here := ProjectSettings.globalize_path("res://")
	var town := here.path_join(TOWN).simplify_path()
	studio.open_place(town)
	# Playing before the open finishes would play the Studio's default place instead.
	for wait in 240:
		await process_frame
		if studio.place != null and studio.place.dir.simplify_path() == town:
			break
	check(studio.place != null and studio.place.dir.simplify_path() == town,
		"the town is the open place (got %s)" % (studio.place.dir if studio.place else "<none>"))
	# Opened is not ready: playing while the tree is still being built starts no scripts.
	for wait in 240:
		await process_frame

	# 4 ms: the default timeout freezes the played world in ten-second blocks.
	studio.script_timeout = 0.004
	studio.play()
	# A listener connected any later misses the place's first prints.
	studio.playing.script_print.connect(func(n, tx): print("  [place] ", n, ": ", tx))
	studio.playing.script_warn.connect(func(n, tx): print("  [place warn] ", n, ": ", tx))
	studio.playing.script_error.connect(func(n, tx): print("  [place ERROR] ", n, ": ", tx))
	# The props wait on their meshes, and the meshes are staged a chunk at a time.
	for wait in 1800:
		await process_frame
	var w: PulseBlockzWorld = studio.playing
	check(w != null, "the place is playing")
	if w == null:
		_done()
		return

	var rs := _service(w, "ReplicatedStorage")
	var folder := _child(w, rs, "PlaceAssets") if rs != 0 else 0
	check(folder != 0, "ReplicatedStorage.PlaceAssets exists without a wallet to make it")

	var want := ["RocketMesh", "RocketColors", "TreeMesh", "TreeColors", "HeartIcon",
		"SfxSword1", "SfxMaster", "SfxHurt", "NesPulse25",
		"SkyboxRt", "SkyboxLf", "SkyboxUp", "SkyboxDn", "SkyboxBk", "SkyboxFt"]
	var missing: Array[String] = []
	for name in want:
		if folder == 0 or _child(w, folder, name) == 0:
			missing.append(name)
	check(missing.is_empty(), "the assets are in it (missing: %s)"
		% ("none" if missing.is_empty() else ", ".join(missing)))

	var ws := _service(w, "Workspace")
	var rocket := _find(w, ws, "Rocket")
	var tree := _find(w, ws, "Tree")
	for wait in 600:
		if _drawn(w, rocket) and _drawn(w, _child(w, tree, "Trunk")) and _drawn(w, _child(w, tree, "Canopy")):
			break
		await process_frame
	check(rocket != 0 and _drawn(w, rocket), "the rocket is built: its pblockz:// mesh drawn off place-assets.json (%d)" % rocket)
	check(tree != 0 and _drawn(w, _child(w, tree, "Trunk")) and _drawn(w, _child(w, tree, "Canopy")), "the tree is built the same way, its trunk and its canopy (%d)" % tree)
	# Sky.server.luau builds all six faces or none, so one child proves the six.
	check(_child(w, _service(w, "Lighting"), "MoonEarthSky") != 0, "and the sky is up, all six faces")
	_done()

func _done() -> void:
	print("place assets: %d failed" % failed)
	quit(1 if failed > 0 else 0)
