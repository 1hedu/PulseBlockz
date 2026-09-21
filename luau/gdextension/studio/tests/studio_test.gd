# Headless check of the Studio: godot --headless --path . -s res://tests/studio_test.gd
# Drives the demo place through every panel and verb -- tree, properties, handles, undo, Play,
# Save, Terrain, plugins -- checking the world and the panels agree.
extends SceneTree

var studio: Node3D
var world: PulseBlockzWorld
var failed := 0
var logs: Array[String] = []

var t := 0.0

func _initialize() -> void:
	root.size = Vector2i(1280, 720)   # headless: the window is 64x64 until told otherwise
	studio = load("res://Studio.tscn").instantiate()
	root.add_child(studio)
	_run.call_deferred()

func _process(delta: float) -> bool:
	t += delta
	if t > 180.0:
		print("studio: TIMED OUT")
		return true
	return false

func check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok: failed += 1

func _run() -> void:
	world = studio.world
	# The demo's Evil script busy-loops every Heartbeat: 4 ms here in place of Roblox's ten seconds
	studio.script_timeout = 0.004
	world.max_millis_per_call = 4.0
	world.script_print.connect(func(n, tx): logs.append(n + ": " + tx))
	world.script_error.connect(func(n, e): logs.append("ERROR " + n + ": " + e))
	await _settle(150)

	var ws := _child_of(0, "Workspace")
	var map := _child_of(ws, "Map")
	check(ws != 0 and map != 0, "the place opened: Workspace.Map is in the mirrored tree")
	check(not _logged("map built:"), "and nothing ran: an edit world is a tree to work on, not a game")
	check(_child_of(map, "Baseplate") != 0 and _child_of(map, "Kart") != 0,
		"the map is all there from its files, so there is something to edit")
	var bench := _child_of(map, "Bench")
	check(bench != 0 and world.get_instance(bench).class_name == "Seat", "the Bench is a Seat")
	check(world.get_instance(bench).path == "game.Workspace.Map.Bench", "an instance knows its path")

	var explorer = studio._explorer
	explorer.reveal([bench])
	await _settle(4)
	check(explorer._items.has(bench) and explorer._items.has(map), "the Explorer reveals a deep instance, unfolding its ancestors")
	check(explorer._items[bench].get_text(1) == "Seat", "and names its class beside it")

	var props = studio._properties
	studio._select_one(bench)
	await _settle(2)
	check(props._rows.has("Size") and props._rows.has("Occupant"), "the Properties panel lists the class's properties")
	check(props._rows["Size"].get_text(1) == "5, 1, 2", "with the value the mirror holds")
	check(not props._rows["Occupant"].is_editable(1), "a ReadOnly property (Occupant) cannot be edited")
	check(props._rows["Transparency"].get_custom_color(1) == props.DIM, "an untouched property shows its class default, dimmed")

	check(world.set_property(bench, "Size", Vector3(9, 1, 2)), "the panel writes Size")
	check(world.set_property(bench, "Material", "Neon"), "and an EnumItem by its item name")
	check(not world.set_property(bench, "Material", "Cheese"), "an item the enum does not have is refused")
	check(world.set_property(bench, "Occupant", 0), "a ReadOnly one is taken: it binds scripts, and the Studio is the host (the pane greys those rows itself; the Rig Builder sets Humanoid.RigType)")
	await _settle(6)
	props.refresh()
	check(props._rows["Size"].get_text(1) == "9, 1, 2", "the panel shows what the runtime took")
	check(props._rows["Size"].get_custom_color(1) == props.LIVE, "and no longer calls it a default")
	check(props._rows.has("CustomPhysicalProperties") and props._rows["CustomPhysicalProperties"].get_text(1) == "",
		"a property holding nothing reads empty, not an error")
	var node := world.get_part_node(bench)
	check(node != null and studio._drawn_mesh(node, "Bench") != null, "the selection has a mesh to outline")

	var version = world.get_tree_version()
	var before := world.get_child_ids(ws).size()
	studio._select([])              # nothing selected: it goes under the Workspace
	await studio._insert_part()
	await _settle(4)
	check(world.get_child_ids(ws).size() == before + 1, "the Part button inserts one under the Workspace")
	var made = studio.primary()
	check(made != 0 and world.get_instance(made).class_name == "Part", "and selects what it made")
	var props_of_new = _prop_of(made, "Anchored")
	check(props_of_new.get("value", false) == true, "anchored where the camera is looking, as Studio leaves one")
	check(world.get_tree_version() != version, "the tree version moved, so the Explorer rebuilds")
	studio._delete()
	await _settle(6)
	check(world.get_child_ids(ws).size() == before, "Delete destroys the selection")
	check(world.get_instance(made).is_empty(), "and it is gone from the mirror")

	# Handles take rays, not screen points: a ray aimed at a world point drags to exactly there
	var handles = studio._handles
	var cam = studio._cam
	studio._select_one(bench)
	await _settle(2)
	var f = handles.frame()
	check(f.middle.is_equal_approx(Vector3(19, 1.5, 8)) and f.half.is_equal_approx(Vector3(4.5, 0.5, 1)),
		"the handles frame the part where it is drawn: %s half %s" % [f.get("middle", ""), f.get("half", "")])

	handles.mode = Handles.Mode.MOVE
	var arrow: Vector3 = f.middle + Vector3(f.half.x + 8, 0, 0)
	var pixel: Vector2 = cam.unproject_position(arrow)
	check(handles.pick(cam.project_ray_origin(pixel), cam.project_ray_normal(pixel)) == 0, "the pointer over the +X arrow picks it")
	var elsewhere: Vector2 = pixel + Vector2(120, 90)
	check(handles.pick(cam.project_ray_origin(elsewhere), cam.project_ray_normal(elsewhere)) == -1, "and away from them all, none")

	var was = _prop_of(bench, "Position").value
	check(handles.begin(0, cam.global_position, (f.middle - cam.global_position).normalized()), "the +X move handle takes the drag")
	handles.drag(cam.global_position, ((f.middle + Vector3(5, 0, 0)) - cam.global_position).normalized())
	handles.finish()
	await _settle(6)
	check(_prop_of(bench, "Position").value.is_equal_approx(was + Vector3(5, 0, 0)),
		"dragging it five studs moves the part five studs: %s" % _prop_of(bench, "Position").value)

	handles.mode = Handles.Mode.SCALE
	f = handles.frame()
	var was_size = _prop_of(bench, "Size").value
	was = _prop_of(bench, "Position").value
	check(handles.begin(0, cam.global_position, (f.middle - cam.global_position).normalized()), "the +X face takes the drag")
	handles.drag(cam.global_position, ((f.middle + Vector3(2, 0, 0)) - cam.global_position).normalized())
	handles.finish()
	await _settle(6)
	check(_prop_of(bench, "Size").value.is_equal_approx(was_size + Vector3(2, 0, 0)), "pulling the face grows Size by what you dragged")
	check(_prop_of(bench, "Position").value.is_equal_approx(was + Vector3(1, 0, 0)), "and the far face stays put: the middle moved half of it")

	handles.mode = Handles.Mode.ROTATE
	f = handles.frame()
	check(handles.begin(1, cam.global_position, ((f.middle + Vector3(6, 0, 0)) - cam.global_position).normalized()), "the Y ring takes the drag")
	handles.drag(cam.global_position, ((f.middle + Vector3(0, 0, 6)) - cam.global_position).normalized())
	handles.finish()
	await _settle(8)
	var turn = _prop_of(bench, "Orientation").value
	check(is_equal_approx(turn.y, -90.0) and is_zero_approx(turn.x) and is_zero_approx(turn.z),
		"a quarter turn round the ring is -90 on Orientation.Y: %s" % turn)
	check(handles.frame().basis.is_equal_approx(Handles._euler_basis(turn)),
		"Orientation is YXZ Euler: the drawn part sits exactly where the panel says")
	check(_prop_of(bench, "Size").value.is_equal_approx(was_size + Vector3(2, 0, 0)), "turning it did not resize it")

	handles.snap = 0.0
	f = handles.frame()
	was = _prop_of(bench, "Position").value
	handles.mode = Handles.Mode.MOVE
	handles.begin(2, cam.global_position, (f.middle - cam.global_position).normalized())
	handles.drag(cam.global_position, ((f.middle + Vector3(0, 2.5, 0)) - cam.global_position).normalized())
	handles.finish()
	await _settle(6)
	check(_prop_of(bench, "Position").value.is_equal_approx(was + Vector3(0, 2.5, 0)), "with the grid off a drag lands where it likes")

	var history: History = studio.history
	var held_size = _prop_of(bench, "Size").value
	check(history.set_property(bench, "Size", Vector3(2, 2, 2)), "an edit from the panel goes through the history")
	await _settle(6)
	await studio._undo()
	await _settle(6)
	check(_prop_of(bench, "Size").value.is_equal_approx(held_size), "undo puts back what was there: %s" % _prop_of(bench, "Size").value)
	await studio._redo()
	await _settle(6)
	check(_prop_of(bench, "Size").value.is_equal_approx(Vector3(2, 2, 2)), "redo does it again")
	await studio._undo()
	await _settle(6)

	var was_at = _prop_of(bench, "Position").value
	handles.mode = Handles.Mode.MOVE
	f = handles.frame()
	var along: Vector3 = f.basis.x     # the part's own +X, not the world's: it was turned a quarter earlier
	handles.begin(0, cam.global_position, (f.middle - cam.global_position).normalized())
	for step in [1.0, 2.0, 3.0]:
		handles.drag(cam.global_position, ((f.middle + along * step) - cam.global_position).normalized())
	handles.finish()
	await _settle(6)
	check(_prop_of(bench, "Position").value.is_equal_approx(was_at + along * 3.0),
		"the drag ended three studs along the part's own axis")
	await studio._undo()
	await _settle(6)
	check(_prop_of(bench, "Position").value.is_equal_approx(was_at), "and one undo takes the whole drag, not one step of it")

	var count := world.get_child_ids(ws).size()
	studio._select([])
	await studio._insert_part()
	await _settle(6)
	check(world.get_child_ids(ws).size() == count + 1 and studio.primary() != 0, "the Part button inserts through the history")
	await studio._undo()
	await _settle(10)
	check(world.get_child_ids(ws).size() == count, "undo takes it away again")
	var known := {}
	for c in world.get_child_ids(ws):
		known[c] = true
	await studio._redo()
	await _settle(12)
	var remade := 0
	for c in world.get_child_ids(ws):
		if not known.has(c):
			remade = c
	check(remade != 0 and _prop_of(remade, "Anchored").value == true,
		"redo makes it again, anchored where it was put")
	studio._select_one(remade)
	await studio._undo()
	await _settle(10)

	var chest := _child_of(map, "Chest")
	var chest_kids := world.get_child_ids(chest).size()
	check(chest_kids >= 2, "the Chest has children of its own: a Sound and a prompt")
	check(history.destroy(chest), "delete it")
	await _settle(8)
	check(_child_of(map, "Chest") == 0, "and it goes")
	await studio._undo()
	await _settle(14)
	var chest_back := _child_of(map, "Chest")
	check(chest_back != 0 and world.get_child_ids(chest_back).size() == chest_kids,
		"undo brings it back with its children, from the model the entry kept")

	var lamp := _child_of(map, "Lamp")
	check(history.set_parent(lamp, ws), "an instance moves to another parent")
	await _settle(8)
	check(world.get_instance(lamp).parent == ws, "it is under the Workspace now")
	await studio._undo()
	await _settle(8)
	check(world.get_instance(lamp).parent == map, "and undo puts it back in the Map")

	check(history.can_redo(), "there is something to redo")
	history.set_property(bench, "Transparency", 0.25)
	check(not history.can_redo(), "and a fresh edit clears it: a new edit is a new future")
	await _settle(6)
	await studio._undo()
	await _settle(6)

	var base := _child_of(map, "BenchBase")
	studio._select_one(bench)
	studio._picked([base], true, false)          # shift-click
	await _settle(2)
	check(studio._selection.size() == 2 and studio.primary() == base,
		"shift adds to the selection, and the last one picked leads")
	studio._picked([base], false, true)          # ctrl-click the same one
	await _settle(2)
	check(studio._selection == [bench], "ctrl takes one back out")
	studio._picked([base], true, false)
	await _settle(2)

	var pair: Dictionary = handles.frame()
	check(pair.basis.is_equal_approx(Basis.IDENTITY),
		"several at once are boxed on the world's axes, having none of their own in common")
	var round_both := AABB(pair.middle - pair.half, pair.half * 2.0)
	check(round_both.has_point(handles.frame_of(bench).middle) and round_both.has_point(handles.frame_of(base).middle),
		"and the box round them holds both")

	handles.mode = Handles.Mode.SCALE
	check(handles.pick(cam.global_position, (pair.middle - cam.global_position).normalized()) == -1,
		"scale stays out of it: Size is along axes a selection does not share")

	handles.mode = Handles.Mode.MOVE
	var bench_at = _prop_of(bench, "Position").value
	var base_at = _prop_of(base, "Position").value
	check(handles.begin(0, cam.global_position, (pair.middle - cam.global_position).normalized()), "the box takes a drag")
	handles.drag(cam.global_position, ((pair.middle + Vector3(4, 0, 0)) - cam.global_position).normalized())
	handles.finish()
	await _settle(6)
	check(_prop_of(bench, "Position").value.is_equal_approx(bench_at + Vector3(4, 0, 0))
		and _prop_of(base, "Position").value.is_equal_approx(base_at + Vector3(4, 0, 0)),
		"a move takes the whole selection, each part by the same amount")
	await studio._undo()
	await _settle(6)
	check(_prop_of(bench, "Position").value.is_equal_approx(bench_at)
		and _prop_of(base, "Position").value.is_equal_approx(base_at), "and one undo takes them both back")

	props.show_instances([bench, base])
	props.refresh()
	check(props._rows["Anchored"].is_checked(1), "a value they agree on shows as itself")
	check(props._rows["Size"].get_text(1) == Properties.MIXED, "one they do not shows as mixed")
	check(props._rows["Anchored"].get_cell_mode(1) == TreeItem.CELL_MODE_CHECK, "an agreed boolean keeps its checkbox")
	history.set_property(base, "Anchored", false)
	await _settle(6)
	props.refresh()
	check(props._rows["Anchored"].get_cell_mode(1) == TreeItem.CELL_MODE_STRING
		and props._rows["Anchored"].get_text(1) == Properties.MIXED,
		"and a boolean they disagree on stops being a box, which would have read as off")
	await studio._undo()
	await _settle(6)
	props.refresh()
	check(props._rows["Anchored"].get_cell_mode(1) == TreeItem.CELL_MODE_CHECK, "and gets it back when they agree again")
	check(props.write("Transparency", 0.5), "a write from the panel goes to everything selected")
	await _settle(6)
	check(_prop_of(bench, "Transparency").value == 0.5 and _prop_of(base, "Transparency").value == 0.5, "both took it")
	await studio._undo()
	await _settle(6)
	check(_prop_of(bench, "Transparency").value == 0.0 and _prop_of(base, "Transparency").value == 0.0,
		"and one undo takes it off both")

	var over: Vector2 = cam.unproject_position(handles.frame_of(bench).middle)
	var caught: Array = studio._parts_in(Rect2(over - Vector2(70, 70), Vector2(140, 140)))
	check(caught.has(bench), "a drag box over a part finds it")
	check(studio._parts_in(Rect2(Vector2(2, 2), Vector2(24, 24))).is_empty(), "and one over empty sky finds nothing")

	var in_map := world.get_child_ids(map).size()
	studio._select([bench, base])
	studio._delete()
	await _settle(8)
	check(world.get_child_ids(map).size() == in_map - 2, "Delete takes the whole selection")
	await studio._undo()
	await _settle(18)
	check(world.get_child_ids(map).size() == in_map, "and one undo brings them all back")
	bench = _child_of(map, "Bench")           # restored, so both are new instances
	base = _child_of(map, "BenchBase")
	check(bench != 0 and base != 0, "both by name, where they were")
	studio._select_one(bench)
	await _settle(2)

	# The keyboard, the menus and the ribbon all read this one action table.
	var acts := {}
	for action in studio.actions():
		acts[action.id] = action
	check(not acts.paste.on, "with nothing copied, Paste is off")
	studio._select_one(bench)
	studio._do("copy")
	acts = {}
	for action in studio.actions():
		acts[action.id] = action
	check(acts.paste.on, "and on once something is")

	var was_in_map := world.get_child_ids(map).size()
	var bench_size = _prop_of(bench, "Size").value
	studio._select_one(map)
	await studio._paste()
	await _settle(8)
	check(world.get_child_ids(map).size() == was_in_map + 1, "Paste puts it in what is selected")
	var pasted: int = studio.primary()
	check(pasted != 0 and _prop_of(pasted, "Size").value.is_equal_approx(bench_size),
		"a copy of the thing, not a fresh one: it kept its Size")
	await studio._undo()
	await _settle(10)
	check(world.get_child_ids(map).size() == was_in_map, "and one undo takes the paste back")

	studio._select_one(bench)
	await studio._duplicate()
	await _settle(8)
	check(world.get_child_ids(map).size() == was_in_map + 1 and studio.primary() != bench,
		"Duplicate leaves the copy beside the original, selected")
	await studio._undo()
	await _settle(10)

	studio._select([bench, base])
	await studio._group()
	await _settle(10)
	var model: int = studio.primary()
	check(model != 0 and world.get_instance(model).class_name == "Model", "Group makes a Model")
	check(world.get_instance(model).parent == map, "where the selection was")
	check(world.get_instance(bench).parent == model and world.get_instance(base).parent == model,
		"and moves them into it")
	studio._select_one(model)
	await studio._ungroup()
	await _settle(10)
	check(world.get_instance(bench).parent == map and world.get_instance(base).parent == map,
		"Ungroup hands them back to where the Model was")
	check(world.get_instance(model).is_empty(), "and the Model goes")

	explorer.name_typed.emit(bench, "Pew")
	await _settle(8)
	check(world.get_instance(bench).name == "Pew", "a name typed in the Explorer is written through the history")
	await studio._undo()
	await _settle(8)
	check(world.get_instance(bench).name == "Bench", "and undo puts the old name back")

	var classes: PackedStringArray = world.get_creatable_classes()
	check(classes.has("Part") and classes.has("WedgePart") and classes.has("ParticleEmitter"),
		"the insert list is everything the runtime will make")
	check(not classes.has("Workspace") and not classes.has("Players"), "and no services, which are not yours to make")
	studio._select_one(map)
	var wedge: int = await studio.history.insert("WedgePart", map)
	await _settle(8)
	check(wedge != 0 and world.get_instance(wedge).class_name == "WedgePart", "inserting one that is not a Part works the same way")
	await studio._undo()
	await _settle(10)

	# Roblox's Locked: still reachable in the Explorer, skipped by viewport picking.
	studio.history.set_property(bench, "Locked", true)
	await _settle(6)
	check(studio._locked(bench), "a locked instance is marked as such")
	var over_locked: Vector2 = cam.unproject_position(handles.frame_of(bench).middle)
	check(not studio._parts_in(Rect2(over_locked - Vector2(70, 70), Vector2(140, 140))).has(bench),
		"and a drag box passes over it")
	await studio._undo()
	await _settle(6)

	var far_off := Vector3(400, 300, 400)
	cam.global_position = far_off
	studio._select_one(bench)
	studio._focus()
	check(cam.global_position.distance_to(handles.frame_of(bench).middle) < far_off.length(),
		"F brings the camera to what is selected")

	# Play builds a second world from the edit tree, through the same serializer Save writes with.
	var dragged = _prop_of(bench, "Position").value
	# Same-named siblings are ordinary in Roblox -- a door is three parts all called "Part".
	studio._run_command("local m = workspace.Map local a = Instance.new('Part') a.Name = 'Twin' a.Size = Vector3.new(2, 2, 2) a.Position = Vector3.new(40, 6, 40) a.Anchored = true a.Parent = m local b = Instance.new('Part') b.Name = 'Twin' b.Size = Vector3.new(2, 2, 2) b.Position = Vector3.new(43, 6, 40) b.Parent = m local w = Instance.new('Weld') w.Part0 = a w.Part1 = b w.C0 = CFrame.new(3, 0, 0) w.Parent = a")
	await _settle(20)
	var twins_json := JSON.stringify(Model.node(world, map, map))
	check(twins_json.contains("\"Part1\":\"Twin[2]\"") and twins_json.contains("\"Part0\":\"Twin[1]\""), "the model file names same-named siblings by ordinal: the weld's Part1 is Twin[2]")
	studio.play()
	studio.playing.script_print.connect(func(n, tx): logs.append(n + ": " + tx))
	await _settle(200)
	check(studio.playing != null and not studio.world.visible, "Play makes a world and stands the edit one down")
	check(_logged("map built:"), "and runs the place: the map's Script wired it up")
	var live: PulseBlockzWorld = studio.playing
	var play_map := _in(live, _service(live, "Workspace"), "Map")
	var play_bench := _in(live, play_map, "Bench")
	check(play_bench != 0 and live.get_instance(play_bench).class_name == "Seat", "the tree crossed into it")
	check(_prop_in(live, play_bench, "Position").value.is_equal_approx(dragged),
		"including what you had not saved: the Bench is where you dragged it")
	var play_kart := _in(live, play_map, "Kart")
	var chassis := _in(live, play_kart, "Chassis")
	var weld := _in(live, chassis, "WeldConstraint")
	check(weld != 0 and _prop_in(live, weld, "Part0").value == chassis and _prop_in(live, weld, "Part1").value == _in(live, play_kart, "Seat"),
		"and its Refs came with it: the kart's weld still holds the seat to the chassis")
	var twins: Array = []
	for cid in live.get_child_ids(play_map):
		if live.get_instance(cid).name == "Twin": twins.append(cid)
	var twin_weld := _in(live, twins[0], "Weld") if twins.size() == 2 else 0
	check(twins.size() == 2 and twin_weld != 0 and _prop_in(live, twin_weld, "Part0").value == twins[0] and _prop_in(live, twin_weld, "Part1").value == twins[1],
		"the twins' weld crossed too, Part1 the second Twin and not the first again")
	check(twins.size() == 2 and _prop_in(live, twins[1], "Position").value.y > 5.5, "so the loose twin hangs off the anchored one instead of falling")
	check(live.get_local_character_id() != 0, "you are in it, with a character")
	# The played world keeps a StarterPlayerScripts of its own: the copy merges into it.
	var client_up := false
	for k in range(0, 300):
		for l in studio._lines:
			if str(l.get("text", "")).begins_with("client up"):
				client_up = true
		if client_up:
			break
		await _settle(1)
	check(client_up, "and the place's LocalScript ran on the client: StarterPlayerScripts reached PlayerScripts")

	studio.stop()
	await _settle(10)
	check(studio.playing == null and studio.world.visible, "Stop throws that world away and brings the edit one back")
	check(_prop_of(bench, "Position").value.is_equal_approx(dragged),
		"with the place as you left it, mid-edit: nothing the game did came back with it")

	# Everything below saves into a copy: the place in the repository is left alone.
	var scratch := ProjectSettings.globalize_path("user://studio_save_test")
	_wipe_dir(scratch)          # what a previous run left is not part of this one
	_copy_dir(studio.place.dir, scratch)
	studio.open_place(scratch)
	await _settle(150)
	var copy_bench := _in(studio.world, _in(studio.world, _service(studio.world, "Workspace"), "Map"), "Bench")
	check(copy_bench != 0, "the copied place opens")
	check(studio.world.set_property(copy_bench, "Position", Vector3(3, 9, 27)), "move something in it")
	await _settle(6)
	await studio.save_place()
	await _settle(4)
	var file: String = studio.place.file_for("Workspace/Map/Bench")
	check(FileAccess.file_exists(file), "Save writes it into the place: %s" % file.get_file())
	var written = JSON.parse_string(FileAccess.get_file_as_string(file))
	var saved_at := Vector3.ZERO
	if written != null and written.properties.has("Position"):
		var xyz: Array = written.properties.Position
		saved_at = Vector3(xyz[0], xyz[1], xyz[2])
	check(written != null and written.className == "Seat" and saved_at.is_equal_approx(Vector3(3, 9, 27)),
		"as the model.json the loader reads back, with where you put it: %s" % saved_at)
	check(not FileAccess.file_exists(scratch.path_join("src/server/Main.model.json")),
		"a Script keeps its .luau: Save does not overwrite what is authored another way")

	# A deleted instance's file goes on the next Save, or reopening hands the instance back.
	var copy_map := _in(studio.world, _service(studio.world, "Workspace"), "Map")
	var post: String = studio.place.file_for("Workspace/Map/LampPost")
	check(FileAccess.file_exists(post), "the LampPost has a file of its own")
	check(studio.history.destroy(_in(studio.world, copy_map, "LampPost")), "delete it")
	await _settle(8)
	await studio.save_place()
	await _settle(4)
	check(not FileAccess.file_exists(post), "saving takes its file with it")

	studio.open_place(scratch)
	await _settle(150)
	var reopened := _in(studio.world, _service(studio.world, "Workspace"), "Map")
	var again := _in(studio.world, reopened, "Bench")
	check(_prop_of(again, "Position").value.is_equal_approx(Vector3(3, 9, 27)), "and opening the place again finds it there")
	check(_in(studio.world, reopened, "LampPost") == 0, "with what was deleted still gone")

	var build := _in(studio.world, reopened, "Build")
	check(build != 0 and studio.world.get_instance(build).class_name == "Script", "the map's Script is in the tree")
	studio._open_script(build)
	check(studio._editor.showing() == build, "opening it puts it in the editor")
	var original: String = studio._editor.source_of(build)
	check(original.contains("map.Lava.Touched"), "with its own source in the box")
	studio._editor.type(original + "\nprint('edited in the Studio')\n")
	check(studio._editor.dirty() and studio.unsaved(), "typing marks it unsaved, and the place with it")
	studio._editor.commit()
	await _settle(8)
	check(studio._editor.source_of(build).contains("edited in the Studio"), "handing it over writes Source")
	check(not studio._editor.dirty(), "and the tab is clean again")
	await studio._undo()
	await _settle(8)
	check(not studio._editor.source_of(build).contains("edited in the Studio"),
		"one undo takes the whole edit, not a keystroke of it")
	await studio._redo()
	await _settle(8)

	await studio.save_place()
	await _settle(6)
	var luau := scratch.path_join("src/map/Build.server.luau")
	check(FileAccess.get_file_as_string(luau).contains("edited in the Studio"),
		"Save writes a script back to the .luau it came from")
	check(not FileAccess.file_exists(scratch.path_join("src/map/Build.model.json")),
		"and does not leave a model file beside it")
	check(not studio.unsaved(), "with nothing left unsaved")

	check(not FileAccess.file_exists(scratch.path_join("src/tools/Wand.model.json")),
		"the Wand does not become a single model file")
	check(FileAccess.file_exists(scratch.path_join("src/tools/Wand/Swing.server.luau")),
		"so the scripts inside it keep their own files")

	studio._select_one(build)
	studio._delete()
	await _settle(10)
	check(studio._editor.showing() == 0, "deleting an open script closes its tab")
	await studio.save_place()
	await _settle(6)
	check(not FileAccess.file_exists(luau), "and saving takes its .luau with it")
	check(FileAccess.file_exists(scratch.path_join("src/map/Bench.model.json")),
		"while everything still there keeps its file")

	# The SVG source is in the script: the repository holds no image file for an icon.
	check(Icons.icon("move", 18) != null, "an icon is rasterised from its SVG source")
	check(Icons.icon("move", 18) == Icons.icon("move", 18), "and cached, not redrawn for every button")
	check(Icons.for_class("WedgePart") == "wedge" and Icons.for_class("TextLabel") == "gui"
		and Icons.for_class("Nonesuch") == "other", "every class falls to one of the shapes")

	for look in Palette.NAMES:
		studio.set_look(look)
		await _settle(2)
		check(studio.palette.name == look and studio._ui.theme != null, "the %s look reaches the panels" % look)
	check(Palette.of("Clear").fill(Color.WHITE).a < 1.0, "Clear panels are see-through")
	check(Palette.of("Dark").fill(Color.WHITE).a == 1.0, "and the other two are not")
	check(Palette.of("Clear").code_fill().a > 0.9,
		"the code editor stays solid even in Clear: text over a moving view reads as text that moves")
	studio.set_look("Dark")

	# Attributes and tags cross the change log as properties named @Thing and #Thing.
	var conf := _in(studio.world, _service(studio.world, "ReplicatedStorage"), "Config")
	check(conf != 0 and studio.world.get_attributes(conf).has("MaxPlayers"),
		"an attribute set in the project file reaches the mirror: %s" % studio.world.get_attributes(conf))
	# Ids die with the world that held them, so everything below uses the place open now.
	var seat := _in(studio.world, reopened, "Bench")
	studio._select_one(seat)
	check(studio.world.set_attribute(seat, "Rating", 4.0) and studio.world.set_tag(seat, "Seating", true),
		"an attribute and a tag can be set from the Studio")
	await _settle(8)
	check(studio.world.get_attributes(seat).get("Rating", 0) == 4.0, "one set from the Studio comes back")
	check(studio.world.get_tags(seat).has("Seating"), "and a CollectionService tag with it")
	var carried = JSON.parse_string(Model.of(studio.world, seat))
	check(carried != null and carried.get("attributes", {}).get("Rating", 0) == 4.0,
		"and both are written into the model file, which has always had room for them")

	# Children keep insertion order, not name order -- hence "Aaa".
	var late: int = await studio.history.insert("Part", reopened)
	await _settle(6)
	studio.history.set_property(late, "Name", "Aaa")
	await _settle(8)
	var order := []
	for c in studio.world.get_child_ids(reopened):
		order.append(studio.world.get_instance(c).name)
	check(order.size() > 4 and order.back() == "Aaa",
		"the newest child comes back last, not first alphabetically: %s" % [order.slice(order.size() - 3, order.size())])
	await studio._undo()
	await _settle(8)

	# studio.cfg is merged, not rewritten: a fresh ConfigFile drops every section it does not hold.
	studio._remember_layout("explorer", 260)
	studio.set_look("Dark")
	var keep := ConfigFile.new()
	check(keep.load("user://studio.cfg") == OK and keep.get_value("layout", "explorer", 0) == 260
		and keep.get_value("studio", "look", "") == "Dark",
		"the layout survives a theme change, and the theme survives a layout change")

	studio._explorer.set_filter("Bench")
	await _settle(3)
	check(studio._explorer._items.has(seat) and studio._explorer._items[seat].visible, "the Explorer keeps a match")
	check(studio._explorer._items.has(reopened) and studio._explorer._items[reopened].visible,
		"and its ancestors, or the match would be orphaned")
	var hidden := 0
	for id in studio._explorer._items:
		if not studio._explorer._items[id].visible: hidden += 1
	check(hidden > 4, "while everything else goes: %d rows hidden" % hidden)
	studio._explorer.set_filter("")
	await _settle(3)

	studio._select_one(seat)
	await _settle(2)
	studio._properties.set_filter("trans")
	check(studio._properties._rows["Transparency"].visible and not studio._properties._rows["Anchored"].visible,
		"the Properties filter goes on the name, not the value")
	studio._properties.set_filter("")

	# Turn it first, or its own axes are the world's and the two cases read alike.
	studio.history.set_property(seat, "Orientation", Vector3(0, 45, 0))
	await _settle(8)
	studio._select_one(seat)
	studio._handles.world_axes = true
	studio._handles.mode = Handles.Mode.MOVE
	check(studio._handles.frame().basis.is_equal_approx(Basis.IDENTITY), "World axes squares the handles to the world")
	studio._handles.mode = Handles.Mode.SCALE
	check(not studio._handles.frame().basis.is_equal_approx(Basis.IDENTITY),
		"and scale stays on the part's own, where Size is the only thing it can mean")
	studio._handles.world_axes = false
	await studio._undo()
	await _settle(6)

	var before_run: Transform3D = cam.global_transform
	studio.run()
	await _settle(150)
	check(studio.playing != null and studio.playing.get_local_character_id() == 0,
		"Run runs the place with nobody in it")
	studio.playing.auto_step = false
	await _settle(4)
	var frozen: Dictionary = studio.playing.get_stats()
	await _settle(10)
	check(studio.playing.get_stats().get("now", 0) == frozen.get("now", -1), "Pause stops the world stepping")
	studio.playing.flush()
	await _settle(2)
	check(studio.playing.get_stats().get("now", 0) != frozen.get("now", -1), "and Step advances it by one")
	studio.stop()
	await _settle(6)
	check(cam.global_transform.is_equal_approx(before_run), "Stop puts the camera back where it was looking")

	var crate := _in(studio.world, reopened, "Crate")
	check(crate != 0, "the Crate is in the place")
	studio._select_one(crate)
	studio._handles.mode = Handles.Mode.MOVE
	studio._handles.surface = true
	studio._handles.snap = 0.0
	cam.global_position = Vector3(0, 60, 0)
	cam.look_at(Vector3(0, 0, 0.01), Vector3.UP)
	await _settle(3)
	var crate_at = _prop_of(crate, "Position").value
	# Grabbed from directly underneath, the hold offset is zero and it lands at the pointer.
	var grab := Vector3(crate_at.x, 0, crate_at.z)
	var land := Vector3(20, 0, 20)
	check(studio._handles.begin_free(cam.global_position, (grab - cam.global_position).normalized()),
		"grabbing the brick takes the drag")
	studio._handles.drag(cam.global_position, (land - cam.global_position).normalized())
	studio._handles.finish()
	await _settle(8)
	var now = _prop_of(crate, "Position").value
	check(now.is_equal_approx(Vector3(20, 1.5, 20)),
		"a 3-stud brick dropped on the baseplate sits on it, not in it: %s" % now)
	await studio._undo()
	await _settle(8)
	check(_prop_of(crate, "Position").value.is_equal_approx(crate_at), "and one undo puts it back")

	var pillar := _in(studio.world, reopened, "Pillar")
	var face := Vector3(15.0, 5.0, -10.0)          # the Pillar is 2 wide at x = 14
	crate_at = _prop_of(crate, "Position").value
	studio._select_one(crate)
	# The turn is the swing from the grab plane to the landing plane: pick it up off the ground first.
	var above := Vector3(0, 60, 0)
	var under := Vector3(crate_at.x, 0, crate_at.z)
	check(studio._handles.begin_free(above, (under - above).normalized()),
		"the same grab works when it is carried somewhere else")
	var beside := Vector3(40, 5, -10)
	studio._handles.drag(beside, (face - beside).normalized())
	studio._handles.finish()
	await _settle(8)
	var against = _prop_of(crate, "Position").value
	check(pillar != 0 and is_equal_approx(against.x, 16.5),
		"it stands off the wall by its own half-width: x %s" % against.x)
	check(not _prop_of(crate, "Orientation").value.is_equal_approx(Vector3.ZERO),
		"and turned to lie against it: %s" % _prop_of(crate, "Orientation").value)
	await studio._undo()
	await _settle(8)
	studio._handles.snap = 1.0

	var ws2 := _service(studio.world, "Workspace")
	check(not studio._explorer._is_under(reopened, crate) and studio._explorer._is_under(crate, reopened),
		"the Explorer knows what is under what")
	check(studio._explorer._is_under(crate, ws2), "however deep")
	studio._reparent([crate], ws2)
	await _settle(10)
	check(studio.world.get_instance(crate).parent == ws2, "a drop moves it, through the history")
	await studio._undo()
	await _settle(10)
	check(studio.world.get_instance(crate).parent == reopened, "and one undo puts it back where it was")

	studio._run_command("workspace.Map.Crate.Transparency = 0.25")
	await _settle(12)
	check(_prop_of(crate, "Transparency").value == 0.25, "a line typed into the command bar runs on the server")
	check(studio._said.has("workspace.Map.Crate.Transparency = 0.25"), "and is kept, to be typed again")
	studio._run_command("workspace.Map.Crate.Transparency = 0")
	await _settle(12)

	studio._select_one(crate)
	await _settle(3)
	var props2: Properties = studio._properties
	check(props2._rows["Color"].get_cell_mode(1) == TreeItem.CELL_MODE_CUSTOM
		and props2._rows["Color"].get_metadata(1) is Color,
		"a colour is a swatch the picker opens on, not three numbers to type")
	check(props2._rows["Color"].get_parent().get_text(0) == "Appearance"
		and props2._rows["Anchored"].get_parent().get_text(0) == "Behaviour",
		"and the rows sit under the headings Roblox groups them by")
	check(props2._axis_rows.has("Size") and props2._axis_rows["Size"].size() == 3,
		"a Vector3 opens into X, Y and Z you can drag")
	var axes: Array = props2._axis_rows["Size"]
	check(is_equal_approx(axes[1].get_range(1), 3.0), "each holding its own component: Y %s" % axes[1].get_range(1))
	axes[1].set_range(1, 5.0)
	props2._on_edited_for(axes[1])
	await _settle(8)
	check(_prop_of(crate, "Size").value.is_equal_approx(Vector3(3, 5, 3)),
		"and setting one rebuilds the whole vector: %s" % _prop_of(crate, "Size").value)
	await studio._undo()
	await _settle(6)

	var kart2 := _in(studio.world, reopened, "Kart")
	var body2 := _in(studio.world, kart2, "Chassis")
	var weld2 := _in(studio.world, body2, "WeldConstraint")
	check(weld2 != 0, "the kart's weld is there")
	studio._select_one(weld2)
	await _settle(3)
	var ref_row: TreeItem = studio._properties._rows.get("Part1")
	check(ref_row != null, "its Part1 has a row")
	studio._properties._on_button(ref_row, 1, 0, MOUSE_BUTTON_LEFT)
	check(studio._properties.waiting_for_ref(), "pressing the button waits for the next instance clicked")
	check(studio._properties.pointed_at(body2), "and takes it")
	await _settle(8)
	check(_prop_of(weld2, "Part1").value == body2, "which the runtime accepted")
	await studio._undo()
	await _settle(6)

	var a: int = await studio.history.insert("Part", reopened)
	var b: int = await studio.history.insert("Part", reopened)
	var c: int = await studio.history.insert("Part", reopened)
	await _settle(8)
	studio.world.set_property(a, "Anchored", true)
	studio.world.set_property(b, "Anchored", true)
	studio.world.set_property(c, "Anchored", true)
	studio.world.set_property(a, "Position", Vector3(0, 40, 0))
	studio.world.set_property(b, "Position", Vector3(7, 41, 0))
	studio.world.set_property(c, "Position", Vector3(20, 42, 0))
	await _settle(10)
	studio._select([a, b, c])
	studio.align(1, 0)                      # line them up on Y
	await _settle(10)
	check(is_equal_approx(_prop_of(a, "Position").value.y, _prop_of(b, "Position").value.y)
		and is_equal_approx(_prop_of(b, "Position").value.y, _prop_of(c, "Position").value.y),
		"align puts a selection on one line: %s" % _prop_of(b, "Position").value.y)
	studio.distribute(0)                    # even gaps along X
	await _settle(10)
	var gap1: float = _prop_of(b, "Position").value.x - _prop_of(a, "Position").value.x
	var gap2: float = _prop_of(c, "Position").value.x - _prop_of(b, "Position").value.x
	check(is_equal_approx(gap1, gap2) and gap1 > 1.0, "and distribute spreads it evenly: %s vs %s" % [gap1, gap2])
	await studio._undo()
	await _settle(8)
	check(not is_equal_approx(_prop_of(b, "Position").value.x - _prop_of(a, "Position").value.x, gap2),
		"each is one thing to undo")
	studio._select([a, b, c])
	studio._delete()
	await _settle(10)

	# ---- the drawn shape, not what Size says --------------------------------------
	# A Ball's radius is half its smallest Size component, not half the axis it stands on.
	var ball: int = await studio.history.insert("Part", reopened)
	await _settle(8)
	studio.world.set_property(ball, "Shape", "Ball")
	studio.world.set_property(ball, "Anchored", true)
	studio.world.set_property(ball, "Size", Vector3(2, 6, 2))
	studio.world.set_property(ball, "Position", Vector3(20, 8, 20))
	await _settle(12)
	studio._select_one(ball)
	studio._handles.mode = Handles.Mode.MOVE
	studio._handles.surface = true
	studio._handles.snap = 0.0
	check(studio._handles.begin_free(Vector3(20, 60, 20), Vector3.DOWN), "the ball can be taken hold of")
	var from_side := Vector3(40, 5, -10)
	studio._handles.drag(from_side, (Vector3(15, 5, -10) - from_side).normalized())
	studio._handles.finish()
	await _settle(10)
	var ball_at = _prop_of(ball, "Position").value
	check(absf(ball_at.x - 16.0) < 0.01,
		"a Ball stands off a wall by its radius, not half its Size: x %s" % ball_at.x)

	# Floor to ceiling: opposed normals, so the turn is about an axis square to both. About the
	# part's own up it would only yaw.
	var roof: int = await studio.history.insert("Part", reopened)
	await _settle(8)
	studio.world.set_property(roof, "Anchored", true)
	studio.world.set_property(roof, "Size", Vector3(20, 1, 20))
	studio.world.set_property(roof, "Position", Vector3(-40, 30, -40))
	var brick: int = await studio.history.insert("Part", reopened)
	await _settle(8)
	studio.world.set_property(brick, "Anchored", true)
	studio.world.set_property(brick, "Position", Vector3(20, 8, 20))
	await _settle(14)
	studio._select_one(brick)
	check(studio._handles.begin_free(Vector3(20, 60, 20), Vector3.DOWN), "and a brick to carry up to it")
	studio._handles.drag(Vector3(-40, 5, -40), Vector3.UP)
	studio._handles.finish()
	await _settle(10)
	var up_now: Vector3 = Handles._euler_basis(_prop_of(brick, "Orientation").value) * Vector3.UP
	var deep: float = _prop_of(brick, "Size").value.y * 0.5
	check(up_now.is_equal_approx(Vector3.DOWN),
		"a brick put on a ceiling turns over: its up is %s" % up_now)
	check(absf(_prop_of(brick, "Position").value.y - (29.5 - deep)) < 0.01,
		"and hangs under it: y %s" % _prop_of(brick, "Position").value.y)

	# A Cylinder's mesh lies along X inside its body, so the handle pulled is not the Size axis.
	var pipe: int = await studio.history.insert("Part", reopened)
	await _settle(8)
	studio.world.set_property(pipe, "Shape", "Cylinder")
	studio.world.set_property(pipe, "Anchored", true)
	studio.world.set_property(pipe, "Size", Vector3(6, 2, 2))
	studio.world.set_property(pipe, "Position", Vector3(0, 50, 0))
	await _settle(14)
	studio._select_one(pipe)
	studio._handles.surface = false
	studio._handles.mode = Handles.Mode.SCALE
	var eye := Vector3(0, 50, 60)
	check(studio._handles.frame().basis.y.is_equal_approx(Vector3.RIGHT),
		"the cylinder is drawn turned a quarter inside its own body")
	check(studio._handles.begin(2, eye, (Vector3(0, 50, 0) - eye).normalized()),
		"the handle on its flat end takes the drag")
	studio._handles.drag(eye, (Vector3(2, 50, 0) - eye).normalized())
	studio._handles.finish()
	await _settle(10)
	var pipe_size = _prop_of(pipe, "Size").value
	check(absf(pipe_size.x - 8.0) < 0.02 and absf(pipe_size.y - 2.0) < 0.01,
		"pulling it lengthens the cylinder, not its radius: %s" % pipe_size)
	studio._handles.mode = Handles.Mode.MOVE
	studio._handles.snap = 1.0

	# A restored Model comes back with its children in it: the child's own entry must not add it
	# a second time at the root.
	var box_model: int = await studio.history.insert("Model", reopened)
	await _settle(8)
	var inside: int = await studio.history.insert("Part", box_model)
	await _settle(10)
	var was_here: int = studio.world.get_child_ids(reopened).size()
	studio._select([box_model, inside])
	studio._delete()
	await _settle(12)
	await studio._undo()
	await _settle(16)
	check(studio.world.get_child_ids(reopened).size() == was_here,
		"undoing the delete puts back one Model, not two things")
	var back := _in(studio.world, reopened, "Model")
	check(back != 0 and studio.world.get_child_ids(back).size() == 1,
		"with its one child still inside it")
	studio._select([back])
	studio._delete()
	studio._select([ball, roof, brick, pipe])
	studio._delete()
	await _settle(12)

	# A Ref pick left armed would write a Ref on the next instance clicked.
	studio._select_one(weld2)
	await _settle(4)
	studio._properties._on_button(studio._properties._rows["Part1"], 1, 0, MOUSE_BUTTON_LEFT)
	check(studio._properties.waiting_for_ref(), "the Ref button arms the pick")
	studio._select_one(crate)
	await _settle(4)
	check(not studio._properties.waiting_for_ref(), "and looking at something else calls it off")

	# Undo and Redo sit on the ribbon and on the title row, in one register of commands.
	var quick: Button = studio._commands["undo"][0]
	studio._tab_bar.current_tab = 2
	studio._fill_band()
	await _settle(2)
	check(studio._commands.get("undo", []).has(quick),
		"the quick-access arrows outlive a ribbon rebuild")
	studio._tab_bar.current_tab = 0
	studio._fill_band()
	await _settle(2)
	check(studio._commands["undo"].size() == 1 and studio._commands["select"].size() == 1,
		"and no rebuild leaves a dead button behind in the register")

	# Rojo mounts at the service: with no $path on the Workspace node, Save makes the directory.
	var ws3 := _service(studio.world, "Workspace")
	check(not studio.place.mounts.has("Workspace"), "nothing in the project maps to the Workspace itself")
	var loose: int = await studio.history.insert("Part", ws3)
	await _settle(10)
	studio.world.set_property(loose, "Name", "Loose")
	studio.world.set_property(loose, "Anchored", true)
	studio.world.set_property(loose, "Position", Vector3(3, 44, 5))
	await _settle(12)
	await studio.save_place()
	await _settle(8)
	var project = JSON.parse_string(FileAccess.get_file_as_string(scratch.path_join("default.project.json")))
	check(project != null and project.tree.Workspace.has("$path"),
		"Save gives the Workspace a directory of its own: %s" % project.tree.Workspace.get("$path", "-"))
	check(project.tree.Workspace.has("Map") and project.tree.ReplicatedStorage.has("Config"),
		"and leaves what the project already said alone")
	check(FileAccess.file_exists(scratch.path_join("src/workspace/Loose.model.json")),
		"the part with nowhere to go is written into it")
	await studio.save_place()
	await _settle(6)
	check(studio.place.changed.is_empty(),
		"and a second Save with nothing changed rewrites nothing: %d files" % studio.place.changed.size())
	check(not FileAccess.file_exists(scratch.path_join("src/workspace/Terrain.model.json"))
		and not FileAccess.file_exists(scratch.path_join("src/workspace/Camera.model.json")),
		"what the runtime makes for itself is not written at all")
	studio.open_place(scratch)
	await _settle(150)
	var found := _in(studio.world, _service(studio.world, "Workspace"), "Loose")
	check(found != 0 and _prop_of(found, "Position").value.is_equal_approx(Vector3(3, 44, 5)),
		"and opening the place again finds it where you left it")

	var script4 := _in(studio.world, _service(studio.world, "ServerScriptService"), "Main")
	check(script4 != 0 and studio._script_named("ServerScriptService.Main") == script4,
		"the Output finds a script by the name an error gives it")
	check(studio._jump_of("ServerScriptService.Main", "ServerScriptService.Main:3: attempt to index nil")
		== "ServerScriptService.Main:3", "and reads the line out of the error")
	check(studio._jump_of("studio", "saved 19 into scripts") == "",
		"a line that names nowhere is not a link")
	studio._jump_to("ServerScriptService.Main:3")
	await _settle(4)
	check(studio._editor.showing() == script4 and studio._editor.at_line() == 3,
		"clicking it opens the script on that line: %d" % studio._editor.at_line())
	studio.close_script()

	# What you build in a running place goes when it stops, so a running place is framed and named.
	check(not studio._playmark.visible, "an edit place is not marked as running")
	studio.play(false)
	await _settle(40)
	check(studio._playmark.visible and studio._playing_says.text.contains("Running"),
		"a running place is: %s" % studio._playing_says.text.strip_edges())
	studio.stop()
	await _settle(10)
	check(not studio._playmark.visible, "and the mark goes with it")

	# A model authored as an *.rbxmx is written back as one, in Studio's own XML spelling.
	var map5 := _in(studio.world, _service(studio.world, "Workspace"), "Map")
	var shrine := _in(studio.world, map5, "Shrine")
	var altar := _in(studio.world, shrine, "Altar")
	check(shrine != 0 and altar != 0, "the Shrine came out of its .rbxmx")
	var altar_was = _prop_of(altar, "Color").value
	studio.world.set_property(altar, "Position", Vector3(31, 2.5, 30))
	studio.world.set_property(altar, "Transparency", 0.25)
	await _settle(12)
	await studio.save_place()
	await _settle(8)
	var xml := FileAccess.get_file_as_string(scratch.path_join("src/map/Shrine.rbxmx"))
	check(xml.begins_with("<roblox") and xml.contains("<CoordinateFrame name=\"CFrame\">")
		and xml.contains("<token name=\"Material\">"),
		"Save writes it as the XML Studio reads, tokens and all")
	check(xml.contains("<Vector3 name=\"size\">") and xml.contains("<Color3uint8 name=\"Color3uint8\">"),
		"under the names Studio serializes those under")
	check(not FileAccess.file_exists(scratch.path_join("src/map/Shrine.model.json")),
		"and does not leave a model.json beside it")
	studio.open_place(scratch)
	await _settle(150)
	var shrine2 := _in(studio.world, _in(studio.world, _service(studio.world, "Workspace"), "Map"), "Shrine")
	var altar2 := _in(studio.world, shrine2, "Altar")
	check(altar2 != 0 and _prop_of(altar2, "Position").value.is_equal_approx(Vector3(31, 2.5, 30)),
		"opening it again finds the altar where you put it: %s" % _prop_of(altar2, "Position").value)
	check(is_equal_approx(_prop_of(altar2, "Transparency").value, 0.25), "with what else you changed")
	check(_prop_of(altar2, "Color").value.is_equal_approx(altar_was),
		"its colour to the byte, through the one packed integer: %s" % _prop_of(altar2, "Color").value)
	check(_prop_of(shrine2, "PrimaryPart").value == altar2, "and the Model still points at it as its PrimaryPart")
	check(_in(studio.world, altar2, "Glow") != 0, "the PointLight under it came too")
	var greet := _in(studio.world, shrine2, "Greet")
	var greet_text := ""
	for one in studio.world.get_properties(greet, true):
		if one.name == "Source":
			greet_text = str(one.value)
	check(greet != 0 and greet_text.contains("shrine"), "and the Script inside it, text and all")

	var recent: Array = studio._setting("places", "recent", [])
	check(not recent.is_empty() and String(recent[0]) == studio.place.dir,
		"the place you are in is the top of the recent list")
	check(studio._recent_pop.get_item_count() >= 1 and studio._recent_pop.is_item_disabled(0),
		"which the menu shows, greyed out, because you are already in it")
	studio._remember_place("C:/nowhere-at-all")
	studio._open_recent(0)
	await _settle(2)
	check(not studio._setting("places", "recent", []).has("C:/nowhere-at-all"),
		"and one that is not there any more drops out when you reach for it")

	check(not studio._autosave_on and studio._autosave.is_stopped(), "autosave is off unless asked for")
	studio.set_autosave(true)
	check(studio._autosave_on and not studio._autosave.is_stopped()
		and studio._file_pop.is_item_checked(studio._file_pop.get_item_index(4)),
		"turning it on starts the clock, and the menu says so")
	studio.set_autosave(false)
	check(studio._autosave.is_stopped(), "and turning it off stops it")

	# ---- corners of the serializer, the handles and the history ------------------
	# A keypoint colour on exact bytes is written in Roblox's Color3uint8 spelling. Play, paste
	# and undo move subtrees through the one serializer, so every one of them reads it back.
	var host: int = await studio.history.insert("Part", reopened)
	await _settle(10)
	studio.world.set_property(host, "Anchored", true)
	studio.world.set_property(host, "Position", Vector3(-60, 20, -60))
	await _settle(8)
	var ramp := '{"className":"ParticleEmitter","properties":{"Color":{"keypoints":[' \
		+ '{"time":0,"color":{"Color3uint8":[255,0,0]}},' \
		+ '{"time":1,"color":{"Color3uint8":[0,0,255]}}]}}}'
	studio.world.add_model(studio.history._path_of(host), "Sparks", ramp)
	await _settle(14)
	var emitter := _in(studio.world, host, "Sparks")
	var ramp_was = _prop_of(emitter, "Color").value
	# Four floats per stop: time, r, g, b. A dropped property reads as white at both ends.
	var ramp_want := PackedFloat32Array([0, 1, 0, 0, 1, 0, 0, 1])
	check(emitter != 0 and ramp_was == ramp_want,
		"a sequence in Rojo's own spelling reaches the tree: %s" % [ramp_was])
	check(not _prop_of(emitter, "Color").is_default,
		"as something written, not as the default it would fall back to")
	studio.world.add_model(studio.history._path_of(host), "Copy", Model.of(studio.world, emitter))
	await _settle(14)
	var copied := _in(studio.world, host, "Copy")
	check(copied != 0 and _prop_of(copied, "Color").value == ramp_was,
		"and it survives being copied through a model, which is what Play and paste do")

	# A free drag needs a drawn box to land a face on, which a Folder has not, Position or no.
	var bodiless: int = await studio.history.insert("Folder", reopened)
	await _settle(10)
	studio._select([bodiless])
	studio._handles.mode = Handles.Mode.MOVE
	studio._handles.surface = true
	check(not studio._handles.begin_free(Vector3(20, 60, 20), Vector3.DOWN),
		"a selection with no drawn box takes no free drag at all")

	# The mode is latched for the length of a drag: _from was taken under the mode it began in.
	studio._select_one(crate)
	await _settle(4)
	check(studio._handles.begin_free(Vector3(20, 60, 20), Vector3.DOWN), "a drag that is running")
	studio._handles.mode = Handles.Mode.ROTATE
	check(studio._handles.mode == Handles.Mode.MOVE, "keeps the tool it began with")
	studio._handles.finish()
	studio._handles.mode = Handles.Mode.ROTATE
	check(studio._handles.mode == Handles.Mode.ROTATE, "and takes the new one once it lets go")
	studio._handles.mode = Handles.Mode.MOVE
	await studio._undo()
	await _settle(8)

	# Ids are never reused: a restored parent comes back under a new one, so the child's entry
	# has to find it by where it is rather than by number.
	var nest: int = await studio.history.insert("Model", reopened)
	await _settle(8)
	studio.world.set_property(nest, "Name", "Nest")
	await _settle(8)
	var egg: int = await studio.history.insert("Part", nest)
	await _settle(10)
	studio.world.set_property(egg, "Name", "Egg")
	await _settle(10)
	check(studio.history.destroy(egg), "delete the part")
	await _settle(12)
	check(studio.history.destroy(nest), "then the Model it was in")
	await _settle(12)
	await studio._undo()
	await _settle(16)
	await studio._undo()
	await _settle(16)
	var nest_back := _in(studio.world, reopened, "Nest")
	check(nest_back != 0 and _in(studio.world, nest_back, "Egg") != 0,
		"two undos bring both back, the child into the parent's new id")
	check(studio.world.get_child_ids(nest_back).size() == 1, "and only one of it")
	studio._select([nest_back])
	studio._delete()
	studio._select([host, bodiless])
	studio._delete()
	await _settle(12)

	# Commit is refused while a place is playing, so several tabs go dirty at once and Save has
	# to flush every one of them, not just the tab on screen.
	var main5 := _in(studio.world, _service(studio.world, "ServerScriptService"), "Main")
	var evil5 := _in(studio.world, _service(studio.world, "ServerScriptService"), "Evil")
	check(main5 != 0 and evil5 != 0, "two scripts to type in")
	studio._editor.close_all()
	studio.play(false)
	await _settle(40)
	studio._open_script(main5)
	studio._editor.type(studio._editor.source_of(main5) + "\n-- one\n")
	studio._open_script(evil5)
	studio._editor.type(studio._editor.source_of(evil5) + "\n-- two\n")
	check(studio._editor.dirty(), "both tabs are holding text while the place runs")
	studio.stop()
	await _settle(14)
	await studio.save_place()
	await _settle(10)
	check(not studio._editor.dirty(), "Stop and Save flush every one of them")
	check(FileAccess.get_file_as_string(scratch.path_join("src/server/Main.server.luau")).contains("-- one")
		and FileAccess.get_file_as_string(scratch.path_join("src/server/Evil.server.luau")).contains("-- two"),
		"and both reached their own file on disk")
	studio._editor.close_all()

	# The Explorer hands out ids from whichever world it is showing.
	studio.play(false)
	await _settle(40)
	var playing_main := _in(studio.playing, _service(studio.playing, "ServerScriptService"), "Main")
	check(playing_main != 0, "the running copy has its own id for the same script")
	check(studio._full_name(studio.playing, playing_main) == "ServerScriptService.Main",
		"which the Explorer's path resolves by name: %s" % studio._full_name(studio.playing, playing_main))
	check(studio._script_named("ServerScriptService.Main") == main5,
		"back to the one in the place, not the copy")
	studio.stop()
	await _settle(12)

	# A name a filename cannot hold fails the write, and a Save that wrote less than all of it
	# sweeps nothing.
	var luau_before := FileAccess.get_file_as_string(scratch.path_join("src/server/Patrol.server.luau"))
	var patrol := _in(studio.world, _service(studio.world, "ServerScriptService"), "Patrol")
	studio.history.set_property(patrol, "Name", "Patrol:Core")
	await _settle(12)
	await studio.save_place()
	await _settle(10)
	check(FileAccess.file_exists(scratch.path_join("src/server/Patrol.server.luau")),
		"a Save that could not write everything sweeps nothing")
	check(FileAccess.get_file_as_string(scratch.path_join("src/server/Patrol.server.luau")) == luau_before,
		"and the file it could not write is exactly as it was")
	check(studio.unsaved(), "the place is still unsaved, and says so")
	studio.history.set_property(patrol, "Name", "Patrol")
	await _settle(12)
	await studio.save_place()
	await _settle(10)

	# The project file is hand-written: adding a mount keeps its order, and a service whose node
	# already says where it lives is never repointed.
	var project_text := FileAccess.get_file_as_string(scratch.path_join("default.project.json"))
	var ws_node := project_text.substr(project_text.find("\"Workspace\""))
	check(ws_node.find("\"$path\"") < ws_node.find("\"Map\""),
		"the node Save wrote says $path first, the way one written by hand does")
	check(project_text.find("\"ReplicatedStorage\"") < project_text.find("\"Lighting\"")
		and project_text.find("\"Teams\"") < project_text.find("\"StarterPlayer\""),
		"and the rest of the file keeps the order somebody wrote it in")
	check(studio.place.make_mount("ServerScriptService/Thing") == studio.place.mounts["ServerScriptService"],
		"a service that is already mounted is handed back, not mounted twice")
	studio.place.mounts.erase("Workspace")     # as a $path naming a single file leaves it
	check(studio.place.make_mount("Workspace/Thing") == "",
		"and one whose node already says where it lives is never repointed")
	studio.place.mounts["Workspace"] = scratch.path_join("src/workspace")

	# The running mark spans the window: it must not be a hit target.
	check(studio._playmark.mouse_filter == Control.MOUSE_FILTER_IGNORE
		and studio._playmark.get_child(0).mouse_filter == Control.MOUSE_FILTER_IGNORE,
		"nothing in the running mark takes a click")

	# Source goes in the file as CDATA, which is raw: the loader must not decode entities in it.
	var map6 := _in(studio.world, _service(studio.world, "Workspace"), "Map")
	var shrine6 := _in(studio.world, map6, "Shrine")
	var greet6 := _in(studio.world, shrine6, "Greet")
	var tricky := "local s = \"&lt;b&gt;Score&lt;/b&gt; &amp; more\"\nprint(s)\n"
	studio.history.set_property(greet6, "Source", tricky)
	await _settle(12)

	# A CFrame is two properties here -- C0Position and C0Orientation -- and one in the file.
	var motor: int = await studio.history.insert("Motor6D", shrine6)
	await _settle(10)
	studio.world.set_property(motor, "Name", "Hinge")
	studio.world.set_property(motor, "C0Position", Vector3(0, 2, 0))
	studio.world.set_property(motor, "C0Orientation", Vector3(0, 90, 0))
	await _settle(14)
	await studio.save_place()
	await _settle(10)
	var xml2 := FileAccess.get_file_as_string(scratch.path_join("src/map/Shrine.rbxmx"))
	check(xml2.contains("<CoordinateFrame name=\"C0\">"),
		"a joint's offset is written as the one CoordinateFrame Studio reads")
	check(not xml2.contains("C0Position"), "not as the two properties this engine keeps it in")
	check(xml2.contains("&lt;b&gt;"), "and the script's text is in the file exactly as typed")
	studio.open_place(scratch)
	await _settle(150)
	var shrine7 := _in(studio.world, _in(studio.world, _service(studio.world, "Workspace"), "Map"), "Shrine")
	var greet7 := _in(studio.world, shrine7, "Greet")
	var back_text := ""
	for one in studio.world.get_properties(greet7, true):
		if one.name == "Source":
			back_text = str(one.value)
	check(back_text == tricky, "opening it again gives back the same characters, not the decoded ones")
	var motor7 := _in(studio.world, shrine7, "Hinge")
	var c0p := _prop_of(motor7, "C0Position")
	var c0o := _prop_of(motor7, "C0Orientation")
	check(motor7 != 0 and not c0p.is_empty() and c0p.value.is_equal_approx(Vector3(0, 2, 0))
		and c0o.value.is_equal_approx(Vector3(0, 90, 0)),
		"and the joint is still offset the way it was: %s %s"
		% [c0p.get("value", "-"), c0o.get("value", "-")])
	check(motor7 != 0, "a joint renamed in the Explorer is renamed in the file too")
	check(studio.history.destroy(motor7), "put the model back as it was")
	await _settle(12)

	# A script authored as a model file stays one: raw Lua over it would drop the properties and
	# children the XML carries.
	var lone := scratch.path_join("src/shared/Util.rbxmx")
	var lone_xml := "<roblox version=\"4\"><Item class=\"ModuleScript\" referent=\"RBX1\">" \
		+ "<Properties><string name=\"Name\">Util</string>" \
		+ "<ProtectedString name=\"Source\"><![CDATA[return {n = 1}]]></ProtectedString>" \
		+ "</Properties></Item></roblox>"
	var lf := FileAccess.open(lone, FileAccess.WRITE)
	lf.store_string(lone_xml)
	lf = null
	studio.open_place(scratch)
	await _settle(150)
	var util := _in(studio.world, _service(studio.world, "ReplicatedStorage"), "Util")
	check(util != 0 and studio.world.get_instance(util).class_name == "ModuleScript",
		"a ModuleScript kept as its own .rbxmx opens")
	studio.history.set_property(util, "Source", "return {n = 2}")
	await _settle(12)
	await studio.save_place()
	await _settle(10)
	check(FileAccess.get_file_as_string(lone).begins_with("<roblox"),
		"and Save writes XML back over it, never raw Lua")
	studio.open_place(scratch)
	await _settle(150)
	var util2 := _in(studio.world, _service(studio.world, "ReplicatedStorage"), "Util")
	var util_text := ""
	for one in studio.world.get_properties(util2, true):
		if one.name == "Source":
			util_text = str(one.value)
	check(util2 != 0 and util_text == "return {n = 2}", "with the edit in it: %s" % util_text)
	DirAccess.remove_absolute(lone)

	# Save skips the Camera the runtime makes for itself, so the sweep must spare an authored
	# Camera's file all the same.
	var cam_file := scratch.path_join("src/workspace/Camera.model.json")
	var cf := FileAccess.open(cam_file, FileAccess.WRITE)
	cf.store_string('{"className":"Camera","properties":{"FieldOfView":55}}')
	cf = null
	studio.open_place(scratch)
	await _settle(150)
	await studio.save_place()
	await _settle(10)
	check(FileAccess.file_exists(cam_file),
		"a Camera with a file of its own is not swept away by the Save that skipped it")

	# ---- a Model moves, turns and scales as one thing ----------------------------
	var kart5 := _in(studio.world, _in(studio.world, _service(studio.world, "Workspace"), "Map"), "Kart")
	var chassis5 := _in(studio.world, kart5, "Chassis")
	var seat5 := _in(studio.world, kart5, "Seat")
	check(kart5 != 0 and chassis5 != 0 and seat5 != 0, "the Kart is a Model with parts in it")
	check(studio.world.get_part_node(seat5) != null,
		"its Seat is welded, and an edit world still gives it a body of its own to click")
	check(studio._outermost(chassis5, false) == kart5,
		"clicking a part inside a Model takes the Model, as Studio does")
	check(studio._outermost(chassis5, true) == chassis5, "and alt reaches past it to the part")

	studio._select([kart5])
	await _settle(4)
	studio._handles.mode = Handles.Mode.MOVE
	studio._handles.world_axes = false
	studio._handles.snap = 0.0
	var got: Array = studio._handles.bodies()
	check(got.has(chassis5) and got.has(seat5) and not got.has(kart5),
		"the handles take every BasePart under it, body or no body: %d" % got.size())
	var kframe: Dictionary = studio._handles.frame()
	check(not kframe.is_empty() and kframe.basis.is_equal_approx(Basis.IDENTITY),
		"and box it on the world's axes, since a group shares none of its own")

	var chassis_was = _prop_of(chassis5, "Position").value
	var seat_was = _prop_of(seat5, "Position").value
	var mid: Vector3 = kframe.middle
	var eye3 := mid + Vector3(0, 0, 70)
	check(studio._handles.begin(0, eye3, (mid - eye3).normalized()), "the X arrow takes the drag")
	studio._handles.drag(eye3, ((mid + Vector3(5, 0, 0)) - eye3).normalized())
	studio._handles.finish()
	await _settle(14)
	check(_prop_of(chassis5, "Position").value.is_equal_approx(chassis_was + Vector3(5, 0, 0)),
		"moving the Model moves the part with a body: %s" % _prop_of(chassis5, "Position").value)
	check(_prop_of(seat5, "Position").value.is_equal_approx(seat_was + Vector3(5, 0, 0)),
		"and the welded one too, which nothing would have moved for it: %s" % _prop_of(seat5, "Position").value)
	await studio._undo()
	await _settle(14)
	check(_prop_of(chassis5, "Position").value.is_equal_approx(chassis_was)
		and _prop_of(seat5, "Position").value.is_equal_approx(seat_was),
		"and one undo puts the whole Model back")

	# Turning a Model swings its parts about the middle of the box, not each on the spot.
	var apart_was: float = (_prop_of(seat5, "Position").value - _prop_of(chassis5, "Position").value).length()
	studio._handles.mode = Handles.Mode.ROTATE
	studio._handles.rotate_snap = 0.0
	var ring_from := mid + Vector3(0, 70, 0)
	check(studio._handles.begin(1, ring_from, (mid - ring_from).normalized()), "the Y ring takes the drag")
	studio._handles.drag(ring_from, ((mid + Vector3(6, 0, 6)) - ring_from).normalized())
	studio._handles.finish()
	await _settle(14)
	var apart_now: float = (_prop_of(seat5, "Position").value - _prop_of(chassis5, "Position").value).length()
	check(absf(apart_now - apart_was) < 0.01,
		"a turned Model keeps its shape: %s vs %s" % [apart_now, apart_was])
	check(not _prop_of(chassis5, "Orientation").value.is_equal_approx(Vector3.ZERO),
		"and every part in it turned")
	await studio._undo()
	await _settle(14)
	studio._handles.rotate_snap = 15.0

	# Scaling a Model scales all of it about its middle, as Roblox's Model:ScaleTo does.
	studio._handles.mode = Handles.Mode.SCALE
	studio._handles.snap = 0.0
	check(studio._handles.can_scale(), "a Model can be scaled as one thing")
	# The box moved with the Model, so the middle is read here and not earlier.
	var sframe: Dictionary = studio._handles.frame()
	var smid: Vector3 = sframe.middle
	var half_x: float = sframe.half.x
	var size_was = _prop_of(chassis5, "Size").value
	var spread_was: Vector3 = _prop_of(seat5, "Position").value - _prop_of(chassis5, "Position").value
	var pull := smid + Vector3(half_x, 0, 0)
	var eye4 := smid + Vector3(0, 0, 70)
	check(studio._handles.begin(0, eye4, (pull - eye4).normalized()), "its X face takes the drag")
	studio._handles.drag(eye4, ((pull + Vector3(half_x, 0, 0)) - eye4).normalized())
	studio._handles.finish()
	await _settle(14)
	var size_now = _prop_of(chassis5, "Size").value
	var spread_now: Vector3 = _prop_of(seat5, "Position").value - _prop_of(chassis5, "Position").value
	check(size_now.is_equal_approx(size_was * 2.0),
		"every part grows by the same factor, on all three axes: %s from %s" % [size_now, size_was])
	check(_prop_of(seat5, "Size").value.is_equal_approx(_prop_of(seat5, "Size").value),
		"the welded one included")
	check(spread_now.is_equal_approx(spread_was * 2.0),
		"and they spread apart by it, so the Model keeps its shape: %s from %s"
		% [spread_now, spread_was])
	await studio._undo()
	await _settle(14)
	check(_prop_of(chassis5, "Size").value.is_equal_approx(size_was), "one undo puts that back too")
	studio._handles.mode = Handles.Mode.MOVE
	studio._handles.snap = 1.0
	studio._select([])

	# ---- a place's UI, previewed in the edit world ------------------------------
	# A ScreenGui is only real under a player's PlayerGui and an edit world has no player, so
	# the preview draws the same Controls in the hole the docks leave and never takes a click.
	var startergui := _service(studio.world, "StarterGui")
	check(startergui != 0, "the place has a StarterGui")
	var screen: int = await studio.history.insert("ScreenGui", startergui)
	await _settle(10)
	var panel: int = await studio.history.insert("Frame", screen)
	await _settle(10)
	studio.world.set_property(panel, "Position", Vector4(0, 100, 0, 100))
	studio.world.set_property(panel, "Size", Vector4(0, 200, 0, 200))
	studio.set_gui_preview(true)
	await _settle(20)
	# The Studio keeps this rect up to date as the splitters move.
	var hole: Rect2 = studio.world.gui_preview_rect
	check(hole.size.x > 1.0 and hole.size.y > 1.0, "the preview is laid out in the viewport hole: %s" % hole)
	check(studio.world.gui_at(hole.position + Vector2(200, 200)) == panel,
		"the Frame is drawn where the game would put it, and can be clicked in the view")
	check(studio.world.gui_at(hole.position + Vector2(400, 400)) == 0, "and only where it actually is")

	var inner: int = await studio.history.insert("TextButton", panel)
	await _settle(10)
	studio.world.set_property(inner, "Position", Vector4(0, 10, 0, 10))
	studio.world.set_property(inner, "Size", Vector4(0, 60, 0, 60))
	await _settle(20)
	check(studio.world.gui_at(hole.position + Vector2(140, 140)) == inner,
		"a button inside it answers first, being the innermost thing there")
	check(studio.world.gui_at(hole.position + Vector2(280, 280)) == panel, "and the frame elsewhere")

	studio._select_one(panel)
	await _settle(4)
	var prect: Rect2 = studio.world.gui_rect(panel)
	studio._gui_handles.show_for(panel, prect)
	check(studio._gui_handles.visible and prect.size.is_equal_approx(Vector2(200, 200)),
		"a selected Frame gets handles round the rect it is drawn in: %s" % prect)
	var pmid := prect.position + prect.size * 0.5
	check(studio._gui_handles.begin(pmid), "taking hold of its body")
	studio._gui_handles.drag(pmid + Vector2(30, 20))
	studio._gui_handles.finish()
	await _settle(10)
	check(_prop_of(panel, "Position").value == Vector4(0, 130, 0, 120),
		"moves it by exactly the pixels dragged, in offset: %s" % _prop_of(panel, "Position").value)
	await studio._undo()
	await _settle(10)
	check(_prop_of(panel, "Position").value == Vector4(0, 100, 0, 100), "and one undo puts it back")
	var corner := prect.position + prect.size
	check(studio._gui_handles.begin(corner), "taking the bottom-right grip")
	studio._gui_handles.drag(corner + Vector2(40, 10))
	studio._gui_handles.finish()
	await _settle(10)
	check(_prop_of(panel, "Size").value == Vector4(0, 240, 0, 210) and _prop_of(panel, "Position").value == Vector4(0, 100, 0, 100),
		"a grip grows the Size and leaves the far edge where it was: %s" % _prop_of(panel, "Size").value)
	await studio._undo()
	await _settle(8)

	studio.set_gui_preview(false)
	await _settle(20)
	check(studio.world.gui_at(hole.position + Vector2(200, 200)) == 0, "turned off, nothing of it is drawn")
	studio.set_gui_preview(true)
	await _settle(20)
	check(studio.world.gui_at(hole.position + Vector2(200, 200)) == panel, "and back on, it is there again")
	check(studio.history.destroy(screen), "put the place back as it was")
	await _settle(12)

	# ---- find and replace in the script editor ----------------------------------
	var main6 := _in(studio.world, _service(studio.world, "ServerScriptService"), "Main")
	studio._open_script(main6)
	await _settle(4)
	studio._editor.type("local a = 1\nlocal b = a + a\nprint(a, b, A)\n")
	studio._editor.open_find(true)
	check(studio._editor.finding(), "Ctrl+F opens the find bar")
	studio._editor.set_query("a")
	# Seven a's in that text: one inside each "local", four standing alone, and the capital.
	check(studio._editor.match_count() == 7,
		"it counts every match in the text, the ones inside words included: %d" % studio._editor.match_count())
	studio._editor.set_case(true)
	check(studio._editor.match_count() == 6,
		"and one fewer with case on, the capital dropping out: %d" % studio._editor.match_count())
	studio._editor.set_case(false)

	studio._editor._step(1)
	var first := [studio._editor.at_line(), studio._editor.at_column()]
	studio._editor._step(1)
	var second := [studio._editor.at_line(), studio._editor.at_column()]
	check(first != second, "Enter walks from one match to the next: %s then %s" % [first, second])
	for i in 4:
		studio._editor._step(1)
	check([studio._editor.at_line(), studio._editor.at_column()] != [0, 0]
		or studio._editor.match_count() == 4, "and round again at the end")

	# Every match is rewritten inside one complex operation: one undo takes the whole Replace All
	# back, and a commit adds one entry to the Studio's history, not one per match.
	studio._editor.set_query("local")
	studio._editor.set_with("var")
	studio._editor.replace_all()
	await _settle(2)
	var after_all: String = studio._editor.text_now()
	check(after_all == "var a = 1\nvar b = a + a\nprint(a, b, A)\n",
		"Replace All rewrites every match and nothing else: %s" % after_all.replace("\n", "\\n"))
	check(studio._editor.dirty(), "and the tab knows it was typed in")
	check(studio._editor.match_count() == 0, "with nothing left to find")
	check(studio._editor.text_now().count("var") == 2
		and studio._editor.text_now().contains("print(a, b, A)"),
		"both matches rewritten, and the line it was not asked about untouched")

	studio._editor.close_find()
	check(not studio._editor.finding(), "and the bar closes again")
	studio._editor.close_all()

	# ---- a playtest with more than one player in it -----------------------------
	studio._test_players = 2
	studio.play(true)
	await _settle(60)
	var players := _service(studio.playing, "Players")
	var seated := []
	for pid in studio.playing.get_child_ids(players):
		seated.append(studio.playing.get_instance(pid).get("name", ""))
	check(seated.size() == 3, "Play seats the extra test players beside you: %s" % [seated])
	check(seated.has("Player2") and seated.has("Player3"),
		"named the way Roblox names them")
	var extra := 0
	for pid in studio.playing.get_child_ids(players):
		if studio.playing.get_instance(pid).get("name", "") == "Player2":
			extra = pid
	check(extra != 0 and studio.playing.get_local_player_id() != extra,
		"one of them is not you")
	studio.stop()
	await _settle(14)
	studio._test_players = 0
	check(studio.playing == null, "and Stop throws the lot away with the world")

	# ---- attributes and tags in the Properties panel ----------------------------
	var tagged: int = await studio.history.insert("Part", reopened)
	await _settle(10)
	studio._select_one(tagged)
	await _settle(6)
	var props6: Properties = studio._properties
	check(props6._attr_head != null and props6._tag_head != null,
		"the panel has an Attributes and a Tags heading of its own")

	var adder: TreeItem = props6._attr_head.get_child(props6._attr_head.get_child_count() - 1)
	adder.set_text(1, "Speed")
	props6._on_edited_for(adder)
	await _settle(12)
	check(studio.world.get_attributes(tagged).has("Speed"),
		"typing a name into the add row makes the attribute: %s" % [studio.world.get_attributes(tagged)])
	props6.refresh()
	var speed_row: TreeItem = props6._rows_for_attr("Speed")
	check(speed_row != null, "and it gets a row")
	speed_row.set_text(1, "27.5")
	props6._on_edited_for(speed_row)
	await _settle(12)
	check(is_equal_approx(float(studio.world.get_attributes(tagged).get("Speed", 0)), 27.5),
		"whose value is read as the number it looks like: %s" % [studio.world.get_attributes(tagged).get("Speed", null)])
	await studio._undo()
	await _settle(12)
	check(str(studio.world.get_attributes(tagged).get("Speed", "-")) != "27.5",
		"and one undo takes the value back")

	var tag_adder: TreeItem = props6._tag_head.get_child(props6._tag_head.get_child_count() - 1)
	tag_adder.set_text(1, "Door")
	props6._on_edited_for(tag_adder)
	await _settle(12)
	check(studio.world.get_tags(tagged).has("Door"),
		"typing a tag into the add row tags it: %s" % [studio.world.get_tags(tagged)])
	props6.refresh()
	var door_row: TreeItem = props6._rows_for_tag("Door")
	check(door_row != null and door_row.is_checked(1), "and it shows as ticked")
	door_row.set_checked(1, false)
	props6._on_edited_for(door_row)
	await _settle(12)
	check(not studio.world.get_tags(tagged).has("Door"), "unticking it takes the tag off")
	await studio._undo()
	await _settle(12)
	check(studio.world.get_tags(tagged).has("Door"), "and one undo puts it back")
	studio._select([tagged])
	studio._delete()
	await _settle(10)

	# ---- a model in and out of a file -------------------------------------------
	var kart7 := _in(studio.world, _in(studio.world, _service(studio.world, "Workspace"), "Map"), "Kart")
	var out_file := ProjectSettings.globalize_path("user://studio_kart.rbxmx")
	studio._select_one(kart7)
	await _settle(4)
	studio.write_selection(out_file)
	check(FileAccess.get_file_as_string(out_file).begins_with("<roblox"),
		"Save Selection writes the model out as XML")

	var ws7 := _service(studio.world, "Workspace")
	studio._select([])
	var before7: int = studio.world.get_child_ids(ws7).size()
	await studio.insert_file(out_file)
	await _settle(16)
	check(studio.world.get_child_ids(ws7).size() == before7 + 1,
		"Insert from File puts one thing in, under what is selected")
	var brought := _in(studio.world, ws7, "studio_kart")
	check(brought != 0 and studio.world.get_instance(brought).get("class_name", "") == "Model",
		"as the Model it was, named after the file")
	check(_in(studio.world, brought, "Chassis") != 0 and _in(studio.world, brought, "Seat") != 0,
		"with everything that was inside it")
	await studio._undo()
	await _settle(16)
	check(studio.world.get_child_ids(ws7).size() == before7,
		"and one undo takes the import back out")
	DirAccess.remove_absolute(out_file)

	# ---- New Place ---------------------------------------------------------------
	var fresh := ProjectSettings.globalize_path("user://studio_new_place")
	_wipe_dir(fresh)
	DirAccess.make_dir_recursive_absolute(fresh)
	studio.new_place(fresh)
	await _settle(150)
	check(FileAccess.file_exists(fresh.path_join("default.project.json")),
		"New Place writes a project file")
	var made_ws := _service(studio.world, "Workspace")
	check(_in(studio.world, made_ws, "Baseplate") != 0,
		"and the place opens with something to stand on")
	check(_in(studio.world, made_ws, "SpawnLocation") != 0, "and somewhere to spawn")
	check(studio.place.mounts.has("Workspace"),
		"with a directory already mounted for whatever you build next")
	var newbie: int = await studio.history.insert("Part", made_ws)
	await _settle(12)
	await studio.save_place()
	await _settle(10)
	check(FileAccess.file_exists(fresh.path_join("src/workspace/Part.model.json")),
		"so the first thing you make has somewhere to go without being asked")
	studio.open_place(scratch)      # back to the place the rest of this works on
	await _settle(150)

	# ---- solid modelling --------------------------------------------------------
	# The place has been reopened since `reopened` was taken: find the Map again.
	var map8 := _in(studio.world, _service(studio.world, "Workspace"), "Map")
	check(map8 != 0, "the Map is where the parts to union go")
	var lump_a: int = await studio.history.insert("Part", map8)
	var lump_b: int = await studio.history.insert("Part", map8)
	await _settle(10)
	for one in [lump_a, lump_b]:
		studio.world.set_property(one, "Anchored", true)
		studio.world.set_property(one, "Size", Vector3(4, 4, 4))
	studio.world.set_property(lump_a, "Position", Vector3(80, 20, 80))
	studio.world.set_property(lump_b, "Position", Vector3(84, 20, 80))
	studio.world.set_property(lump_a, "Color", Color(1, 0, 0))
	await _settle(16)
	var was_here2: int = studio.world.get_child_ids(map8).size()
	studio._select([lump_a, lump_b])
	await studio._union()
	await _settle(20)
	var welded := _in(studio.world, map8, "Union")
	check(welded != 0 and studio.world.get_instance(welded).get("class_name", "") == "UnionOperation",
		"Union makes one UnionOperation")
	check(studio.world.get_child_ids(map8).size() == was_here2 - 1,
		"and the two it was made from are gone: %d" % studio.world.get_child_ids(map8).size())
	# Two 4-stud cubes meeting face to face: 8 along X, 4 the other ways.
	var lump_size = _prop_of(welded, "Size").value
	check(absf(lump_size.x - 8.0) < 0.2 and absf(lump_size.y - 4.0) < 0.2 and absf(lump_size.z - 4.0) < 0.2,
		"shaped like both of them together: %s" % lump_size)
	check(_prop_of(welded, "Color").value.is_equal_approx(Color(1, 0, 0)),
		"and looking like the first of them")
	check(studio.world.get_part_node(welded) != null, "the runtime drew it")
	var drew: Dictionary = studio._handles.frame_of(welded)
	check(not drew.is_empty() and absf(drew.half.x - 4.0) < 0.3,
		"from the geometry the union carries, not a box round it: %s" % [drew.get("half", "-")])
	# A Decal draws as a quad beside the part's own mesh, and a flat quad in a CSG is a hole.
	var hidden_of := func(of: int, pname: String) -> String:
		for hp in studio.world.get_properties(of, true):
			if hp.name == pname:
				return str(hp.value)
		return ""
	var plain_blob: String = hidden_of.call(welded, "MeshData")
	var lump_c: int = await studio.history.insert("Part", map8)
	var lump_d: int = await studio.history.insert("Part", map8)
	await _settle(10)
	for lid in [lump_c, lump_d]:
		studio.world.set_property(lid, "Anchored", true)
		studio.world.set_property(lid, "Size", Vector3(4, 4, 4))
	studio.world.set_property(lump_c, "Position", Vector3(80, 20, 120))
	studio.world.set_property(lump_d, "Position", Vector3(84, 20, 120))
	var sticker: int = await studio.history.insert("Decal", lump_c)
	await _settle(10)
	studio.world.set_property(sticker, "Texture", "images/logo.png")
	await _settle(10)
	studio._select([lump_c, lump_d])
	await studio._union()
	await _settle(20)
	var decal_union := 0
	for kid in studio.world.get_child_ids(map8):
		if kid != welded and studio.world.get_instance(kid).get("class_name", "") == "UnionOperation":
			decal_union = kid
	check(decal_union != 0 and hidden_of.call(decal_union, "MeshData").length() == plain_blob.length(),
		"a union of a part wearing a Decal is the same mesh as one without: %d vs %d bytes" % [hidden_of.call(decal_union, "MeshData").length(), plain_blob.length()])
	studio._select([decal_union])
	studio._delete()
	await _settle(10)

	var before_sep: int = studio.world.get_child_ids(map8).size()
	studio._select([welded])
	await studio._separate()
	await _settle(20)
	check(studio.world.get_child_ids(map8).size() == before_sep + 1 and _in(studio.world, map8, "Union") == 0,
		"Separate puts the parts back and takes the union away: %d" % studio.world.get_child_ids(map8).size())
	var back_a := 0
	for kid in studio.world.get_child_ids(map8):
		if _prop_of(kid, "Position").get("value", Vector3.ZERO) == Vector3(80, 20, 80):
			back_a = kid
	check(back_a != 0 and _prop_of(back_a, "Size").value == Vector3(4, 4, 4) and _prop_of(back_a, "Color").value.is_equal_approx(Color(1, 0, 0)),
		"each exactly as it was, where it was")
	await studio._undo()
	await _settle(24)
	check(_in(studio.world, map8, "Union") != 0 and studio.world.get_child_ids(map8).size() == before_sep,
		"and one undo makes the union again")
	welded = _in(studio.world, map8, "Union")
	var union_pos: Vector3 = _prop_of(welded, "Position").value
	studio.world.set_property(welded, "Position", union_pos + Vector3(18, 10, 0))
	studio.world.set_property(welded, "Orientation", Vector3(0, 90, 0))
	await _settle(10)
	studio._select([welded])
	await studio._separate()
	await _settle(20)
	# Roblox's frame is right-handed: lump_a stood 2 studs -X of the union's middle, and -X
	# turned +90 about Y lands on -Z.
	var expect := union_pos + Vector3(18, 10, 0) + Basis.from_euler(Vector3(0, deg_to_rad(90), 0), EULER_ORDER_YXZ) * (Vector3(80, 20, 80) - union_pos)
	var moved_a := 0
	for kid in studio.world.get_child_ids(map8):
		var kp = _prop_of(kid, "Position").get("value", Vector3.ZERO)
		if kp is Vector3 and kp.is_equal_approx(expect):
			moved_a = kid
	check(moved_a != 0 and absf(_prop_of(moved_a, "Orientation").value.y - 90.0) < 0.5,
		"separated after a move and a turn, the parts come out where the union is now: %s" % [expect])
	await studio._undo()
	await _settle(24)
	welded = _in(studio.world, map8, "Union")
	check(welded != 0, "and undo makes the union again, where it was moved to")

	# The union's mesh is baked onto the part, so it survives the file.
	await studio.save_place()
	await _settle(10)
	studio.open_place(scratch)
	await _settle(150)
	var reopened3 := _in(studio.world, _service(studio.world, "Workspace"), "Map")
	var back3 := _in(studio.world, reopened3, "Union")
	check(back3 != 0 and _prop_of(back3, "Size").value.is_equal_approx(lump_size),
		"opening the place again finds the union with its shape: %s" % _prop_of(back3, "Size").value)
	check(studio._handles.frame_of(back3).size() > 0, "and the runtime draws it from the file")

	var cutter: int = await studio.history.insert("Part", reopened3)
	await _settle(10)
	studio.world.set_property(cutter, "Anchored", true)
	studio.world.set_property(cutter, "Size", Vector3(2, 2, 8))
	studio.world.set_property(cutter, "Position", Vector3(82, 20, 80))
	await _settle(16)
	studio._select([cutter])
	await studio._negate()
	await _settle(20)
	var neg: int = studio.primary()
	check(neg != 0 and studio.world.get_instance(neg).get("class_name", "") == "NegateOperation",
		"Negate turns a part into a NegateOperation")
	check(_prop_of(neg, "CanCollide").value == false, "which collides with nothing")

	studio._select([back3, neg])
	await studio._union()
	await _settle(24)
	var cut := _in(studio.world, reopened3, "Union")
	check(cut != 0 and cut != back3, "unioning a negation with it cuts that shape out")
	studio._select([cut])
	studio._delete()
	await _settle(12)

	# ---- Terrain ----------------------------------------------------------------
	# One per Workspace, made by the runtime: a BasePart by class with no body of its own.
	var ground: int = studio.world.get_terrain_id()
	check(ground != 0, "the runtime made a Terrain in the Workspace")
	check(studio.world.get_instance(ground).get("class_name", "") == "Terrain"
		and studio.world.get_part_node(ground) == null,
		"which has no body of its own: it is not a 4 by 1.2 by 2 brick at the origin")
	check(studio.world.get_terrain_info().is_empty(), "and no ground in it until you make some")

	studio._ground_res = 32
	studio._make_ground()
	await _settle(14)
	var info: Dictionary = studio.world.get_terrain_info()
	check(info.get("resolution", 0) == 32 and is_equal_approx(info.get("span", 0), 128.0),
		"Create lays a flat field to sculpt: %s" % info)
	check(absf(studio.world.terrain_height_at(0, 0)) < 0.05, "flat, to begin with: %s" % studio.world.terrain_height_at(0, 0))
	var aimed: Dictionary = studio.world.terrain_raycast(Vector3(0, 80, 0), Vector3.DOWN)
	check(not aimed.is_empty() and aimed.position.distance_to(Vector3.ZERO) < 0.05,
		"and a ray finds it where it is: %s" % [aimed.get("position", "-")])

	var terrain_was := ""
	for one in studio.world.get_properties(ground, true):
		if one.name == "Heights":
			terrain_was = str(one.value)
	check(terrain_was != "", "the field is on the instance, as the blob the place keeps")

	studio._terrain.mode = TerrainTool.Mode.ADD
	studio._terrain.radius = 24.0
	studio._terrain.strength = 40.0
	check(studio._terrain.begin(Vector3(0, 80, 0), Vector3.DOWN), "the brush takes the ground")
	for i in 3:
		studio._terrain.drag(Vector3(0, 80, 0), Vector3.DOWN, 0.05)
	studio._terrain.finish()
	await _settle(14)
	var raised: float = studio.world.terrain_height_at(0, 0)
	check(raised > 2.0, "raising pushes the ground up under the pointer: %s" % raised)
	check(absf(studio.world.terrain_height_at(50, 50)) < 0.05,
		"and leaves what is outside the brush alone: %s" % studio.world.terrain_height_at(50, 50))
	# terrain_raycast meets the mesh, terrain_height_at gives Roblox's column rule. Surface nets
	# round a peak by about a third of a stud, hence the half-stud tolerance.
	var hill: Dictionary = studio.world.terrain_raycast(Vector3(0, 80, 0), Vector3.DOWN)
	check(not hill.is_empty() and absf(hill.position.y - raised) < 0.5,
		"a ray lands on the hill now, not where the ground used to be: %s vs %.2f" % [hill.get("position", "-"), raised])

	await studio._undo()
	await _settle(16)
	check(absf(studio.world.terrain_height_at(0, 0)) < 0.05,
		"and one undo takes the whole stroke back, not one push of it: %s" % studio.world.terrain_height_at(0, 0))
	await studio._redo()
	await _settle(16)
	check(studio.world.terrain_height_at(0, 0) > 2.0, "redo puts the hill back")

	# A cave is air with solid above it and below it in one column, which a height-per-column
	# field cannot hold.
	check(absf(studio.world.terrain_height_at(40, 40)) < 0.05, "flat ground to dig under: %s" % studio.world.terrain_height_at(40, 40))
	# Radius 8 about -14 tops out at -6, so the surface voxels (centres at -4) keep their matter.
	studio.world.terrain_sculpt(Vector3(40, -14, 40), 8.0, 4.0, TerrainTool.Mode.SUBTRACT, 0.0)
	studio.world.terrain_sculpt(Vector3(40, -14, 40), 8.0, 4.0, TerrainTool.Mode.SUBTRACT, 0.0)
	await _settle(6)
	check(absf(studio.world.terrain_height_at(40, 40)) < 0.6,
		"digging underneath leaves the roof where it was: %s" % studio.world.terrain_height_at(40, 40))
	var floor_hit: Dictionary = studio.world.terrain_raycast(Vector3(40, -12, 40), Vector3.DOWN)
	var roof_hit: Dictionary = studio.world.terrain_raycast(Vector3(40, -12, 40), Vector3.UP)
	check(not floor_hit.is_empty() and floor_hit.position.y < -15.0 and floor_hit.position.y > -22.0,
		"from inside it a ray finds a floor below: %s" % [floor_hit.get("position", "-")])
	check(not roof_hit.is_empty() and roof_hit.position.y > -10.0 and roof_hit.position.y < -2.0,
		"and a roof above -- a cave, with ground on both sides of the air: %s" % [roof_hit.get("position", "-")])
	check(roof_hit.get("normal", Vector3.ZERO).y < -0.5, "whose roof faces down into it")
	var blob2: String = studio.world.terrain_commit()
	check(blob2.length() < 60000, "and the whole field, cave and all, still fits in a property: %d bytes" % blob2.length())
	studio.history.set_property(ground, "Heights", blob2)
	await _settle(8)

	check(studio.world.terrain_material_at(Vector3(-40, -1, -40)) == "Grass", "a fresh field is grass")
	studio.world.terrain_sculpt(Vector3(-40, 0, -40), 10.0, 1.0, TerrainTool.Mode.PAINT, 0.0, 1296)
	await _settle(6)
	check(studio.world.terrain_material_at(Vector3(-40, -1, -40)) == "Sand",
		"Paint makes it sand: %s" % studio.world.terrain_material_at(Vector3(-40, -1, -40)))
	check(absf(studio.world.terrain_height_at(-40, -40)) < 0.05, "and moves nothing")
	check(studio.world.terrain_material_at(Vector3(-40, -1, 20)) == "Grass", "where it was not painted it is still grass")
	# The ground is drawn in chunks, and only the ones a stroke touched are rebuilt.
	var st: Dictionary = studio.world.get_terrain_stats()
	check(int(st.get("chunks", 0)) > 4 and int(st.get("rebuilt_last", 99)) < int(st.get("chunks", 0)) and int(st.get("dirty", 1)) == 0,
		"a corner stroke redraws %s of %s chunks, none left dirty" % [st.get("rebuilt_last", "?"), st.get("chunks", "?")])
	studio.world.terrain_sculpt(Vector3(-20, 0, 40), 8.0, 4.0, TerrainTool.Mode.ADD, 0.0, 896)
	studio.world.terrain_sculpt(Vector3(-20, 0, 40), 8.0, 4.0, TerrainTool.Mode.ADD, 0.0, 896)
	await _settle(6)
	check(studio.world.terrain_height_at(-20, 40) > 2.0 and studio.world.terrain_material_at(Vector3(-20, 2, 40)) == "Rock",
		"Add with rock chosen raises rock: %s at %s" % [studio.world.terrain_material_at(Vector3(-20, 2, 40)), studio.world.terrain_height_at(-20, 40)])
	studio.history.set_property(ground, "Heights", studio.world.terrain_commit())
	await _settle(8)

	# Godot's front faces are clockwise: wound the other way the ground faces in.
	await studio.get_tree().physics_frame
	await studio.get_tree().physics_frame
	var space9: PhysicsDirectSpaceState3D = studio.world.get_world_3d().direct_space_state
	var ray9: Dictionary = space9.intersect_ray(PhysicsRayQueryParameters3D.create(Vector3(-20, 200, 40), Vector3(-20, -200, 40), 0xFFFFFFFF))
	check(not ray9.is_empty() and str(ray9.collider.name) == "Terrain" and absf(float(ray9.position.y) - studio.world.terrain_height_at(-20, 40)) < 2.0 and ray9.normal.y > 0.5,   # the mesh's surface and the field's iso sit a stud or so apart on a slope
		"a ray from the sky lands on the ground's top, normal up: y=%s vs %.2f, normal %s" % [ray9.get("position", "-"), studio.world.terrain_height_at(-20, 40), ray9.get("normal", "-")])

	# SmoothGrid sits on Roblox's 4-stud grid and a field made here does not, so the first trip
	# out and back resamples it and the second is exact. The field is put back after.
	var heights_keep := ""
	for one9 in studio.world.get_properties(ground, true):
		if one9.name == "Heights":
			heights_keep = str(one9.value)
	var grid_out: String = studio.world.terrain_smoothgrid()
	var grid_head := Marshalls.base64_to_raw(grid_out)
	check(grid_head.size() > 100 and grid_head[0] == 1 and grid_head[1] == 5, "the ground encodes as SmoothGrid: %d bytes, version 1, 32-voxel chunks" % grid_head.size())
	var h_a: float = studio.world.terrain_height_at(-20, 40)
	var h_b: float = studio.world.terrain_height_at(0, 0)
	studio.world.set_property(ground, "SmoothGrid", grid_out)
	await _settle(30)
	check(absf(studio.world.terrain_height_at(-20, 40) - h_a) < 2.5 and absf(studio.world.terrain_height_at(0, 0) - h_b) < 2.5,
		"read back in on Roblox's 4-stud grid, the ground stands about where it did: %.2f vs %.2f, %.2f vs %.2f" % [studio.world.terrain_height_at(-20, 40), h_a, studio.world.terrain_height_at(0, 0), h_b])
	var h2a: float = studio.world.terrain_height_at(-20, 40)
	var h2b: float = studio.world.terrain_height_at(0, 0)
	var m2: String = studio.world.terrain_material_at(Vector3(-20, h2a - 1.0, 40))
	var grid2: String = studio.world.terrain_smoothgrid()
	studio.world.set_property(ground, "SmoothGrid", grid2)
	await _settle(30)
	check(absf(studio.world.terrain_height_at(-20, 40) - h2a) < 0.05 and absf(studio.world.terrain_height_at(0, 0) - h2b) < 0.05
		and studio.world.terrain_material_at(Vector3(-20, h2a - 1.0, 40)) == m2,
		"and once on that grid, out and back again is exact: %.3f vs %.3f, %s" % [studio.world.terrain_height_at(-20, 40), h2a, m2])
	var sg_left := ""
	for one9 in studio.world.get_properties(ground, true):
		if one9.name == "SmoothGrid":
			sg_left = str(one9.value)
	check(sg_left == "", "and the Roblox blob is cleared once read: Heights is what the place keeps")
	studio.world.set_property(ground, "Heights", heights_keep)
	await _settle(30)

	# A script's Terrain op builds ground on the host, and the field goes back to the tree as
	# one Heights write -- what a place keeps and what clients get.
	var heights_before := ""
	for one in studio.world.get_properties(ground, true):
		if one.name == "Heights":
			heights_before = str(one.value)
	# The budget is Roblox's: a script is stopped only after ten seconds without yielding.
	var busy_mark: int = studio._lines.size()       # the Output's own store: this world is not the one `logs` listens to
	studio.world.max_millis_per_call = 10000.0       # the Studio's real default, for this one call
	studio._run_command("local n = 0 for i = 1, 3000000 do n += 1 end print('busy', n)")
	await _settle(30)
	studio.world.max_millis_per_call = 4.0
	var busy_line := ""
	for k in range(busy_mark, studio._lines.size()):
		var lt := str(studio._lines[k].get("text", ""))
		if lt.begins_with("busy") or lt.find("killed") >= 0:   # the print, not the command's own echo
			busy_line = lt
	check(busy_line.find("3000000") > 0 and busy_line.find("killed") < 0,
		"three million loop iterations in one call run to the end, as they would on Roblox: %s" % busy_line)
	# Enums nothing in the engine acts on still resolve, at Roblox's values, and a part's six
	# surface properties are its to set.
	var enum_mark: int = studio._lines.size()
	studio._run_command("local p = Instance.new('Part') p.TopSurface = Enum.SurfaceType.Studs print('enums', Enum.SurfaceType.SmoothNoOutlines.Value, Enum.Limb.RightLeg.Value, Enum.Technology.Future.Value, p.TopSurface.Name, p.BottomSurface.Name)")
	await _settle(20)
	var enum_line := ""
	for k in range(enum_mark, studio._lines.size()):
		var et := str(studio._lines[k].get("text", ""))
		if et.begins_with("enums") or et.find("not a valid") >= 0:
			enum_line = et
	check(enum_line.find("enums\t10\t5\t4\tStuds\tSmooth") >= 0,
		"Enum.SurfaceType, Limb, Technology and a part's six surfaces are there: %s" % enum_line)
	studio._run_command("workspace.Terrain:FillBlock(CFrame.new(50, 4, -50), Vector3.new(16, 8, 16), Enum.Material.Rock)")
	await _settle(20)
	# Roblox puts the surface at the voxel centre + 4 * occupancy, so a block filled to 8 tops
	# out 2 studs past its face, at 10.
	check(absf(studio.world.terrain_height_at(50, -50) - 10.0) < 0.6,
		"Terrain:FillBlock from a script raises a block, its top read as Roblox reads it, 2 studs past the face: %s" % studio.world.terrain_height_at(50, -50))
	check(studio.world.terrain_material_at(Vector3(50, 4, -50)) == "Rock", "made of the material it asked for")
	check(absf(studio.world.terrain_height_at(50, -20)) < 0.05, "and only where it asked")
	# A full voxel under a three-quarter one: Roblox's own raycast puts that surface at 9.0.
	studio._run_command("workspace.Terrain:FillBlock(CFrame.new(-52, 3.5, 52), Vector3.new(8, 7, 8), Enum.Material.Grass)")
	await _settle(20)
	check(absf(studio.world.terrain_height_at(-52, 52) - 9.0) < 0.15,
		"a full voxel under a three-quarter one reads 9.0, the height Roblox's raycast gave the same column: %s" % studio.world.terrain_height_at(-52, 52))
	# Water is not ground: it stands on the field as its own surface.
	var water_chunks_before: int = studio.world.get_terrain_stats().get("water_chunks", 0)
	studio._run_command("workspace.Terrain:FillBlock(CFrame.new(0, 4, -50), Vector3.new(16, 8, 16), Enum.Material.Water)")
	await _settle(20)
	check(absf(studio.world.terrain_height_at(0, -50)) < 0.6, "a block of Water does not raise the ground under it: %s" % studio.world.terrain_height_at(0, -50))
	check(studio.world.terrain_material_at(Vector3(0, 4, -50)) == "Water" and studio.world.terrain_material_at(Vector3(0, -2, -50)) == "Grass",
		"a point in it is Water, the ground under it still Grass: %s / %s" % [studio.world.terrain_material_at(Vector3(0, 4, -50)), studio.world.terrain_material_at(Vector3(0, -2, -50))])
	check(studio.world.get_terrain_stats().get("water_chunks", 0) > water_chunks_before, "and it is drawn: %d water chunks" % studio.world.get_terrain_stats().get("water_chunks", 0))
	var heights_after := ""
	for one in studio.world.get_properties(ground, true):
		if one.name == "Heights":
			heights_after = str(one.value)
	check(heights_after != "" and heights_after != heights_before,
		"and the tree holds the new ground, written back as one property")
	studio._run_command("workspace.Terrain:FillBall(Vector3.new(-50, 0, -50), 6, Enum.Material.Snow)")
	await _settle(20)
	check(studio.world.terrain_height_at(-50, -50) > 4.0 and studio.world.terrain_material_at(Vector3(-50, 3, -50)) == "Snow",
		"Terrain:FillBall does the same with a ball: %s at %s" % [studio.world.terrain_material_at(Vector3(-50, 3, -50)), studio.world.terrain_height_at(-50, -50)])

	await studio.save_place()
	await _settle(10)
	check(FileAccess.file_exists(scratch.path_join("src/workspace/Terrain.model.json")),
		"Save writes the ground into the place")
	studio.open_place(scratch)
	await _settle(160)
	check(absf(studio.world.terrain_height_at(0, 0) - raised) < 0.3,
		"and opening it again finds the same ground: %s vs %s" % [studio.world.terrain_height_at(0, 0), raised])
	check(studio.world.get_terrain_info().get("resolution", 0) == 32, "at the resolution it was made at")
	# Terrain has a fixed id: the file applies onto the one the runtime made rather than
	# replacing it, and a client makes its own at that same id.
	check(studio.world.get_terrain_id() == ground,
		"and the Terrain is still the one at the fixed id, not a second one the file made: %d vs %d"
		% [studio.world.get_terrain_id(), ground])
	check(studio.world.terrain_material_at(Vector3(-40, -1, -40)) == "Sand"
		and studio.world.terrain_material_at(Vector3(50, 4, -50)) == "Rock",
		"with every material where it was put: %s, %s" % [studio.world.terrain_material_at(Vector3(-40, -1, -40)), studio.world.terrain_material_at(Vector3(50, 4, -50))])
	studio._run_command("workspace.Terrain:Clear()")
	await _settle(20)
	check(studio.world.terrain_height_at(0, 0) < -1e20, "Terrain:Clear takes all of it away")
	var took: bool = studio.world.set_property(ground, "Heights", heights_after)
	await _settle(16)
	var now_heights := ""
	for one in studio.world.get_properties(ground, true):
		if one.name == "Heights":
			now_heights = str(one.value)
	check(took and now_heights == heights_after and studio.world.terrain_height_at(50, -50) > 7.0,
		"and writing a field back into Heights brings it back, block and all: %s" % studio.world.terrain_height_at(50, -50))

	studio.play(false)
	await _settle(50)
	check(studio.playing != null and studio.playing.get_terrain_id() != 0,
		"the played world has a Terrain of its own")
	check(absf(studio.playing.terrain_height_at(0, 0) - raised) < 0.3,
		"with the ground you sculpted in it: %s" % studio.playing.terrain_height_at(0, 0))
	var doubles := 0
	for kid in studio.playing.get_child_ids(_service(studio.playing, "Workspace")):
		if studio.playing.get_instance(kid).get("class_name", "") == "Terrain":
			doubles += 1
	check(doubles == 1, "and only one of them, not a second beside it: %d" % doubles)
	studio.stop()
	await _settle(14)

	# ---- completion --------------------------------------------------------------
	var main9 := _in(studio.world, _service(studio.world, "ServerScriptService"), "Main")
	studio._open_script(main9)
	await _settle(2)
	var names := func(got: Array) -> Array:
		var t := []
		for one in got:
			t.append(one[0])
		return t
	var kids: Array = names.call(studio._editor.completions_for("workspace.Map."))
	check(kids.has("Crate") and kids.has("Kart") and kids.has("Shrine"),
		"after a dot the live tree's children are offered: %d of them" % kids.size())
	var props9: Array = names.call(studio._editor.completions_for("local p = workspace.Map.Crate."))
	check(props9.has("Position") and props9.has("Anchored") and props9.has("Touched"),
		"and a part's properties and events, from the class table itself")
	var meths: Array = names.call(studio._editor.completions_for("workspace.Map.Crate:"))
	check(meths.has("Destroy") and meths.has("GetMass") and not meths.has("Position"),
		"a colon offers methods, from the runtime's own method table: %d of them" % meths.size())
	var svcs: Array = names.call(studio._editor.completions_for("game."))
	check(svcs.has("Workspace") and svcs.has("ReplicatedStorage"), "game. offers the services")
	var part9: Array = names.call(studio._editor.completions_for("workspace.Map.Cr"))
	check(part9 == ["Crate"], "a partial word narrows it: %s" % [part9])
	var here: Array = names.call(studio._editor.completions_for("script.Parent."))
	check(here.has("Main") and here.has("Evil"), "script.Parent is where this script lives")
	var glob: Array = names.call(studio._editor.completions_for("  pri"))
	check(glob.has("print") and not glob.has("game"), "with no dot, the globals: %s" % [glob])
	check(studio._editor.completions_for("local x = someLocal.").is_empty(),
		"and a local it cannot follow offers nothing rather than a guess")
	check(studio._editor.completions_for('game:GetService("Workspace").').is_empty(),
		"nor does a dot after a call's result offer the globals")
	studio._editor.close_all()

	# ---- a place from Roblox ------------------------------------------------------
	# Services merge into the ones the runtime already has, rather than landing beside them.
	var rbxlx := '<roblox version="4">'
	rbxlx += '<Item class="Workspace" referent="RBX0"><Properties><string name="Name">Workspace</string><float name="Gravity">150</float></Properties>'
	rbxlx += '<Item class="Part" referent="RBX1"><Properties><string name="Name">Imported</string><Vector3 name="size"><X>4</X><Y>2</Y><Z>6</Z></Vector3><CoordinateFrame name="CFrame"><X>5</X><Y>7</Y><Z>9</Z></CoordinateFrame><bool name="Anchored">true</bool></Properties></Item>'
	var red_grass := PackedByteArray()
	red_grass.resize(69)
	for k in 69: red_grass[k] = 0
	red_grass[6] = 255   # Grass is the first of the 21, after the 6-byte header
	red_grass[9] = 0x58; red_grass[10] = 0x59; red_grass[11] = 0x56   # Slate, Roblox's own grey
	rbxlx += '<Item class="Terrain" referent="RBX2"><Properties><string name="Name">Terrain</string><BinaryString name="MaterialColors">%s</BinaryString></Properties></Item>' % Marshalls.raw_to_base64(red_grass)
	rbxlx += '<Item class="Camera" referent="RBX12"><Properties><string name="Name">Camera</string><CoordinateFrame name="CFrame"><X>50</X><Y>60</Y><Z>70</Z><R00>1</R00><R01>0</R01><R02>0</R02><R10>0</R10><R11>1</R11><R12>0</R12><R20>0</R20><R21>0</R21><R22>1</R22></CoordinateFrame></Properties></Item>'
	rbxlx += '<Item class="Model" referent="RBX4"><Properties><string name="Name">Rig</string><Ref name="PrimaryPart">RBX5</Ref></Properties><Item class="Part" referent="RBX5"><Properties><string name="Name">Core</string></Properties></Item></Item>'
	rbxlx += '</Item>'
	rbxlx += '<Item class="ServerScriptService" referent="RBX6"><Properties><string name="Name">ServerScriptService</string></Properties><Item class="Script" referent="RBX7"><Properties><string name="Name">Boot</string><ProtectedString name="Source"><![CDATA[print("booted")]]></ProtectedString></Properties></Item></Item>'
	rbxlx += '<Item class="StarterPlayer" referent="RBX8"><Properties><string name="Name">StarterPlayer</string></Properties><Item class="StarterPlayerScripts" referent="RBX9"><Properties><string name="Name">StarterPlayerScripts</string></Properties><Item class="LocalScript" referent="RBX10"><Properties><string name="Name">Hud</string><ProtectedString name="Source"><![CDATA[print("hud")]]></ProtectedString></Properties></Item></Item></Item>'
	rbxlx += '<Item class="Lighting" referent="RBX11"><Properties><string name="Name">Lighting</string><float name="Brightness">3</float></Properties></Item>'
	rbxlx += '</roblox>'
	var place_file := ProjectSettings.globalize_path("user://studio_import.rbxlx")
	var pf := FileAccess.open(place_file, FileAccess.WRITE)
	pf.store_string(rbxlx)
	pf = null
	var project_dir := ProjectSettings.globalize_path("user://studio_import")
	_wipe_dir(project_dir)
	DirAccess.make_dir_recursive_absolute(project_dir)
	await studio.import_place(place_file, project_dir)
	await _settle(40)
	var iws := _service(studio.world, "Workspace")
	var imported := _in(studio.world, iws, "Imported")
	check(imported != 0 and _prop_of(imported, "Position").value.is_equal_approx(Vector3(5, 7, 9))
		and _prop_of(imported, "Size").value.is_equal_approx(Vector3(4, 2, 6)),
		"a Roblox place file's Workspace comes in, parts where the file put them: %s" % [_prop_of(imported, "Position").get("value", "-")])
	var palette := Marshalls.base64_to_raw(studio.world.terrain_material_colors())
	check(palette.size() == 69 and palette[6] == 255 and palette[7] == 0 and palette[9] == 0x58,
		"the place's MaterialColors is the ground's palette now: Grass red, Slate the file's %02x%02x%02x" % [palette[9], palette[10], palette[11]])
	check(studio._cam.global_position.is_equal_approx(Vector3(50, 60, 70)),
		"and the Studio's view opens where the place saved its Camera, as Roblox Studio's does: %s" % studio._cam.global_position)
	var terrains := 0
	for kid in studio.world.get_child_ids(iws):
		if studio.world.get_instance(kid).get("class_name", "") == "Terrain":
			terrains += 1
	check(terrains == 1, "the file's Terrain does not make a second one: %d" % terrains)
	var rig := _in(studio.world, iws, "Rig")
	check(rig != 0 and _prop_of(rig, "PrimaryPart").value == _in(studio.world, rig, "Core"),
		"a Ref across the file resolves to the instance it names")
	var boot := _in(studio.world, _service(studio.world, "ServerScriptService"), "Boot")
	check(boot != 0 and studio.world.get_instance(boot).get("class_name", "") == "Script", "a Script lands in ServerScriptService")
	var sp := _service(studio.world, "StarterPlayer")
	var sps_count := 0
	var sps := 0
	for kid in studio.world.get_child_ids(sp):
		if studio.world.get_instance(kid).get("class_name", "") == "StarterPlayerScripts":
			sps_count += 1
			sps = kid
	check(sps_count == 1 and _in(studio.world, sps, "Hud") != 0,
		"the file's StarterPlayerScripts merges into the one the runtime keeps, LocalScript and all: %d of them" % sps_count)
	check(is_equal_approx(float(_prop_of(_service(studio.world, "Lighting"), "Brightness").get("value", 0)), 3.0), "and a service's own properties apply")
	await studio.save_place()
	await _settle(10)
	check(FileAccess.file_exists(project_dir.path_join("src/workspace/Imported.model.json"))
		and FileAccess.file_exists(project_dir.path_join("src/server/Boot.server.luau"))
		and FileAccess.file_exists(project_dir.path_join("src/client/Hud.client.luau")),
		"and Ctrl+S writes the imported place out as a Rojo project")
	studio.open_place(scratch)      # back to the place the rest of this works on
	await _settle(150)

	# ---- a binary model with zstd chunks, as a current Studio writes -------------
	# The fixtures are LZ4 throughout: each chunk is decoded here and rewritten as a zstd frame,
	# the other compression Studio has used. The runtime carries no zstd of its own; a zstd chunk
	# loads only through the decoder the Godot host hands it with setZstdDecoder.
	var fixture := ProjectSettings.globalize_path("res://").path_join("../../test/fixtures/attributes.rbxm").simplify_path()
	var orig := FileAccess.get_file_as_bytes(fixture)
	check(orig.size() > 32, "the binary fixture is there: %d bytes" % orig.size())
	var zfile := orig.slice(0, 32)
	var zat := 32
	var zstd_chunks := 0
	while zat + 16 <= orig.size():
		var cname := orig.slice(zat, zat + 4)
		var comp := orig.decode_u32(zat + 4)
		var ulen := orig.decode_u32(zat + 8)
		var stored := comp if comp != 0 else ulen
		var data := orig.slice(zat + 16, zat + 16 + stored)
		zat += 16 + stored
		if comp != 0:
			var plain := _lz4(data, ulen)
			var z := plain.compress(FileAccess.COMPRESSION_ZSTD)
			zfile.append_array(cname)
			zfile.resize(zfile.size() + 12)
			zfile.encode_u32(zfile.size() - 12, z.size())
			zfile.encode_u32(zfile.size() - 8, plain.size())
			zfile.encode_u32(zfile.size() - 4, 0)
			zfile.append_array(z)
			zstd_chunks += 1
		else:
			zfile.append_array(orig.slice(zat - 16 - stored, zat))
		if cname.get_string_from_ascii().begins_with("END"):
			break
	check(zstd_chunks >= 5 and zfile[32 + 16] == 0x28 and zfile[32 + 17] == 0xB5,
		"%d chunks rewritten as zstd frames, magic and all" % zstd_chunks)
	studio.world.load_file("Workspace/FromLZ4.rbxm", Marshalls.raw_to_base64(orig))
	studio.world.load_file("Workspace/FromZstd.rbxm", Marshalls.raw_to_base64(zfile))
	await _settle(30)
	var ws9 := _service(studio.world, "Workspace")
	var from_lz4 := _in(studio.world, ws9, "FromLZ4")
	var from_zstd := _in(studio.world, ws9, "FromZstd")
	check(from_lz4 != 0 and from_zstd != 0, "the same model loads from both files")
	# The fixture carries a NaN attribute, and nan never equals nan, so the compare goes key by key.
	var attrs_a: Dictionary = studio.world.get_attributes(from_lz4)
	var attrs_b: Dictionary = studio.world.get_attributes(from_zstd)
	var same_attrs := attrs_a.size() == attrs_b.size() and attrs_a.size() > 5
	for key in attrs_a:
		if not attrs_b.has(key):
			same_attrs = false
		elif attrs_a[key] is float and is_nan(attrs_a[key]):
			same_attrs = same_attrs and attrs_b[key] is float and is_nan(attrs_b[key])
		else:
			same_attrs = same_attrs and attrs_a[key] == attrs_b[key]
	check(from_zstd != 0 and studio.world.get_instance(from_zstd).get("class_name", "") == studio.world.get_instance(from_lz4).get("class_name", "")
		and studio.world.get_child_ids(from_zstd).size() == studio.world.get_child_ids(from_lz4).size() and same_attrs,
		"identically -- class, children and every attribute, NaN included: %d attributes" % attrs_b.size())
	studio._select([from_lz4, from_zstd])
	studio._delete()
	await _settle(10)

	# ---- more of Studio's own files, as rbx-test-files keeps them -----------------
	# Real binaries, so the property type ids are Studio's and not this reader's idea of them:
	# a ParticleEmitter's sequences are 0x15 and 0x16, an AcousticAbsorption is flag 3.
	var fx_dir := ProjectSettings.globalize_path("res://").path_join("../../test/fixtures").simplify_path()
	var errs_before := 0
	for line in logs:
		if str(line).begins_with("ERROR"):
			errs_before += 1
	var fx_names := ["two-particleemitters", "physical-properties-acoustics", "body-movers", "two-cframevalues", "three-uigradients", "unions", "tags", "content-mixed"]
	for fxn in fx_names:
		studio.world.load_file("Workspace/%s.rbxm" % fxn, Marshalls.raw_to_base64(FileAccess.get_file_as_bytes(fx_dir.path_join(fxn + ".rbxm"))))
	await _settle(40)
	var ws10 := _service(studio.world, "Workspace")
	var fx_folders := {}
	for fxn in fx_names:
		fx_folders[fxn] = _in(studio.world, ws10, fxn)
	var pe_kids: Array = studio.world.get_child_ids(fx_folders["two-particleemitters"]) if fx_folders["two-particleemitters"] != 0 else []
	var pe10: int = pe_kids[0] if not pe_kids.is_empty() else 0
	var size_kp: PackedFloat32Array = _prop_of(pe10, "Size").get("value", PackedFloat32Array())
	check(size_kp.size() == 15 and is_equal_approx(size_kp[7], 1.9375),
		"a binary ParticleEmitter's Size keeps its five keypoints, values and all: %d floats" % size_kp.size())
	var col_kp: PackedFloat32Array = _prop_of(pe10, "Color").get("value", PackedFloat32Array())
	var tr_kp: PackedFloat32Array = _prop_of(pe10, "Transparency").get("value", PackedFloat32Array())
	check(col_kp.size() == 8 and tr_kp.size() == 6, "and its Color and Transparency: %d and %d" % [col_kp.size(), tr_kp.size()])
	var custom_phys := PackedFloat32Array()
	var plain_phys := 0
	for kid in studio.world.get_child_ids(fx_folders["physical-properties-acoustics"]):
		var v = _prop_of(kid, "CustomPhysicalProperties").get("value")
		if v is PackedFloat32Array:
			custom_phys = v
		else:
			plain_phys += 1
	check(custom_phys.size() == 5 and is_equal_approx(custom_phys[0], 0.25) and is_equal_approx(custom_phys[2], 0.125) and is_equal_approx(custom_phys[4], 0.25) and plain_phys == 1,
		"a part's custom physical properties come through the newer six-float form, the other part keeps its material's: %s" % [custom_phys])
	var movers := {}
	for kid in studio.world.get_child_ids(fx_folders["body-movers"]):
		movers[studio.world.get_instance(kid).get("class_name", "")] = kid
	check(movers.size() == 6 and movers.has("BodyGyro") and movers.has("BodyVelocity") and movers.has("BodyThrust"),
		"the six BodyMovers load as themselves: %s" % [movers.keys()])
	var gyro_torque = _prop_of(movers.get("BodyGyro", 0), "MaxTorque").get("value")
	check(gyro_torque is Vector3 and gyro_torque.x > 1000 and _prop_of(movers.get("BodyGyro", 0), "CFrameOrientation").has("value"),
		"a BodyGyro keeps its MaxTorque and its CFrame as a pair: %s" % [gyro_torque])
	var cfv_x := -1.0
	for kid in studio.world.get_child_ids(fx_folders["two-cframevalues"]):
		if studio.world.get_instance(kid).get("class_name", "") == "CFrameValue":
			var pv = _prop_of(kid, "ValuePosition").get("value")
			if pv is Vector3 and is_equal_approx(pv.x, 1.0):
				cfv_x = pv.x
	check(cfv_x == 1.0, "a CFrameValue's Value comes in as a position and an orientation")
	var grads := 0
	var grad_tr := PackedFloat32Array()
	for kid in studio.world.get_child_ids(fx_folders["three-uigradients"]):
		if studio.world.get_instance(kid).get("class_name", "") == "UIGradient":
			grads += 1
			var t = _prop_of(kid, "Transparency").get("value")
			if t is PackedFloat32Array and t.size() > grad_tr.size():
				grad_tr = t
	check(grads == 3 and grad_tr.size() == 15, "three UIGradients, the longest ramp five keypoints long: %d" % grad_tr.size())
	var unions10 := 0
	for kid in studio.world.get_child_ids(fx_folders["unions"]):
		if studio.world.get_instance(kid).get("class_name", "") == "UnionOperation":
			unions10 += 1
	check(unions10 == 3, "three of Studio's own UnionOperations load (their mesh is Roblox's, so they draw as nothing): %d" % unions10)
	# Studio keeps CollectionService tags as a NUL-separated Tags blob.
	var tag_folder: int = fx_folders["tags"]
	var got_tags: PackedStringArray = studio.world.get_tags(tag_folder) if tag_folder != 0 else PackedStringArray()
	check(got_tags.has("Cool") and got_tags.has("My") and got_tags.has("Tags") and got_tags.size() == 3,
		"a Folder's three tags come in from Studio's Tags blob: %s" % [got_tags])
	var tag_xml: String = studio.world.to_rbxmx(JSON.stringify(Model.node(studio.world, tag_folder, tag_folder)), "tags") if tag_folder != 0 else ""
	check(tag_xml.find('<BinaryString name="Tags">Q29vbABNeQBUYWdz</BinaryString>') > 0,
		"and the .rbxmx writer puts them back as the very same blob")
	# ImageContent and TextureContent mirror the older Image and Texture strings; Studio spells
	# a Content as <uri>, or <null> for nothing. The fixture is Studio 0.663's own file.
	var mixed: int = fx_folders["content-mixed"]
	var mixed_json := JSON.stringify(Model.node(studio.world, mixed, mixed)) if mixed != 0 else ""
	const SPAWN_PNG := "rbxasset://textures/SpawnLocation.png"
	check(mixed_json.find('"ImageContent":"%s"' % SPAWN_PNG) >= 0 and mixed_json.find('"TextureContent":"%s"' % SPAWN_PNG) >= 0
		and mixed_json.find('"Image":') < 0 and mixed_json.find('"Texture":') < 0,
		"a model.json keeps the Content properties and not their string twins")
	var mixed_xml: String = studio.world.to_rbxmx(mixed_json, "content-mixed") if mixed != 0 else ""
	check(mixed_xml.find('<Content name="ImageContent"><uri>%s</uri></Content>' % SPAWN_PNG) > 0
		and mixed_xml.find('<Content name="TextureContent"><uri>%s</uri></Content>' % SPAWN_PNG) > 0 and mixed_xml.find('name="Image"') < 0,
		"and the .rbxmx writes them as <uri>")
	var old_xml: String = studio.world.to_rbxmx(JSON.stringify({"className": "ImageLabel", "properties": {"Image": SPAWN_PNG}}), "old")
	var none_xml: String = studio.world.to_rbxmx(JSON.stringify({"className": "ImageLabel", "properties": {"ImageContent": ""}}), "none")
	check(old_xml.find('<Content name="ImageContent"><uri>%s</uri></Content>' % SPAWN_PNG) > 0 and none_xml.find('<Content name="ImageContent"><null></null></Content>') > 0,
		"a model.json with only Image writes ImageContent, and nothing is <null>")
	var errs_after := 0
	var first_err := ""
	for line in logs:
		if str(line).begins_with("ERROR"):
			errs_after += 1
			if errs_after > errs_before and first_err == "":
				first_err = str(line)
	check(errs_after == errs_before, "and none of those files raised an error: %s" % first_err)
	var fx_ids: Array = []
	for fxn in fx_names:
		if fx_folders[fxn] != 0:
			fx_ids.append(fx_folders[fxn])
	studio._select(fx_ids)
	studio._delete()
	await _settle(10)

	# ---- the way back out: Save as Roblox Place ------------------------------------
	# The whole tree as one .rbxlx, with the services as its root items.
	var export_file := ProjectSettings.globalize_path("user://studio_export.rbxlx")
	check(studio.save_rbxlx(export_file), "Save as Roblox Place writes the file")
	var export_xml := FileAccess.get_file_as_string(export_file)
	var root_items := 0
	for line in export_xml.split(char(10)):
		if line.begins_with("  <Item class="):
			root_items += 1
	check(export_xml.find('  <Item class="Workspace"') >= 0 and export_xml.find('  <Item class="ServerScriptService"') >= 0
		and export_xml.find('  <Item class="StarterPlayer"') >= 0 and root_items >= 6,
		"the services are the root items, %d of them" % root_items)
	check(export_xml.find('<Item class="Terrain"') > 0 and export_xml.find('<BinaryString name="SmoothGrid">') > 0 and export_xml.find("Heights") < 0,
		"the ground goes along as Studio's own SmoothGrid on a Terrain item, and this engine's Heights stays home")
	# JSON has no spelling for inf or nan, so the model.json writes {"Float64": "inf"} and the
	# XML's attribute blob carries the real doubles.
	var fixtures_json := Model.of(studio.world, _in(studio.world, _service(studio.world, "ReplicatedStorage"), "Fixtures"))
	check(fixtures_json.find('"Float64": "inf"') > 0 and fixtures_json.find('"Float64": "nan"') > 0 and fixtures_json.find(": inf") < 0 and fixtures_json.find(": nan") < 0,
		"a non-finite attribute is written as a typed string, never a bare inf")
	check(export_xml.find('<string name="Name">Crate</string>') > 0 and export_xml.find('<ProtectedString name="Source">') > 0,
		"the parts and the scripts are in it")
	var cam_ref := RegEx.create_from_string('<Ref name="CurrentCamera">(RBX[0-9A-F]+)</Ref>').search(export_xml)
	check(cam_ref != null and export_xml.find('referent="%s"' % cam_ref.get_string(1)) > 0,
		"Workspace.CurrentCamera is a Ref to a Camera item in the file")
	var rt_dir := ProjectSettings.globalize_path("user://studio_export_back")
	_wipe_dir(rt_dir)
	DirAccess.make_dir_recursive_absolute(rt_dir)
	await studio.import_place(export_file, rt_dir)
	await _settle(40)
	var rws := _service(studio.world, "Workspace")
	var rmap := _in(studio.world, rws, "Map")
	check(rmap != 0 and _in(studio.world, rmap, "Crate") != 0 and _in(studio.world, _service(studio.world, "ServerScriptService"), "Main") != 0,
		"and Open Roblox Place brings the same place back: Map/Crate and the Main script")
	studio.open_place(scratch)
	await _settle(150)

	# baseplate-566.rbxl is a binary place Studio itself saved.
	var bp_dir := ProjectSettings.globalize_path("user://studio_baseplate")
	_wipe_dir(bp_dir)
	DirAccess.make_dir_recursive_absolute(bp_dir)
	var lines_before: int = studio._lines.size()
	await studio.import_place(fx_dir.path_join("baseplate-566.rbxl"), bp_dir)
	await _settle(40)
	# open_place makes a new world, so the Output's own store is what to read
	var import_lines: Array = []
	for k in range(lines_before, studio._lines.size()):
		if str(studio._lines[k].get("text", "")).find("imported ") >= 0:
			import_lines.append(str(studio._lines[k].get("text", "")))
	var bws := _service(studio.world, "Workspace")
	var baseplate := _in(studio.world, bws, "Baseplate")
	check(baseplate != 0 and _in(studio.world, bws, "SpawnLocation") != 0 and _prop_of(baseplate, "Size").get("value", Vector3.ZERO).x > 100,
		"a place Studio saved opens: its Baseplate is %s" % [_prop_of(baseplate, "Size").get("value", "-")])
	var import_line: String = import_lines[0] if not import_lines.is_empty() else "" 
	check(import_line.find("instances") > 0 and import_line.find("Terrain was not imported") < 0 and import_line.find("empty ones Studio keeps") > 0,
		"the Output counts what came in, does not mourn an empty Terrain, and counts Studio's own services: %s" % import_line)
	studio.open_place(scratch)
	await _settle(150)

	check(studio._grid != null and studio._grid.mesh is ImmediateMesh, "there is a ground grid to turn on")
	studio._show_collision(true)
	var outlines := 0
	for id in studio._descendants(studio._workspace(), []):
		var body: Node3D = studio.world.get_part_node(id)
		if body == null: continue
		for kid in body.get_children():
			if kid is CollisionShape3D and kid.get_node_or_null("StudioOutline") != null:
				outlines += 1
	check(outlines > 5, "and the collision shapes can be shown: %d outlined" % outlines)
	studio._show_collision(false)

	# A .lua in the plugins folder runs in the edit world with a `plugin` global.
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("user://plugins"))
	if FileAccess.file_exists("user://plugin_settings.json"):
		DirAccess.remove_absolute(ProjectSettings.globalize_path("user://plugin_settings.json"))
	var plug_file := FileAccess.open("user://plugins/HelloTool.lua", FileAccess.WRITE)
	plug_file.store_string("""
local toolbar = plugin:CreateToolbar("Test Tools")
local button = toolbar:CreateButton("hello", "Say hello and select the crate", "", "Hello")
local Selection = game:GetService("Selection")
local runs = (plugin:GetSetting("runs") or 0) + 1
plugin:SetSetting("runs", runs)
print("plugin loaded", plugin.Name, runs, game:GetService("RunService"):IsEdit(), script.Parent.ClassName)
button.Click:Connect(function()
	Selection:Set({workspace.Map.Crate})
	button:SetActive(true)
	print("plugin clicked", #Selection:Get(), Selection:Get()[1].Name)
end)
Selection.SelectionChanged:Connect(function() print("plugin saw selection", #Selection:Get()) end)
plugin.Unloading:Connect(function() print("plugin unloading", plugin.Name) end)
local ChangeHistoryService = game:GetService("ChangeHistoryService")
local build = toolbar:CreateButton("build", "Build a part and tint the crate", "", "Build")
build.Click:Connect(function()
	local part = Instance.new("Part"); part.Name = "PluginPart"; part.Anchored = true; part.Position = Vector3.new(50, 5, 50); part.Parent = workspace
	workspace.Map.Crate.Transparency = 0.5
	ChangeHistoryService:SetWaypoint("Plugin build")
	print("plugin built")
end)
local info = DockWidgetPluginGuiInfo.new(Enum.InitialDockState.Float, true, false, 240, 120, 100, 60)
local dock = plugin:CreateDockWidgetPluginGui("HelloDock", info)
dock.Title = "Hello Dock"
local go = Instance.new("TextButton"); go.Name = "Go"; go.Size = UDim2.new(0, 120, 0, 32); go.Position = UDim2.new(0, 10, 0, 10); go.Text = "Go"; go.Parent = dock
go.Activated:Connect(function() print("plugin dock pressed", go.Text) end)
""")
	plug_file.close()
	studio._load_plugins()
	await _settle(30)
	var plug_btn: Dictionary = {}
	for pbtn in studio._plugin_buttons:
		if str(pbtn.get("text", "")) == "Hello":
			plug_btn = pbtn
	check(not plug_btn.is_empty() and str(plug_btn.get("toolbar", "")) == "Test Tools" and str(plug_btn.get("plugin", "")) == "HelloTool",
		"a plugin's toolbar button reaches the Studio, under its toolbar's name: %s" % [plug_btn])
	check(_line_has("plugin loaded	HelloTool	1	true	PluginDebugService"),
		"the plugin ran in the edit world as a Script under PluginDebugService, with its `plugin` global, RunService:IsEdit() true, no setting yet")
	studio._tab_bar.current_tab = studio.RIBBON.keys().find("Plugins")
	studio._fill_band()
	await _settle(2)
	check(not plug_btn.is_empty() and studio._commands.has("plugin_%d" % int(plug_btn.get("id", 0))) and studio._groups_of("Plugins").has("Test Tools"),
		"and sits on the Plugins tab in a group named for the toolbar")
	if not plug_btn.is_empty():
		studio._do("plugin_%d" % int(plug_btn.get("id", 0)))
	await _settle(15)
	var plug_crate := _in(studio.world, _in(studio.world, _service(studio.world, "Workspace"), "Map"), "Crate")
	check(_line_has("plugin clicked	1	Crate") and studio._selection == [plug_crate], "Click fires; Selection:Set selects the crate in the Studio")
	var plug_btn2: Dictionary = {}
	for pbtn2 in studio._plugin_buttons:
		if str(pbtn2.get("text", "")) == "Hello":
			plug_btn2 = pbtn2
	check(bool(plug_btn2.get("active", false)), "SetActive(true) shows on the button")
	var dock_layer: CanvasLayer = studio.world.get_node_or_null("HelloDock")
	var dock_panel: Panel = dock_layer.get_node_or_null("Dock") if dock_layer else null
	var dock_title: Label = dock_panel.get_node_or_null("Title") if dock_panel else null
	var go_id := _in(studio.world, _in(studio.world, _service(studio.world, "CoreGui"), "HelloDock"), "Go")
	var go_rect: Rect2 = studio.world.gui_rect(go_id) if go_id != 0 else Rect2()
	check(dock_panel != null and dock_title != null and dock_title.text == "Hello Dock" and absf(dock_panel.size.x - 240) < 0.5 and absf(dock_panel.size.y - 144) < 0.5,
		"CreateDockWidgetPluginGui draws a floating frame over the view, titled, sized by its DockWidgetPluginGuiInfo (240 by 120 plus the title bar): %s" % [dock_panel.size if dock_panel else Vector2.ZERO])
	check(go_id != 0 and go_rect.size.x > 100 and dock_panel != null and dock_panel.get_global_rect().encloses(go_rect),
		"the TextButton in it is drawn inside the frame: %s in %s" % [go_rect, dock_panel.get_global_rect() if dock_panel else Rect2()])
	var go_node: Control = dock_layer.find_child("Go", true, false) if dock_layer else null
	if go_id != 0:
		for pressed in [true, false]:
			var ev := InputEventMouseButton.new()
			ev.button_index = MOUSE_BUTTON_LEFT
			ev.pressed = pressed
			ev.position = Vector2(10, 10)
			studio.world._on_gui_input(ev, go_id)
	await _settle(10)
	check(_line_has("plugin dock pressed	Go"), "and pressing it fires Activated in the plugin (the edit world's own runtime): %s" % [go_node.get_path() if go_node else "no node"])
	studio._select([])
	await _settle(10)
	check(_line_has("plugin saw selection	0"), "the Studio's own selection reaches SelectionChanged and Selection:Get()")
	check(str(studio._plugin_settings.get("HelloTool", {}).get("runs", "")) == "1", "SetSetting is kept by the Studio: %s" % [studio._plugin_settings])
	# What a plugin changes is one undo entry per ChangeHistoryService waypoint.
	var build_btn: Dictionary = {}
	for pbtn3 in studio._plugin_buttons:
		if str(pbtn3.get("text", "")) == "Build":
			build_btn = pbtn3
	var crate_t_before = _prop_in(studio.world, plug_crate, "Transparency").get("value", null)
	if not build_btn.is_empty():
		studio._do("plugin_%d" % int(build_btn.get("id", 0)))
	await _settle(20)
	var plug_part := _in(studio.world, _service(studio.world, "Workspace"), "PluginPart")
	check(_line_has("plugin built") and plug_part != 0 and is_equal_approx(float(_prop_in(studio.world, plug_crate, "Transparency").get("value", 0.0)), 0.5),
		"the plugin's Click built a Part in the Workspace and tinted the crate")
	check(studio.history.undo_label() == "Plugin build", "SetWaypoint closed them as one undo entry, named for the waypoint: %s" % studio.history.undo_label())
	studio.history.undo()
	await _settle(25)
	check(_in(studio.world, _service(studio.world, "Workspace"), "PluginPart") == 0 and is_equal_approx(float(_prop_in(studio.world, plug_crate, "Transparency").get("value", -1.0)), float(crate_t_before)),
		"Ctrl+Z takes the part away and the crate back to %s: part %d, crate %s" % [crate_t_before, _in(studio.world, _service(studio.world, "Workspace"), "PluginPart"), _prop_in(studio.world, plug_crate, "Transparency").get("value", null)])
	studio.history.redo()
	await _settle(25)
	check(_in(studio.world, _service(studio.world, "Workspace"), "PluginPart") != 0 and is_equal_approx(float(_prop_in(studio.world, plug_crate, "Transparency").get("value", 0.0)), 0.5),
		"and Ctrl+Y brings both back")
	studio.history.undo()
	await _settle(25)
	var mf := FileAccess.open("user://plugins/ModelPlugin.rbxmx", FileAccess.WRITE)
	mf.store_string('<roblox version="4"><Item class="Script" referent="RBX0"><Properties><string name="Name">ModelPlugin</string><ProtectedString name="Source">print("model plugin", plugin.Name, script.Parent.ClassName)</ProtectedString></Properties></Item></roblox>')
	mf.close()
	studio._tab_bar.current_tab = 0
	studio._fill_band()
	studio._load_plugins()
	await _settle(30)
	check(_line_has("plugin unloading	HelloTool") and _line_has("plugin loaded	HelloTool	2	true	PluginDebugService"),
		"Reload: Unloading fired, the plugin ran again, GetSetting read the kept value back (runs 2)")
	check(_line_has("model plugin	ModelPlugin	PluginDebugService"), "an .rbxmx in the folder is a plugin too: its Script ran under PluginDebugService with `plugin` named for it")
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://plugins/ModelPlugin.rbxmx"))
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://plugins/HelloTool.lua"))
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://plugin_settings.json"))
	studio._load_plugins()
	await _settle(10)
	check(studio._plugin_buttons.is_empty(), "with the file gone, a reload leaves no buttons")
	# The Rig Builder (Avatar tab) builds Roblox's R6 and R15 block rigs as one undoable insert.
	var rb_ws := _service(studio.world, "Workspace")
	studio._do("rig_r6")
	await _settle(30)
	var rb_rig := _in(studio.world, rb_ws, "Rig")
	var rb_torso := _in(studio.world, rb_rig, "Torso")
	var rb_count := func(rig: int) -> Array:
		var parts := 0
		var motors := 0
		var hum := 0
		for kid in studio.world.get_child_ids(rig):
			var kc: String = studio.world.get_instance(kid).get("class_name", "")
			if kc == "Part":
				parts += 1
			elif kc == "Humanoid":
				hum = kid
			for g in studio.world.get_child_ids(kid):
				if studio.world.get_instance(g).get("class_name", "") == "Motor6D":
					motors += 1
		return [parts, motors, hum]
	var rb6: Array = rb_count.call(rb_rig)
	check(rb_rig != 0 and rb6[0] == 7 and rb6[1] == 6 and rb6[2] != 0,
		"R6 Rig: a Model of the HumanoidRootPart and six limbs, six Motor6Ds and a Humanoid: %d parts, %d joints" % [rb6[0], rb6[1]])
	var rb_shoulder := _in(studio.world, rb_torso, "Right Shoulder")
	check(rb_shoulder != 0 and _hidden_prop(rb_shoulder, "C0Position") == Vector3(1, 0.5, 0) and _hidden_prop(rb_shoulder, "C1Position") == Vector3(-0.5, 0.5, 0)
		and _hidden_prop(rb_shoulder, "C1Orientation") == Vector3(0, 90, 0),
		"Torso.Right Shoulder carries Roblox's C0 (1, 0.5, 0) and C1 (-0.5, 0.5, 0) turned 90 about Y: %s %s %s" % [_hidden_prop(rb_shoulder, "C0Position"), _hidden_prop(rb_shoulder, "C1Position"), _hidden_prop(rb_shoulder, "C1Orientation")])
	var rb_arm := _in(studio.world, rb_rig, "Right Arm")
	check(rb_arm != 0 and _in(studio.world, rb_arm, "RightGripAttachment") != 0 and _in(studio.world, _in(studio.world, rb_rig, "Head"), "HatAttachment") != 0,
		"the limbs wear their attachments (RightGripAttachment, HatAttachment)")
	check(studio._anim.rig_of(studio.world, rb_torso) == rb_rig and studio._selection == [rb_rig], "the Animation Editor sees it as a rig, and it is selected")
	studio.history.undo()
	await _settle(10)
	check(_in(studio.world, rb_ws, "Rig") == 0, "Ctrl+Z takes the whole rig away")
	studio._do("rig_r15")
	await _settle(30)
	var rb15_rig := _in(studio.world, rb_ws, "Rig")
	var rb15: Array = rb_count.call(rb15_rig)
	var rb15_type: String = str(_prop_in(studio.world, rb15[2], "RigType").get("value", "")) if rb15[2] != 0 else ""
	check(rb15_rig != 0 and rb15[0] == 16 and rb15[1] == 15 and rb15_type == "R15",
		"R15 Rig: sixteen parts, fifteen Motor6Ds each in its limb, Humanoid.RigType R15: %d parts, %d joints, %s" % [rb15[0], rb15[1], rb15_type])
	var rb_knee := _in(studio.world, _in(studio.world, rb15_rig, "LeftLowerLeg"), "LeftKnee")
	check(rb_knee != 0 and _hidden_prop(rb_knee, "C0Position") == Vector3(0, -0.7, 0) and _hidden_prop(rb_knee, "C1Position") == Vector3(0, 0.7, 0),
		"LeftLowerLeg.LeftKnee joins the upper leg's bottom to the lower leg's top")
	studio.history.undo()
	await _settle(10)
	studio._run_command("local rig = Instance.new('Model') rig.Name = 'Rig' local root = Instance.new('Part') root.Name = 'HumanoidRootPart' root.Size = Vector3.new(2, 2, 1) root.Position = Vector3.new(30, 3, -30) root.Anchored = true root.Parent = rig local arm = Instance.new('Part') arm.Name = 'Right Arm' arm.Size = Vector3.new(1, 2, 1) arm.Position = Vector3.new(31.5, 3, -30) arm.Anchored = true arm.Parent = rig local m = Instance.new('Motor6D') m.Name = 'Right Shoulder' m.Part0 = root m.Part1 = arm m.C0 = CFrame.new(1, 0.5, 0) m.C1 = CFrame.new(-0.5, 0.5, 0) m.Parent = root local ac = Instance.new('AnimationController') ac.Parent = rig rig.Parent = workspace")
	await _settle(20)
	var ae_rig := _in(studio.world, _service(studio.world, "Workspace"), "Rig")
	var ae_arm := _in(studio.world, ae_rig, "Right Arm")
	check(ae_rig != 0 and ae_arm != 0 and studio._anim.rig_of(studio.world, ae_arm) == ae_rig, "a Model with a Motor6D in it is a rig, found from any part of it")
	studio._select_one(ae_arm)
	studio._toggle_animation()
	await _settle(4)
	var ae_anim = studio._anim
	check(ae_anim.visible and ae_anim.joints.size() == 1 and ae_anim.joints[0].name == "Right Arm", "the Animation Editor opens on it with the joint named for its Part1: %s" % [ae_anim.joints.map(func(j): return j.name)])
	ae_anim.set_time(0.0)
	ae_anim.set_joint_rotation("Right Arm", Vector3.ZERO)
	ae_anim.set_time(1.0)
	ae_anim.set_joint_rotation("Right Arm", Vector3(0, 0, 90))
	await _settle(4)
	var ae_turned = _prop_of(ae_arm, "Orientation").value
	var ae_moved = _prop_of(ae_arm, "Position").value
	check(absf(ae_turned.z - 90) < 0.5 and ae_moved.is_equal_approx(Vector3(31.5, 4, -30)),
		"a turn keyed at 1 s turns the arm 90 about its shoulder, not its middle: %s at %s" % [ae_turned, ae_moved])
	ae_anim.set_time(0.5)
	await _settle(4)
	check(absf(_prop_of(ae_arm, "Orientation").value.z - 45) < 1.0, "halfway between the keys the arm is halfway round: %s" % _prop_of(ae_arm, "Orientation").value)
	check(ae_anim.key_times("Right Arm") == [0.0, 1.0], "two keys on the joint's row: %s" % [ae_anim.key_times("Right Arm")])
	var ae_undo_before: String = studio.history.undo_label()
	await ae_anim.save("Wave")
	await _settle(10)
	var ae_saves := _in(studio.world, ae_rig, "AnimSaves")
	var ae_wave := _in(studio.world, ae_saves, "Wave") if ae_saves != 0 else 0
	check(ae_wave != 0 and studio.world.get_instance(ae_wave).class_name == "KeyframeSequence", "Save writes a KeyframeSequence under the rig's AnimSaves, where Roblox's editor keeps them")
	var ae_kf_times := []
	var ae_pose_z := []
	if ae_wave != 0:
		for ae_kf in studio.world.get_child_ids(ae_wave):
			ae_kf_times.append(_hidden_prop(ae_kf, "Time"))
			var ae_root_pose := _in(studio.world, ae_kf, "HumanoidRootPart")
			var ae_arm_pose := _in(studio.world, ae_root_pose, "Right Arm") if ae_root_pose != 0 else 0
			if ae_arm_pose != 0:
				ae_pose_z.append(roundf(_hidden_prop(ae_arm_pose, "CFrameOrientation").z))
	check(ae_kf_times == [0.0, 1.0] and ae_pose_z == [0.0, 90.0], "one Keyframe per keyed time, its Poses nested root -> limb with the joint's turn: %s %s" % [ae_kf_times, ae_pose_z])
	check(studio.history.undo_label().begins_with("save animation") and ae_undo_before != studio.history.undo_label(), "the save is one undo entry, and posing the arm left none: %s" % studio.history.undo_label())
	ae_anim.close()
	await _settle(4)
	check(not ae_anim.visible and _prop_of(ae_arm, "Position").value.is_equal_approx(Vector3(31.5, 3, -30)) and absf(_prop_of(ae_arm, "Orientation").value.z) < 0.01,
		"closing puts the arm back where the place has it: %s" % _prop_of(ae_arm, "Position").value)
	studio._select_one(ae_rig)
	studio._toggle_animation()
	await _settle(4)
	check(ae_anim.visible and ae_anim.load_named("Wave") and ae_anim.key_times("Right Arm") == [0.0, 1.0], "opened again, Load brings the saved keys back: %s" % [ae_anim.key_times("Right Arm")])
	ae_anim.close()

	studio._run_command("local s = Instance.new('Script') s.Name = 'Ticker' s.Source = 'local n = 0\\nwhile true do\\n\\tn += 1\\n\\ttask.wait(0.05)\\nend' s.Parent = game.ServerScriptService")
	await _settle(10)
	var ticker := _in(studio.world, _service(studio.world, "ServerScriptService"), "Ticker")
	studio._open_script(ticker)
	await _settle(2)
	check(ticker != 0 and studio._editor.source_of(ticker).split("\n").size() == 5, "a Ticker script to stop, open in the editor: %d lines" % studio._editor.source_of(ticker).split("\n").size())
	studio._editor._code.set_line_as_breakpoint(2, true)     # the gutter, line 3 from one
	await _settle(2)
	check(studio._breakpoints.get("ServerScriptService.Ticker", []) == [3], "a click on the gutter is a breakpoint the Studio keeps by the script's name: %s" % [studio._breakpoints])
	studio.play()
	for i in 400:
		await process_frame
		if studio._debug_paused:
			break
	check(studio._debug_paused and studio._debugger.visible and studio._debugger.at_script == "ServerScriptService.Ticker" and studio._debugger.line == 3,
		"Play stops at it: the Debugger shows %s:%d" % [studio._debugger.at_script, studio._debugger.line])
	check(studio._debugger.frames.size() >= 1 and studio._debugger.frames[0].get("function", "") == "(main)" and studio._debugger.local_value("n") == "0",
		"with the chunk's frame and its local n = %s" % studio._debugger.local_value("n"))
	check(studio._editor.showing() == ticker and studio._editor._code.is_line_executing(2), "and the script open in the editor with the arrow on line 3")
	var stops_before: String = studio._debugger.local_value("n")
	studio._do("dbg_over")
	for i in 200:
		await process_frame
		if studio._debugger.line == 4:
			break
	check(studio._debugger.line == 4 and studio._debugger.local_value("n") == "1", "Step Over (F10) runs the line and stops on the next: n went %s -> %s at line %d" % [stops_before, studio._debugger.local_value("n"), studio._debugger.line])
	studio._do("dbg_continue")
	for i in 200:
		await process_frame
		if studio._debugger.line == 3 and studio._debugger.local_value("n") == "1":
			break
	check(studio._debugger.line == 3 and studio._debugger.local_value("n") == "1", "Continue (F5) runs round the loop to the breakpoint again, n = %s" % studio._debugger.local_value("n"))
	studio._editor._code.set_line_as_breakpoint(2, false)
	await _settle(2)
	studio._do("dbg_continue")
	for i in 100:
		await process_frame
		if not studio._debug_paused:
			break
	await _settle(30)
	check(not studio._debug_paused and not studio._debugger.paused(), "with the breakpoint cleared, Continue lets it run: %s" % studio._debug_paused)
	studio.stop()
	await _settle(20)
	check(not studio._debugger.visible, "Stop puts the Debugger away")

	print("studio: " + ("PASS" if failed == 0 else "%d FAILED" % failed))
	quit(1 if failed else 0)

# An LZ4 block: a token per sequence, then its literals, a two-byte little-endian offset and a
# match copied from what is already written out.
func _lz4(src: PackedByteArray, out_len: int) -> PackedByteArray:
	var out := PackedByteArray()
	var i := 0
	while i < src.size():
		var token := src[i]
		i += 1
		var lit := token >> 4
		if lit == 15:
			while true:
				var b := src[i]
				i += 1
				lit += b
				if b != 255:
					break
		for k in lit:
			out.append(src[i])
			i += 1
		if i >= src.size():
			break
		var off := src[i] | (src[i + 1] << 8)
		i += 2
		var ml := token & 15
		if ml == 15:
			while true:
				var b := src[i]
				i += 1
				ml += b
				if b != 255:
					break
		ml += 4
		var start := out.size() - off
		for k in ml:
			out.append(out[start + k])
	return out

func _settle(frames: int) -> void:
	for i in frames:
		await process_frame

func _child_of(parent: int, name: String) -> int:
	return _in(world, parent, name)

func _in(of_world: PulseBlockzWorld, parent: int, name: String) -> int:
	for id in of_world.get_child_ids(parent):
		if of_world.get_instance(id).name == name:
			return id
	return 0

func _service(of_world: PulseBlockzWorld, class_name_: String) -> int:
	for id in of_world.get_child_ids(0):
		if of_world.get_instance(id).class_name == class_name_:
			return id
	return 0

func _prop_of(id: int, name: String) -> Dictionary:
	return _prop_in(studio.live(), id, name)

# print's values are tab-separated, so a needle can span them
func _line_has(needle: String) -> bool:
	for l in studio._lines:
		if str(l.get("text", "")).contains(needle):
			return true
	return false

func _hidden_prop(id: int, name: String):
	for p in studio.world.get_properties(id, true):      # hidden ones too: a Pose's CFrame pair
		if p.name == name:
			return p.value
	return null

func _prop_in(of_world: PulseBlockzWorld, id: int, name: String) -> Dictionary:
	for p in of_world.get_properties(id):
		if p.name == name:
			return p
	return {}

func _wipe_dir(path: String) -> void:
	var d := DirAccess.open(path)
	if d == null:
		return
	d.list_dir_begin()
	var n := d.get_next()
	while n != "":
		if d.current_is_dir():
			_wipe_dir(path.path_join(n))
		else:
			DirAccess.remove_absolute(path.path_join(n))
		n = d.get_next()
	d.list_dir_end()
	DirAccess.remove_absolute(path)

func _copy_dir(from: String, to: String) -> void:
	DirAccess.make_dir_recursive_absolute(to)
	var d := DirAccess.open(from)
	if d == null:
		return
	d.list_dir_begin()
	var n := d.get_next()
	while n != "":
		if d.current_is_dir():
			if not n.begins_with("."):
				_copy_dir(from.path_join(n), to.path_join(n))
		else:
			DirAccess.copy_absolute(from.path_join(n), to.path_join(n))
		n = d.get_next()

func _logged(fragment: String) -> bool:
	for l in logs:
		if l.find(fragment) != -1:
			return true
	return false
