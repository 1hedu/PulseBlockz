# Headless check of Play Solo: godot --headless --path . -s res://tests/play_solo_test.gd
# Drives the demo place with synthetic input and server chunks, phase by phase, and
# checks the Roblox behaviour each one is meant to produce.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var phase := 0
var start_pos: Vector3
var cam_start: Vector3
var peak_y := -1e9
var wave_arm := 0.0
var shrug_arm := 0.0
var note_at := 0.0
var failed := 0
var logs: Array[String] = []

func _initialize() -> void:
	var main = load("res://Main.tscn").instantiate()
	world = main.get_node("World")
	world.data_store_path = "user://play_solo_test_datastores.json"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(world.data_store_path))
	root.add_child(main)
	world.script_print.connect(func(n, t): logs.append(n + ": " + t))
	world.script_error.connect(func(n, e): logs.append("ERROR " + n + ": " + e))
	world.script_warn.connect(func(n, w): logs.append("WARN " + n + ": " + w))

func _key(code: Key, pressed: bool) -> void:
	var ev := InputEventKey.new()
	ev.physical_keycode = code
	ev.keycode = code
	ev.pressed = pressed
	Input.parse_input_event(ev)

func _type(text: String) -> void:
	for ch in text:
		for pressed in [true, false]:
			var ev := InputEventKey.new()
			ev.keycode = KEY_A + (ch.unicode_at(0) - "a".unicode_at(0))
			ev.physical_keycode = ev.keycode
			ev.unicode = ch.unicode_at(0)
			ev.pressed = pressed
			Input.parse_input_event(ev)

func _wheel(at: Vector2, up: bool) -> void:
	var mv := InputEventMouseMotion.new()
	mv.position = at
	mv.global_position = at
	Input.parse_input_event(mv)
	for pressed in [true, false]:
		var ev := InputEventMouseButton.new()
		ev.button_index = MOUSE_BUTTON_WHEEL_UP if up else MOUSE_BUTTON_WHEEL_DOWN
		ev.pressed = pressed
		ev.factor = 1.0
		ev.position = at
		ev.global_position = at
		Input.parse_input_event(ev)

func _click(at: Vector2) -> void:
	var mv := InputEventMouseMotion.new()
	mv.position = at
	mv.global_position = at
	Input.parse_input_event(mv)
	for pressed in [true, false]:
		var ev := InputEventMouseButton.new()
		ev.button_index = MOUSE_BUTTON_LEFT
		ev.pressed = pressed
		ev.position = at
		ev.global_position = at
		Input.parse_input_event(ev)

func check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok: failed += 1

var waited := 0.0
var npc_at := 0.0        # phase 7's timeout runs from here
var lt_at := 0.0         # and phase 8's, from the Lighting chunk

# --- the physics clock ---------------------------------------------------------------
# step_characters runs at a fixed 60 Hz in PulseBlockzWorld::_physics_process; the script
# workers run in _process, one of them the demo's runaway Evil script the world kills every
# frame, so frames here really do hitch. Wall clock is therefore not a measure of walking:
# every window below is counted in physics ticks. SceneTree's physics_process runs before any
# node's, so `p` here is the body after the previous tick and before the world steps the next:
# two samples are (ticks between them)/60 apart at any frame rate.
var phys_p := Vector3.INF        # where the body stood at the last physics tick
var was_floor := true            # is_on_floor() at that tick
var walk_arm := false            # W is down; the window opens when the body first moves
var walk_p0 := Vector3.ZERO      # where it stood on the tick the walk began
var walk_ticks := -1             # ticks since then (-1: it has not started walking)
var walk_moved := Vector3.ZERO   # what it walked inside the phase 1 window
var walk_done := false
var left_floor := false          # phase 2: the jump took it off the ground
var space_up := false            # and space has been let go
var jump_at := 0.0               # t when space went down
var jump_trace: Array[String] = []   # the jump tick by tick, printed on failure

func _char() -> CharacterBody3D:
	if world == null: return null
	return world.get_local_character_node() as CharacterBody3D

func _on_floor() -> bool:
	var cb := _char()
	return cb != null and cb.is_on_floor()

func _physics_process(_delta: float) -> bool:
	var cb := _char()
	if cb == null:
		phys_p = Vector3.INF
		return false
	var p: Vector3 = cb.global_position
	var floor_now: bool = cb.is_on_floor()
	if phase == 2:
		peak_y = max(peak_y, p.y)   # every tick: a hitch cannot step over the apex
		if jump_trace.size() < 14:
			var hit := ""
			for i in cb.get_slide_collision_count():
				var col := cb.get_slide_collision(i)
				if col.get_normal().y < -0.5: hit += " under " + str(col.get_collider().name) + "@" + str(world.get_part_id(col.get_collider()))
			jump_trace.append("%.2f/%.1f/%s%s" % [p.y - start_pos.y, cb.velocity.y, "f" if floor_now else "a", hit])
		if not floor_now: left_floor = true
	if walk_arm:
		if walk_ticks < 0:
			# the walk starts from phys_p, not p: the tick that just ran already moved it
			if phys_p != Vector3.INF and Vector2(p.x - phys_p.x, p.z - phys_p.z).length() > 0.15:
				walk_p0 = phys_p
				walk_ticks = 1
		else:
			walk_ticks += 1
			if not walk_done:
				# 24 ticks is 0.4 s, or the last tick on the 12-stud spawn platform,
				# whichever comes first: its far edge is 6.0 studs out and 0.4 s at
				# WalkSpeed 16 covers 6.4, so the window would close airborne.
				if not floor_now and was_floor:
					walk_moved = phys_p - walk_p0
					walk_done = true
				elif walk_ticks >= 24:
					walk_moved = p - walk_p0
					walk_done = true
	phys_p = p
	was_floor = floor_now
	return false

func _process(delta: float) -> bool:
	if root.size.x < 640: root.size = Vector2i(1152, 648)   # headless: the window is 64x64 until told otherwise
	# t starts when the place is ready: the phase timeouts below do not count the load
	if phase == 0 and world.get_local_character_node() == null and waited < 20.0:
		waited += delta
		return false
	t += delta
	var body: Node3D = world.get_local_character_node()
	if body:   # the furthest the arms turn while the wave plays, sampled every frame
		for c in body.get_children():
			if c is MeshInstance3D and c.name == "Right Arm":
				wave_arm = max(wave_arm, _turn(c.transform.basis))
			if c is MeshInstance3D and c.name == "Left Arm" and _has("waving	1	true	true	Movement	Action"):
				shrug_arm = max(shrug_arm, _turn(c.transform.basis))
	match phase:
		0:
			# "swatches" is the last of the client's deferred startup: no key before it
			if (body != null and _on_floor() and _has("wearing	1	true") and _has("my character is here")
					and _has("swatches	")) or t > 8.0:
				check(body != null, "local character has a CharacterBody3D (player %d, character %d)" % [world.get_local_player_id(), world.get_local_character_id()])
				if body == null: return _finish()
				start_pos = body.global_position
				cam_start = world.get_camera().global_position
				_key(KEY_W, true)
				walk_arm = true
				phase = 1
		1:
			if walk_done or t > 3.0:
				var moved: Vector3 = walk_moved
				check(moved.length() > 5.5 and moved.length() < 8.0 and moved.y < 0.2 and moved.y > -1.5, "W walks at WalkSpeed: %.1f studs in 0.4 s, %s" % [moved.length(), moved])
				check(moved.z < -5.5, "forward is the camera's -Z: dz=%.1f" % moved.z)
				# 3.04 over the root is the flat top of the hexagon Head, where the HatAttachments meet
				var cap: MeshInstance3D = null
				for c in body.get_children(): if c is MeshInstance3D and str(c.name).contains("Handle"): cap = c
				var head_top := body.global_position + Vector3(0, 3.04, 0)
				check(cap != null and cap.global_position.distance_to(head_top) < 0.2, "an Accessory's Handle rides the head where the HatAttachments meet: %s vs %s" % [cap.global_position if cap else Vector3.ZERO, head_top])
				check(_has("wearing	1	true"), "the server put it on with Humanoid:AddAccessory")
				var swinging := 0
				for c in body.get_children(): if c is MeshInstance3D and not c.transform.basis.is_equal_approx(Basis()): swinging += 1
				check(swinging == 4, "arms and legs swing while walking: %d limbs posed" % swinging)
				# the skin ramp runs t = 0 at the left foot's outer-bottom corner to 1 at the head's top-right
				var skinned := 0
				var t_foot := -1.0
				var t_ear := -1.0
				for c in body.get_children():
					if c is MeshInstance3D and c.material_override is ShaderMaterial:
						skinned += 1
						var o: Vector3 = c.material_override.get_shader_parameter("g_origin")
						var d: Vector3 = c.material_override.get_shader_parameter("g_dir")
						if c.name == "Left Leg": t_foot = (Vector3(-0.5, -1, 0) - o).dot(d)
						if c.name == "Head": t_ear = (Vector3(1, 0.5, 0) - o).dot(d)
				check(skinned == 6, "all six limbs wear the gradient skin: %d" % skinned)
				check(abs(t_foot) < 0.01 and abs(t_ear - 1.0) < 0.01, "ramp runs foot corner (%.2f) -> head corner (%.2f)" % [t_foot, t_ear])
				phase = 11
		11:
			# 38 ticks is 10 studs down -Z from the spawn at z = 4: off the platform, inside
			# the ring of Spinners orbiting 9 studs from the origin -- they pass a stud and a
			# half over a head and knock a jump under them back down -- and well short of the
			# lava at 60 ticks, because the release of W below lands a render frame or two
			# after this gate fires, which under a hitch is several ticks of walking.
			if (walk_ticks >= 38 and _on_floor()) or t > 4.0:
				_key(KEY_W, false)
				var moved: Vector3 = body.global_position - start_pos
				check(moved.length() > 10.0 and moved.y < 0.0 and moved.y > -1.5, "steps down off the spawn platform: moved %s" % moved)
				var cam_moved: Vector3 = world.get_camera().global_position - cam_start
				check(cam_moved.length() > 6.0, "the camera followed: %s" % cam_moved)
				start_pos = body.global_position
				_key(KEY_SPACE, true)
				jump_at = t
				phase = 2
		2:
			peak_y = max(peak_y, body.global_position.y)
			if not space_up and (left_floor or t > jump_at + 0.8):
				_key(KEY_SPACE, false)
				space_up = true
			if (left_floor and _on_floor()) or t > 6.0:
				check(peak_y - start_pos.y > 3.0, "space jumps: rose %.1f%s" % [peak_y - start_pos.y, "" if peak_y - start_pos.y > 3.0 else "  height/vy/floor by tick: %s" % [jump_trace]])
				check(abs(body.global_position.y - start_pos.y) < 1.0, "and lands again")
				# E drops a brick: after the landing, since one falling on a jumping body knocks it down
				_key(KEY_E, true)
				_key(KEY_E, false)
				_key(KEY_Q, true)   # Q asks the server to play the wave animation
				_key(KEY_Q, false)
				# a PlayerGui ScreenGui is a CanvasLayer, a GuiObject a Panel
				var hud := world.find_child("Hud", true, false) as CanvasLayer
				var drop := world.find_child("Drop", true, false) as Control
				var counter := world.find_child("Counter", true, false) as Control
				check(hud != null and drop != null and counter != null, "ScreenGui / Frame / TextLabel / TextButton are drawn")
				# ZIndexBehavior Sibling is z_as_relative; the Layers gui is Global, so absolute
				var under := world.find_child("Under", true, false) as Control
				var over := world.find_child("Over", true, false) as Control
				check(under != null and over != null and not under.z_as_relative and not over.z_as_relative and under.z_index == 4 and over.z_index == 9,
					"ZIndexBehavior Global weighs a ZIndex against every object on the gui")
				check(drop != null and drop.z_as_relative, "and Sibling (the default) keeps it among its siblings")
				if drop and counter:
					var vp := root.get_visible_rect().size
					vp_size = vp
					var r := drop.get_global_rect()
					check(r.size.is_equal_approx(Vector2(160, 40)) and abs(r.get_center().x - (24 + 120)) < 1.0 and r.position.y > counter.get_global_rect().end.y and r.end.y < vp.y - 24,
						"UDim2 sizes, AnchorPoint and the UIListLayout place the button: %s in %s" % [r, vp])
					var label := drop.find_child("*", true, false) as Label
					if label == null: for c in drop.get_children(): for cc in c.get_children(): if cc is Label: label = cc
					check(label != null and label.text == "Drop a brick", "the TextButton's Text is a Label")
					_click(r.get_center())
				# the map's Sign, with its ClickDetector, stands at (4.5, 4, -8)
				var cam := world.get_camera()
				var sign_at := Vector3(4.5, 4, -8)
				var sp := cam.unproject_position(sign_at)
				check(not cam.is_position_behind(sign_at) and sp.x > 0 and sp.x < vp_size.x and sp.y > 0 and sp.y < vp_size.y, "the Sign is on screen at %s" % sp)
				_click(sp)
				# 1 equips the StarterPack's Wand out of the Backpack
				_key(KEY_1, true)
				_key(KEY_1, false)
				tool_at = t
				phase = 12
		12:
			if (_has("wand equipped by\tAda") and _has("wand in hand\tMouse\ttrue")
					and _has("prompt shown\tOpen\tKeyboard") and _meshes(body) == 10) or t > tool_at + 3.0:
				check(_has("wand equipped by	Ada"), "the 1 key equips the Backpack's tool: Equipped on the server")
				check(_has("wand in hand	Mouse	true"), "and on the client, with the Mouse, the tool in the character")
				var n := _meshes(body)
				check(n == 10, "the tool's Handle and Tip ride the body with the limbs and the Cap: %d meshes %s" % [n, mesh_names])
				var handle: MeshInstance3D = null
				var arm: MeshInstance3D = null
				if body: for c in body.get_children(): if c is MeshInstance3D and c.name == "Right Arm": arm = c
				# Roblox names an Accessory's part Handle too: the tool's is the one nearest the arm
				if body and arm: for c in body.get_children():
					if c is MeshInstance3D and str(c.name).contains("Handle"):
						if handle == null or c.transform.origin.distance_to(arm.transform.origin) < handle.transform.origin.distance_to(arm.transform.origin): handle = c
				var lit: MeshInstance3D = null
				if body: for c in body.get_children(): if c is MeshInstance3D and c.name == "Tip": lit = c
				if handle and arm and lit:
					var rel: Transform3D = arm.transform.affine_inverse() * handle.transform
					var tip_at: Vector3 = arm.transform.affine_inverse() * lit.transform.origin
					# RightGripAttachment is at the arm's end; this arm is 2.3 studs, not R6's 2
					var hand: float = arm.get_aabb().size.y / 2.0
					check(rel.basis.z.is_equal_approx(Vector3(0, 0, 1)) and rel.origin.is_equal_approx(Vector3(0, -hand, -1))
							and tip_at.is_equal_approx(Vector3(0, -hand, -2.7)),
						"Tool.Grip: the wand lies along the hand's look, held a stud back from the arm's end (%.2f), its lit Tip out front: %s %s %s" % [hand, rel.basis.z, rel.origin, tip_at])
				else:
					check(false, "the Handle, the Tip and the Right Arm are meshes on the body")
				var glow: OmniLight3D = null
				if body: for c in body.get_children(): if c is MeshInstance3D and c.name == "Tip": glow = c.get_node_or_null("Glow")
				check(glow != null and glow.omni_range == 6.0 and glow.light_energy == 1.5, "the Tip's PointLight (a model.json child) rides the tool as an OmniLight3D")
				var hotbar: Node = world.get_node_or_null("CoreGui/Backpack/Hotbar")
				var slot1: Button = hotbar.get_node_or_null("1") if hotbar else null
				var slot2: Button = hotbar.get_node_or_null("2") if hotbar else null
				check(slot1 != null and slot1.visible and slot1.text == "1
Wand" and slot2 != null and not slot2.visible, "the hotbar shows the Wand in slot 1 and nothing more: %s" % (slot1.text if slot1 else "no hotbar"))
				check(slot1 != null and slot1.get_theme_stylebox("normal").bg_color.r > 0.9, "the held tool's slot is lit")
				var vp2: Vector2 = world.get_viewport().get_visible_rect().size
				var hr := slot1.get_global_rect() if slot1 else Rect2()
				check(slot1 != null and abs(hr.get_center().x - vp2.x / 2) < 1.0 and hr.end.y > vp2.y - 40 and hr.end.y <= vp2.y, "along the bottom, centred: %s in %s" % [hr, vp2])
				# the Sign's BillboardGui is 4 by 1 studs wide at its distance, 2.5 studs over the Sign
				var tag: Control = world.get_node_or_null("BillboardGuis/Tag")
				var cam2 := world.get_camera()
				var tag_at := Vector3(4.5, 4, -8) + cam2.global_transform.basis.y * 2.5   # StudsOffset is along the camera's axes
				var tag_rect := tag.get_global_rect() if tag else Rect2()
				var studs := vp2.y / (2.0 * (tag_at - cam2.global_position).dot(-cam2.global_transform.basis.z) * tan(deg_to_rad(cam2.fov) / 2))
				check(tag != null and tag.visible and tag_rect.get_center().distance_to(cam2.unproject_position(tag_at)) < 1.5, "the BillboardGui is drawn over the Sign: %s at %s" % [tag_rect, cam2.unproject_position(tag_at)])
				check(tag != null and abs(tag_rect.size.x - 4 * studs) < 1.5 and abs(tag_rect.size.y - studs) < 1.5, "sized in studs at its distance: %s for %.1f px a stud" % [tag_rect.size, studs])
				var lamp: Node = world.get_node_or_null("Lamp")
				var beam: SpotLight3D = lamp.find_child("Beam", true, false) if lamp else null
				if beam == null and lamp: for c in lamp.get_children(): if c is MeshInstance3D: beam = c.get_node_or_null("Beam")
				check(beam != null and beam.spot_range == 14.0 and beam.spot_angle == 50.0, "a SpotLight in the lamp is a SpotLight3D under its mesh: Range 14, Angle 100 -> 50 each side")
				check(beam != null and (-beam.global_transform.basis.z).is_equal_approx(Vector3.DOWN), "Face = Bottom points it down")
				# the character landed inside the Chest prompt's 8 studs
				check(_has("prompt shown	Open	Keyboard"), "the ProximityPrompt in the Chest is shown near the character (PromptShown on the client)")
				var chest: Node = world.get_node_or_null("Chest")
				var label: Label3D = null
				if chest: for c in chest.get_children(): if c is MeshInstance3D: label = c.get_node_or_null("Open")
				check(label != null and label.visible and label.text == "Chest
[F]  Open", "and drawn as a Label3D under its mesh: %s" % (label.text if label else "none"))
				_click(vp_size / 2)
				_key(KEY_F, true)
				_key(KEY_F, false)
				tool_at = t
				phase = 13
		13:
			if (_has("chest sound ended") and _has("wand activated") and _has("wand swung by\tAda")
					and _has("keyframe	Up") and _sign_clicks() != "" and _sign_clicks() != "Clicks: 0") or t > tool_at + 3.5:
				var tag2: Control = world.get_node_or_null("BillboardGuis/Tag")
				var count: Label = null
				if tag2: for c in tag2.get_children(): for cc in c.get_children(): for ccc in cc.get_children(): if ccc is Label: count = ccc
				# not exactly one: a click with the tool in hand can land on the Sign too
				check(count != null and count.text.begins_with("Clicks: ") and count.text != "Clicks: 0", "its TextLabel shows the server's count after the click: %s" % (count.text if count else "none"))
				check(_has("waving	1	true	true	Movement	Action"), "an Animation names a KeyframeSequence, LoadAnimation reads its Length, Loop and Priority")
				check(shrug_arm > 1.2, "the Movement track underneath keeps the Left Arm it names: %.2f rad" % shrug_arm)
				check(_has("keyframe	Up"), "KeyframeReached fires as the clock passes a named Keyframe")
				check(wave_arm > 1.5, "the animation turns the Right Arm through the poses: %.2f rad" % wave_arm)
				check(_has("wand activated"), "a click with the tool in hand: Activated on the client")
				check(_has("wand swung by	Ada"), "and on the server")
				check(_has("prompt triggered	Open	Ada"), "F triggers the Chest's prompt on the client")
				check(_has("chest opened by	Ada"), "and on the server, with the Player")
				check(_has("chest sound loaded	0.25"), "the engine loaded sounds/creak.wav: Loaded, TimeLength 0.25")
				check(_has("heard	sounds/creak.wav"), "the server's Play() is Played on the client")
				check(_has("chest sound ended"), "and Ended when the clock ran out")
				var creak: AudioStreamPlayer3D = null
				var chest2: Node = world.get_node_or_null("Chest")
				if chest2: for c in chest2.get_children(): if c is MeshInstance3D: creak = c.get_node_or_null("Creak")
				check(creak != null and creak.stream != null and is_equal_approx(creak.volume_db, 20.0 * log(0.8) / log(10.0)), "an AudioStreamPlayer3D under the Chest's mesh, Volume 0.8 as dB")
				_key(KEY_1, true)
				_key(KEY_1, false)
				# click in, type, then W: while the box has focus the keys are its, not the character's
				var box := world.find_child("NameBox", true, false) as Control
				var edit: LineEdit = null
				if box: for c in box.get_children(): for cc in c.get_children(): if cc is LineEdit: edit = cc
				check(box != null and edit != null, "a TextBox is drawn as a LineEdit over its Panel")
				if edit:
					check(edit.placeholder_text == "Name the next brick" and edit.text == "", "with its PlaceholderText")
					_click(edit.get_global_rect().get_center())
					_type("ab")
					_key(KEY_W, true)
					_key(KEY_W, false)
					_type("w")
				start_pos = body.global_position if body else Vector3()
				tool_at = t
				phase = 14
		14:
			if (_has("box\tfocused\t1") and _has("key\tW\ttrue")) or t > tool_at + 2.0:
				var edit: LineEdit = null
				var box := world.find_child("NameBox", true, false) as Control
				if box: for c in box.get_children(): for cc in c.get_children(): if cc is LineEdit: edit = cc
				check(edit != null and edit.has_focus() and edit.text == "abw", "the click focused it and the keys typed into it: %s" % (edit.text if edit else "none"))
				check(_has("box	focused	1"), "Focused fired on the client, CursorPosition 1")
				check(body != null and (body.global_position - start_pos).length() < 0.5, "W while typing does not walk the character")
				check(_has("key	W	true"), "though UserInputService still hears it, as processed")
				if edit:
					var ev := InputEventKey.new()
					ev.keycode = KEY_ENTER
					ev.physical_keycode = KEY_ENTER
					ev.pressed = true
					Input.parse_input_event(ev)
					ev = ev.duplicate(); ev.pressed = false
					Input.parse_input_event(ev)
				tool_at = t
				phase = 15
		15:
			if _has("box\tlost\tabw\ttrue\t-1") or t > tool_at + 2.0:
				var edit: LineEdit = null
				var box := world.find_child("NameBox", true, false) as Control
				if box: for c in box.get_children(): for cc in c.get_children(): if cc is LineEdit: edit = cc
				check(edit != null and not edit.has_focus(), "Enter lets the focus go")
				check(_has("box	lost	abw	true	-1"), "FocusLost(true) with the text typed, CursorPosition back to -1")
				var note := world.find_child("NoteBox", true, false) as Control
				var lines: TextEdit = null
				if note: for c in note.get_children(): for cc in c.get_children(): if cc is TextEdit: lines = cc
				check(note != null and lines != null and lines.placeholder_text == "A note, in as many lines as you like", "a MultiLine TextBox is drawn as a TextEdit")
				if lines:
					_click(lines.get_global_rect().get_center())
					_type("hi")
					var ev := InputEventKey.new()
					ev.keycode = KEY_ENTER
					ev.physical_keycode = KEY_ENTER
					ev.pressed = true
					Input.parse_input_event(ev)
					ev = ev.duplicate(); ev.pressed = false
					Input.parse_input_event(ev)
					_type("ho")
				note_at = t
				phase = 150
		150:
			if (_has("note\thi|ho\t2\t6") and _has("scrolled\t0, 52")) or t > note_at + 2.0:
				var note := world.find_child("NoteBox", true, false) as Control
				var lines: TextEdit = null
				if note: for c in note.get_children(): for cc in c.get_children(): if cc is TextEdit: lines = cc
				check(lines != null and lines.has_focus() and lines.get_line_count() == 2 and lines.text == "hi\nho", "Enter puts in a newline instead of letting the focus go: %s" % (lines.text if lines else "none"))
				check(_has("note	hi|ho	2	6"), "the two lines reach the script as one Text, CursorPosition counted over them")
				if lines: lines.release_focus()
				tool_at = t
				phase = 15
				# 12 swatches at 3 cells of 40 (+4 padding) a row: 4 rows, 172 tall in a 120 window
				check(_has("swatches	3, 4	40, 40	160, 172	160, 120"), "UIGridLayout: AbsoluteCellCount 3 by 4, AbsoluteCellSize 40; ScrollingFrame: AutomaticCanvasSize Y makes the canvas 172 tall in a 120 window")
				var swatches := world.find_child("Swatches", true, false) as Control
				var s5 := world.find_child("Swatch5", true, false) as Control
				var bar: VScrollBar = null
				if swatches: for c in swatches.get_children(): for cc in c.get_children(): if cc is VScrollBar: bar = cc
				check(swatches != null and s5 != null and s5.get_global_position() == swatches.get_global_position() + Vector2(44, 44 - 52), "the fifth swatch sits in the second cell of the second row, 52 up the canvas")
				check(bar != null and bar.visible and is_equal_approx(bar.max_value, 172) and is_equal_approx(bar.page, 120) and bar.size.x == 8, "a scroll bar, ScrollBarThickness wide, down the right edge")
				# the badge's Size is 20% of the viewport, clamped to 180 wide at 3:1
				check(_has("badge	90, 30	24, 24"), "UISizeConstraint MaxSize then UIAspectRatioConstraint size it 180 by 60; UIScale 0.5 shows it 90 by 30")
				var badge := world.find_child("Badge", true, false) as Control
				var label: Label = null
				if badge: for c in badge.get_children(): for cc in c.get_children(): if cc is Label: label = cc
				check(badge != null and badge.size == Vector2(180, 60) and badge.scale == Vector2(0.5, 0.5), "the Control is 180 by 60 at scale 0.5")
				check(label != null and label.get_theme_font_size("font_size") == 24, "TextScaled text capped by UITextSizeConstraint.MaxTextSize: %d" % (label.get_theme_font_size("font_size") if label else -1))
				check(_has("scrolled	0, 52"), "the script scrolled it to the end: CanvasPosition clamped to the canvas, 172 - 120")
				if swatches: _wheel(swatches.get_global_position() + Vector2(80, 60), true)
				tool_at = t
				phase = 16
		16:
			if _has("scrolled\t0, 12") or t > tool_at + 2.0:
				var swatches := world.find_child("Swatches", true, false) as Control
				check(_has("scrolled	0, 12"), "the wheel over a swatch scrolls the frame 40 back: CanvasPosition 0, 12 reaches the script")
				if swatches: _click(swatches.get_global_position() + Vector2(80, 60))
				tool_at = t
				phase = 17
		17:
			if _has("swatch\t5") or t > tool_at + 2.0:
				check(_has("swatch	5") and not _has("swatch	8"), "a click lands on the swatch scrolled under it (the fifth, not the eighth that was there)")
				check(_has("logo	true	32, 16	true"), "ImageLabel / ImageButton load images/sheet.png: IsLoaded, ContentImageSize 32 by 16")
				var logo := world.find_child("Logo", true, false) as Control
				var pic: TextureRect = null
				if logo: for c in logo.get_children(): for cc in c.get_children(): if cc is TextureRect: pic = cc
				var atlas := pic.texture as AtlasTexture if pic else null
				check(atlas != null and atlas.region == Rect2(0, 0, 16, 16) and atlas.atlas.get_size() == Vector2(32, 16), "a TextureRect over the Panel showing ImageRectOffset / ImageRectSize of it")
				check(pic != null and pic.stretch_mode == TextureRect.STRETCH_KEEP_ASPECT_CENTERED and pic.texture_filter == CanvasItem.TEXTURE_FILTER_NEAREST, "ScaleType.Fit, ResampleMode.Pixelated")
				var aim := world.find_child("Aim", true, false) as Control
				if aim: _click(aim.get_global_rect().get_center())
				tool_at = t
				phase = 18
		18:
			if _has("aim\timages/cursor.png") or t > tool_at + 3.0:
				check(_has("aim	images/cursor.png"), "the ImageButton's Activated set Mouse.Icon to images/cursor.png")
				var aim := world.find_child("Aim", true, false) as Control
				var pic: TextureRect = null
				if aim: for c in aim.get_children(): for cc in c.get_children(): if cc is TextureRect: pic = cc
				var hover := pic.texture as AtlasTexture if pic else null
				check(hover != null and hover.atlas.get_size() == Vector2(24, 24) and pic.modulate == Color(1, 1, 1), "HoverImage (images/cursor.png, 24 by 24) while the pointer is over it, ImageColor3 followed the script")
				var warned := false
				for l in logs: if l.begins_with("WARN Image"): warned = true
				check(not warned, "the cursor image loaded: no Image warning")
				check(_has("pyramid	2, 1.5, 2"), "MeshPart.MeshSize is the file's own bounds")
				var pyr: Node = world.get_node_or_null("Pyramid")
				var pmesh: MeshInstance3D = null
				var pcol: CollisionShape3D = null
				if pyr: for c in pyr.get_children():
					if c is MeshInstance3D: pmesh = c
					elif c is CollisionShape3D: pcol = c
				var pab: AABB = pmesh.mesh.get_aabb() if pmesh and pmesh.mesh else AABB()
				check(pmesh != null and pmesh.mesh is ArrayMesh and pab.size.is_equal_approx(Vector3(6, 4.5, 6)) and pab.get_center().is_zero_approx() and pmesh.transform == Transform3D(), "meshes/pyramid.obj drawn fitted to Size (6, 4.5, 6), centred on the part: %s" % pab)
				var hull := pcol.shape as ConvexPolygonShape3D if pcol else null
				check(hull != null and hull.points.size() == 5 and pcol.transform == Transform3D(), "its collision is the mesh's hull: %d points" % (hull.points.size() if hull else -1))
				var crate: Node = world.get_node_or_null("Crate")
				var cmesh: MeshInstance3D = null
				var ccol: CollisionShape3D = null
				if crate: for c in crate.get_children():
					if c is MeshInstance3D: cmesh = c
					elif c is CollisionShape3D: ccol = c
				var cab: AABB = cmesh.mesh.get_aabb() if cmesh and cmesh.mesh else AABB()
				check(cmesh != null and cmesh.mesh is ArrayMesh and cab.size.is_equal_approx(Vector3(3, 3, 3)) and cab.get_center().is_zero_approx(), "a Part with a FileMesh SpecialMesh draws meshes/cube.glb (1 stud) at Scale 3: %s" % cab)
				check(ccol != null and ccol.shape is BoxShape3D and ccol.shape.size == Vector3(3, 3, 3), "its collision stays the Part's own box")
				var orb: Node = world.get_node_or_null("Orb")
				var omesh: MeshInstance3D = null
				var ocol: CollisionShape3D = null
				if orb: for c in orb.get_children():
					if c is MeshInstance3D: omesh = c
					elif c is CollisionShape3D: ocol = c
				var oab: AABB = omesh.mesh.get_aabb() if omesh and omesh.mesh else AABB()
				check(omesh != null and (oab.size - Vector3(3, 2, 3)).length() < 0.02 and ocol != null and ocol.shape is BoxShape3D and ocol.shape.size == Vector3(2, 2, 2), "MeshType.Sphere fills the part's Size times Scale (1.5, 1, 1.5): %s over a 2 by 2 by 2 box" % oab)
				var mesh_warned := false
				for l in logs: if l.begins_with("WARN Mesh"): mesh_warned = true
				check(not mesh_warned, "both mesh files loaded: no Mesh warning")
				# turn the Board to the camera so its Front face, which carries the SurfaceGui, is in view
				var cam3 := world.get_camera()
				var fwd3: Vector3 = -cam3.global_transform.basis.z
				# Above the line from the character to the camera, not on it: on Roblox a part on that
				# line pops the camera in front of it, and this one is then behind the lens.
				board_at = cam3.global_position + fwd3 * 6 + Vector3(0, 5, 0)
				board_eye = cam3.global_position
				var eye := board_eye
				world.run_chunk("board", "workspace.Map.Board.Screen.MaxDistance = 0 workspace.Map.Board.CFrame = CFrame.lookAt(Vector3.new(%f, %f, %f), Vector3.new(%f, %f, %f))" % [board_at.x, board_at.y, board_at.z, eye.x, eye.y, eye.z])
				board_moved_at = t
				phase = 30
		30:
			if _part_at("Board", board_at) or t > board_moved_at + 4.0:
				var cam3 := world.get_camera()
				var screen: SubViewport = world.get_node_or_null("Screen")
				var face: MeshInstance3D = world.get_node_or_null("ScreenFace")
				check(screen != null and screen.size == Vector2i(800, 400) and screen.transparent_bg, "the SurfaceGui is drawn into a SubViewport the size of CanvasSize: %s" % (screen.size if screen else Vector2i()))
				var qm := face.mesh as QuadMesh if face else null
				check(qm != null and qm.size == Vector2(8, 4) and face.visible, "and painted by a quad the size of the Board's Front face, shown at MaxDistance 0 (Roblox's default: no limit): %s" % (qm.size if qm else Vector2()))
				var toward := (board_eye - board_at).normalized()
				var fxf: Transform3D = face.global_transform if face else Transform3D()
				check(face != null and fxf.basis.z.is_equal_approx(toward) and (fxf.origin - (board_at + toward * 0.26)).length() < 0.02, "the quad sits a hair off the face, its normal out of it: %s, %s" % [fxf.origin, fxf.basis.z])
				var mat := face.material_override as StandardMaterial3D if face else null
				check(mat != null and mat.albedo_texture is ViewportTexture and mat.shading_mode == BaseMaterial3D.SHADING_MODE_UNSHADED and mat.transparency == BaseMaterial3D.TRANSPARENCY_ALPHA, "wearing the viewport's texture, unshaded (LightInfluence 0)")
				var press: Control = screen.get_node_or_null("Canvas/Press") if screen else null
				var prect := press.get_global_rect() if press else Rect2()
				check(press != null and prect.position.is_equal_approx(Vector2(200, 220)) and prect.size.is_equal_approx(Vector2(400, 160)), "its TextButton laid out in canvas pixels: %s" % prect)
				# canvas pixel -> point on the quad -> screen
				var c := prect.get_center()
				var on_quad: Vector3 = fxf.origin + fxf.basis.x * ((c.x / 800.0 - 0.5) * 8) + fxf.basis.y * ((0.5 - c.y / 400.0) * 4)
				_click(cam3.unproject_position(on_quad))
				clicked_board_at = t
				phase = 31
		31:
			if _has("board pressed\t1") or t > clicked_board_at + 4.0:
				check(_has("board pressed	1"), "clicking the face presses the button: Activated on the client")
				var title: Label = null
				var screen: SubViewport = world.get_node_or_null("Screen")
				var tp: Control = screen.get_node_or_null("Canvas/Title") if screen else null
				if tp: for c in tp.get_children(): for cc in c.get_children(): if cc is Label: title = cc
				check(title != null and title.text == "Pressed: 1", "and the label on the face follows: %s" % (title.text if title else "none"))
				var face: Font = title.get_theme_font("font") if title else null
				var sys := face as SystemFont
				check(sys != null and sys.font_weight == 700 and sys.font_names.size() > 0 and sys.font_names[0] == "Gotham", "its Font = GothamBold is a bold Gotham system font: %s" % (str(sys.font_names) + " " + str(sys.font_weight) if sys else "none"))
				phase = 3
		3:
			# keep walking: the lava is ahead
			_key(KEY_W, true)
			old_body = body
			phase = 4
		4:
			if (_has("Ada	died") and _has("waved	false	0") and _has("note	lost	false")) or t > 14.0:
				_key(KEY_W, false)
				check(_has("note	lost	false"), "letting the MultiLine box go is a FocusLost(false)")
				check(_has("waved	false	0"), "AnimationTrack:Stop(fade) fades it out and ends it: nothing is playing on the Animator")
				check(_has("Ada	died"), "the lava killed the character (Humanoid.Died on the server)")
				died_at = t
				phase = 5
		5:
			if t > died_at + 6.5:   # Players.RespawnTime is 5
				check(_has("ouch"), "the client saw Died too (a frame later)")
				check(body != null and body != old_body and is_instance_valid(body), "a new character body after RespawnTime")
				var at := body.global_position if body else Vector3(99, 99, 99)
				check(Vector2(at.x, at.z - 4).length() < 1.5 and at.y > 0.5 and at.y < 5.0, "back at the spawn: %s" % at)
				var n := _meshes(body)
				check(n == 8, "the R6 limbs and the Cap ride on the new body: %d meshes %s" % [n, mesh_names])
				world.run_chunk("mv", """
					local hum = game.Players.Ada.Character.Humanoid
					hum.MoveToFinished:Connect(function(reached) print("moveto", reached, hum.MoveDirection.Magnitude) end)
					hum:MoveTo(Vector3.new(5, 4, 4))
				""")
				world.run_chunk("weld", """
					local function r(v) return Vector3.new(math.round(v.X * 100) / 100, math.round(v.Y * 100) / 100, math.round(v.Z * 100) / 100) end
					local frame = Instance.new("Part"); frame.Name = "WFrame"; frame.Anchored = true; frame.Size = Vector3.new(4, 4, 1); frame.CFrame = CFrame.new(30, 10, 0); frame.Parent = workspace
					local panel = Instance.new("Part"); panel.Name = "WPanel"; panel.Size = Vector3.new(2, 2, 1); panel.CFrame = CFrame.new(34, 10, 0); panel.Parent = workspace
					local wc = Instance.new("WeldConstraint"); wc.Part0 = frame; wc.Part1 = panel; wc.Parent = frame
					local a = Instance.new("Part"); a.Name = "WA"; a.Size = Vector3.new(2, 2, 2); a.CFrame = CFrame.new(40, 30, 0); a.Parent = workspace
					local b = Instance.new("Part"); b.Name = "WB"; b.Size = Vector3.new(1, 1, 1); b.CFrame = CFrame.new(44, 30, 0); b.Parent = workspace
					local w = Instance.new("Weld"); w.Part0 = a; w.Part1 = b; w.C0 = CFrame.new(4, 0, 0); w.Parent = a
					local sign = Instance.new("Part"); sign.Name = "Billboard"; sign.Anchored = true; sign.Size = Vector3.new(6, 3, 1); sign.CFrame = CFrame.new(-30, 10, 0); sign.Parent = workspace
					local logo = Instance.new("Decal"); logo.Name = "Logo"; logo.Texture = "images/sheet.png"; logo.Transparency = 0.25; logo.Parent = sign
					local tile = Instance.new("Texture"); tile.Name = "Tile"; tile.Texture = "images/sheet.png"; tile.Face = Enum.NormalId.Top; tile.StudsPerTileU = 3; tile.StudsPerTileV = 0.5; tile.Parent = sign
					local face = Instance.new("Decal"); face.Name = "face"; face.Texture = "images/cursor.png"; face.Parent = game.Players.Ada.Character.Head
					local torch = Instance.new("Part"); torch.Name = "Torch"; torch.Anchored = true; torch.Size = Vector3.new(1, 4, 1); torch.CFrame = CFrame.new(-36, 12, 0); torch.Parent = workspace
					local flame = Instance.new("ParticleEmitter"); flame.Name = "Flame"; flame.Rate = 30; flame.Lifetime = NumberRange.new(1, 2); flame.Speed = NumberRange.new(2, 4)
					flame.Size = NumberSequence.new(2, 0); flame.Color = ColorSequence.new(Color3.new(1, 0.5, 0), Color3.new(0.2, 0.2, 0.2)); flame.Transparency = NumberSequence.new(0, 1)
					flame.LightEmission = 1; flame.Parent = torch
					flame:Emit(10)
					local fire = Instance.new("Fire"); fire.Parent = torch
					local at = Instance.new("Attachment"); at.Name = "Tip"; at.Position = Vector3.new(0, 2, 0); at.Parent = torch
					local smoke = Instance.new("Smoke"); smoke.Name = "Puff"; smoke.Parent = at
					local glitter = Instance.new("Sparkles"); glitter.Name = "Glitter"; glitter.Parent = game.Players.Ada.Character.Head
					local glow = Instance.new("Highlight"); glow.Name = "Glow"; glow.FillColor = Color3.new(0, 0, 1); glow.FillTransparency = 0.25; glow.OutlineColor = Color3.new(1, 1, 0); glow.Parent = torch
					local pick = Instance.new("Highlight"); pick.Name = "Pick"; pick.Adornee = game.Players.Ada.Character; pick.DepthMode = Enum.HighlightDepthMode.Occluded; pick.Parent = game.ReplicatedStorage
					local dim = Instance.new("Highlight"); dim.Name = "Dim"; dim.Parent = sign
					task.wait(1.5)
					print("welded", r(panel.Position), a.Position.Y < 20, r(b.Position - a.Position), math.abs(b.Position.Y - a.Position.Y) < 0.05)
					frame.CFrame = CFrame.new(30, 12, 0) * CFrame.Angles(0, math.rad(90), 0)
					task.wait(0.2)
					print("moved", r(panel.Position), r(frame.Position))
				""")
				# a RemoteEvent fired before the client connects a handler is kept until it does, as on Roblox
				world.run_client_chunk("early", "task.spawn(function() local e = game.ReplicatedStorage:WaitForChild('Early') task.wait(1) e.OnClientEvent:Connect(function(m) print('early', m) end) end)")
				world.run_chunk("early", "local e = Instance.new('RemoteEvent') e.Name = 'Early' e.Parent = game.ReplicatedStorage task.wait(0.2) e:FireAllClients('kept') e:FireAllClients('too')")
				world.run_chunk("hinge", """
					local function part(name, size, pos, anchored) local p = Instance.new("Part"); p.Name = name; p.Size = size; p.Position = pos; p.Anchored = anchored; p.Parent = workspace; return p end
					local function att(parent, pos, orient) local a = Instance.new("Attachment", parent); a.Position = pos; a.Orientation = orient or Vector3.new(0, 0, 0); return a end
					local post = part("Post", Vector3.new(1, 8, 1), Vector3.new(-30, 4, 20), true)
					local door = part("Door", Vector3.new(4, 7, 0.5), Vector3.new(-27.5, 4, 20), false)
					local h = Instance.new("HingeConstraint"); h.Name = "DoorHinge"
					h.Attachment0 = att(post, Vector3.new(0.5, 0, 0), Vector3.new(0, 0, 90)); h.Attachment1 = att(door, Vector3.new(-2, 0, 0), Vector3.new(0, 0, 90))
					h.ActuatorType = Enum.ActuatorType.Motor; h.AngularVelocity = 3; h.MotorMaxTorque = 5000; h.LimitsEnabled = true; h.LowerAngle = -45; h.UpperAngle = 90
					h.Parent = post
					local pole = part("Pole", Vector3.new(1, 8, 1), Vector3.new(-30, 4, 30), true)
					local flap = part("Flap", Vector3.new(4, 1, 1), Vector3.new(-27.5, 4, 30), false)
					local s = Instance.new("HingeConstraint"); s.Name = "FlapServo"
					s.Attachment0 = att(pole, Vector3.new(0.5, 0, 0), Vector3.new(0, 0, 90)); s.Attachment1 = att(flap, Vector3.new(-2, 0, 0), Vector3.new(0, 0, 90))
					s.ActuatorType = Enum.ActuatorType.Servo; s.AngularSpeed = 4; s.ServoMaxTorque = 5000; s.TargetAngle = 60
					s.Parent = pole
					local hook = part("Hook", Vector3.new(1, 1, 1), Vector3.new(-30, 12, 40), true)
					local bob = part("Bob", Vector3.new(1, 1, 1), Vector3.new(-30, 9, 40), false)
					local bs = Instance.new("BallSocketConstraint"); bs.Attachment0 = att(hook, Vector3.new(0, 0, 0)); bs.Attachment1 = att(bob, Vector3.new(0, 3, 0)); bs.Parent = hook
					bob.AssemblyLinearVelocity = Vector3.new(16, 0, 0)
					-- a rope the load falls onto, a rod the tip swings on, a spring the weight settles on
					local crane = part("Crane", Vector3.new(1, 1, 1), Vector3.new(-30, 14, 50), true)
					local load = part("Load", Vector3.new(1, 1, 1), Vector3.new(-30, 8, 50), false)
					local rope = Instance.new("RopeConstraint"); rope.Name = "Rope"; rope.Attachment0 = att(crane, Vector3.new(0, 0, 0)); rope.Attachment1 = att(load, Vector3.new(0, 0.5, 0)); rope.Length = 5; rope.Parent = crane
					local beam = part("Beam", Vector3.new(1, 1, 1), Vector3.new(-30, 14, 60), true)
					local tip = part("Tip", Vector3.new(1, 1, 1), Vector3.new(-25, 14, 60), false)
					local rod = Instance.new("RodConstraint"); rod.Attachment0 = att(beam, Vector3.new(0, 0, 0)); rod.Attachment1 = att(tip, Vector3.new(0, 0, 0)); rod.Length = 5; rod.Parent = beam
					local ceiling = part("Ceiling", Vector3.new(1, 1, 1), Vector3.new(-30, 14, 70), true)
					local weight = part("Weight", Vector3.new(1, 1, 1), Vector3.new(-30, 10, 70), false)
					local spring = Instance.new("SpringConstraint"); spring.Attachment0 = att(ceiling, Vector3.new(0, 0, 0)); spring.Attachment1 = att(weight, Vector3.new(0, 0, 0))
					spring.FreeLength = 4; spring.Stiffness = 200; spring.Damping = 20; spring.Parent = ceiling
					-- the movers: a VectorForce of GetMass() * Gravity hovers a part an AngularVelocity turns,
					-- a LinearVelocity carries a crate through the air, an AlignPosition / AlignOrientation
					-- (rigid, OneAttachment) puts a part where they say
					local hover = part("Hover", Vector3.new(4, 1, 2), Vector3.new(-30, 6, 80), false)
					local vf = Instance.new("VectorForce"); vf.Attachment0 = att(hover, Vector3.new(0, 0, 0)); vf.RelativeTo = Enum.ActuatorRelativeTo.World
					vf.Force = Vector3.new(0, hover:GetMass() * workspace.Gravity, 0); vf.ApplyAtCenterOfMass = true; vf.Parent = hover
					local av = Instance.new("AngularVelocity"); av.Attachment0 = vf.Attachment0; av.AngularVelocity = Vector3.new(0, 0.5, 0); av.MaxTorque = 1e5; av.Parent = hover
					local crate2 = part("Crate2", Vector3.new(1, 1, 1), Vector3.new(-34, 6, 90), false)
					local lv = Instance.new("LinearVelocity"); lv.Attachment0 = att(crate2, Vector3.new(0, 0, 0)); lv.VectorVelocity = Vector3.new(4, 0, 0); lv.MaxForce = 1e5; lv.Parent = crate2
					local follower = part("Follower", Vector3.new(1, 1, 1), Vector3.new(-30, 3, 100), false)
					local ap = Instance.new("AlignPosition"); ap.Attachment0 = att(follower, Vector3.new(0, 0, 0)); ap.Mode = Enum.PositionAlignmentMode.OneAttachment
					ap.Position = Vector3.new(-20, 10, 100); ap.RigidityEnabled = true; ap.Parent = follower
					local ao = Instance.new("AlignOrientation"); ao.Attachment0 = ap.Attachment0; ao.Mode = Enum.OrientationAlignmentMode.OneAttachment
					ao.CFrame = CFrame.Angles(0, math.rad(90), 0); ao.RigidityEnabled = true; ao.Parent = follower
					task.delay(1.5, function()
						print("belt", crate2.Position.X > -30 and crate2.Position.X < -24, math.abs(crate2.Position.Y - 6) < 0.3)
					end)
					-- the legacy BodyMovers, in the part itself: a BodyPosition and a BodyGyro hold a
					-- tipped part at a point, upright; a BodyVelocity carries a crate level; a BodyForce
					-- of the weight hovers a part a BodyAngularVelocity spins
					local held = part("Held", Vector3.new(4, 1, 2), Vector3.new(-30, 4, 180), false); held.Orientation = Vector3.new(0, 0, 50)
					local bp = Instance.new("BodyPosition"); bp.Position = Vector3.new(-30, 12, 180); bp.MaxForce = Vector3.new(1e5, 1e5, 1e5); bp.Parent = held
					local bg = Instance.new("BodyGyro"); bg.MaxTorque = Vector3.new(1e5, 1e5, 1e5); bg.CFrame = CFrame.new(); bg.Parent = held
					local crate3 = part("Crate3", Vector3.new(1, 1, 1), Vector3.new(-34, 6, 190), false)
					local bvel = Instance.new("BodyVelocity"); bvel.Velocity = Vector3.new(4, 0, 0); bvel.MaxForce = Vector3.new(1e5, 1e5, 1e5); bvel.Parent = crate3
					-- a Trail between two attachments on the moving crate: a ribbon behind it
					local ta = Instance.new("Attachment"); ta.Position = Vector3.new(0, 0.5, 0); ta.Parent = crate3
					local tb = Instance.new("Attachment"); tb.Position = Vector3.new(0, -0.5, 0); tb.Parent = crate3
					local trail = Instance.new("Trail"); trail.Attachment0 = ta; trail.Attachment1 = tb; trail.Lifetime = 3
					trail.Texture = "images/sheet.png"; trail.WidthScale = NumberSequence.new(1, 0); trail.Parent = crate3   -- textured, tapering to nothing at its tail
					-- a Beam between two attachments: a ribbon of Segments quads bowed CurveSize0 along Attachment0's X (turned to point up here)
					local ba = part("BeamA", Vector3.new(1, 1, 1), Vector3.new(-40, 10, 260), true)
					local bb = part("BeamB", Vector3.new(1, 1, 1), Vector3.new(-30, 10, 260), true)
					local a0 = Instance.new("Attachment"); a0.Orientation = Vector3.new(0, 0, 90); a0.Parent = ba
					local a1 = Instance.new("Attachment"); a1.Parent = bb
					local arc = Instance.new("Beam"); arc.Name = "Arc"; arc.Attachment0 = a0; arc.Attachment1 = a1; arc.Width0 = 2; arc.Width1 = 0.5; arc.CurveSize0 = 6; arc.Segments = 8
					arc.Texture = "images/sheet.png"; arc.TextureMode = Enum.TextureMode.Wrap; arc.TextureLength = 2; arc.TextureSpeed = 0; arc.Parent = ba
					local top = part("Top", Vector3.new(2, 2, 2), Vector3.new(-30, 6, 200), false)
					local bforce = Instance.new("BodyForce"); bforce.Force = Vector3.new(0, top:GetMass() * workspace.Gravity, 0); bforce.Parent = top
					local bav = Instance.new("BodyAngularVelocity"); bav.AngularVelocity = Vector3.new(0, 3, 0); bav.MaxTorque = Vector3.new(1e5, 1e5, 1e5); bav.Parent = top
					-- a NoCollisionConstraint lets its two parts pass through each other, and nothing else
					local shelf = part("Shelf", Vector3.new(6, 1, 6), Vector3.new(-30, 6, 210), true)
					part("Shelf2", Vector3.new(6, 1, 6), Vector3.new(-20, 6, 210), true)
					local ghost = part("Ghost", Vector3.new(2, 2, 2), Vector3.new(-30, 12, 210), false)
					local solid = part("Solid", Vector3.new(2, 2, 2), Vector3.new(-20, 12, 210), false)
					local nc = Instance.new("NoCollisionConstraint"); nc.Part0 = shelf; nc.Part1 = ghost; nc.Parent = ghost
					task.delay(2, function() print("nocollide", ghost.Position.Y < 4, math.abs(solid.Position.Y - 7.5) < 0.5) end)
					-- what a part weighs, alone and with what it is welded to
					local heavy = part("Heavy", Vector3.new(2, 2, 2), Vector3.new(-30, 1, 220), true)
					local light = part("Light", Vector3.new(1, 1, 1), Vector3.new(-30, 2.5, 220), false)
					local wc2 = Instance.new("WeldConstraint"); wc2.Part0 = heavy; wc2.Part1 = light; wc2.Parent = heavy
					print("assembly", heavy.Mass, light.AssemblyMass, light.AssemblyRootPart == heavy, #light:GetConnectedParts(), heavy:GetRootPart() == heavy)
					-- ApplyImpulse throws a resting part up by impulse / mass; ApplyAngularImpulse spins one
					local thrown = part("Thrown", Vector3.new(2, 2, 2), Vector3.new(-30, 6, 230), false)
					local spun = part("Spun", Vector3.new(2, 2, 2), Vector3.new(-20, 6, 230), false)
					local bfs = Instance.new("BodyForce"); bfs.Force = Vector3.new(0, spun:GetMass() * workspace.Gravity, 0); bfs.Parent = spun
					local bft = Instance.new("BodyForce"); bft.Force = Vector3.new(0, thrown:GetMass() * workspace.Gravity, 0); bft.Parent = thrown   -- no ground out here: held against gravity
					task.delay(0.5, function()
						thrown:ApplyImpulse(Vector3.new(0, thrown:GetMass() * 60, 0))        -- 60 studs/s upward, from rest
						spun:ApplyAngularImpulse(Vector3.new(0, 40, 0))
					end)
					task.delay(1.2, function()   -- held against gravity, it keeps the 60 studs/s the impulse gave it
						print("impulse", thrown.AssemblyLinearVelocity.Y > 40 and thrown.AssemblyLinearVelocity.Y < 75, spun.AssemblyAngularVelocity.Magnitude > 3, thrown.AssemblyLinearVelocity.Y, spun.AssemblyAngularVelocity.Magnitude)
					end)
					-- the old Rotate joint is a free hinge about C0's Z: a door on its jamb swings when pushed
					local jamb = part("Jamb", Vector3.new(1, 8, 1), Vector3.new(-30, 4, 240), true)
					local door = part("Door", Vector3.new(4, 7, 0.4), Vector3.new(-27.5, 4, 240), false)
					local hinge = Instance.new("Rotate"); hinge.Part0 = jamb; hinge.Part1 = door
					hinge.C0 = CFrame.new(0.5, 0, 0) * CFrame.Angles(math.rad(90), 0, 0)
					hinge.C1 = CFrame.new(-2, 0, 0) * CFrame.Angles(math.rad(90), 0, 0)
					hinge.Parent = jamb
					task.delay(0.5, function() door:ApplyImpulse(Vector3.new(0, 0, door:GetMass() * 10)) end)
					-- the legacy powered joints: a RotateV spins at its Motor face's ParamB, a RotateP serves to DesiredAngle
					local hub = part("Hub", Vector3.new(1, 1, 1), Vector3.new(-30, 4, 250), true)
					hub.RightSurfaceInput = Enum.InputType.Constant; hub.RightParamB = 3
					local blade = part("Blade", Vector3.new(0.4, 6, 0.4), Vector3.new(-28.5, 4, 250), false)
					local fan = Instance.new("RotateV"); fan.Part0 = hub; fan.Part1 = blade
					fan.C0 = CFrame.new(0.5, 0, 0) * CFrame.Angles(0, math.rad(90), 0)
					fan.C1 = CFrame.new(-1, 0, 0) * CFrame.Angles(0, math.rad(90), 0)
					fan.Parent = hub
					local hub2 = part("Hub2", Vector3.new(1, 1, 1), Vector3.new(-30, 4, 264), true)
					local blade2 = part("Blade2", Vector3.new(0.4, 6, 0.4), Vector3.new(-28.5, 4, 264), false)
					local servo = Instance.new("RotateP"); servo.Part0 = hub2; servo.Part1 = blade2
					servo.C0 = CFrame.new(0.5, 0, 0) * CFrame.Angles(0, math.rad(90), 0)
					servo.C1 = CFrame.new(-1, 0, 0) * CFrame.Angles(0, math.rad(90), 0)
					servo.DesiredAngle = math.rad(90); servo.MaxVelocity = 0.1
					servo.Parent = hub2
					task.delay(2, function()
						local spin = blade.AssemblyAngularVelocity.Magnitude   -- 3 radians a second, whatever angle it happens to be at
						local served = math.abs(math.abs(blade2.Orientation.X) - 90) < 15 or math.abs(math.abs(blade2.Orientation.Z) - 90) < 15
						local still = blade2.AssemblyAngularVelocity.Magnitude < 0.5
						print("rotatev", spin > 2 and spin < 4.5, served and still, blade.Position.Y > 3 and blade.Position.Y < 5, spin, blade2.Orientation, blade2.AssemblyAngularVelocity.Magnitude)
					end)
					task.delay(1.5, function()
						print("door", math.abs(door.Orientation.Y) > 15, (door.Position - jamb.Position).Magnitude < 3.5, math.abs(door.Position.Y - 4) < 1)
					end)
					local turned = false
					task.delay(2, function()
						print("bodymovers", (held.Position - Vector3.new(-30, 12, 180)).Magnitude < 0.6, math.abs(held.Orientation.Z) < 5,
							crate3.Position.X > -30 and crate3.Position.X < -24, math.abs(crate3.Position.Y - 6) < 0.3,
							math.abs(top.Position.Y - 6) < 0.3, math.abs(top.Orientation.Y) > 1)
					end)
					-- Touched names the welded part that was hit, not its assembly's root
					local pad = part("Pad", Vector3.new(10, 1, 6), Vector3.new(-30, -0.5, 160), true)
					local wbase = part("Base", Vector3.new(2, 1, 2), Vector3.new(-32, 3, 160), false)
					local nose = part("Nose", Vector3.new(2, 1, 2), Vector3.new(-28, 3, 160), false)
					local weld = Instance.new("WeldConstraint"); weld.Part0 = wbase; weld.Part1 = nose; weld.Parent = wbase
					local hit, noseHit = {}, false
					pad.Touched:Connect(function(p) hit[p.Name] = true end)
					nose.Touched:Connect(function(p) if p.Name == "Pad" then noseHit = true end end)
					task.delay(2, function() print("welded touch", hit.Nose == true, hit.Base == true, noseHit) end)
					-- a PrismaticConstraint slides along the axis, a CylindricalConstraint turns about it too
					local rail = part("Rail", Vector3.new(8, 1, 2), Vector3.new(-30, 6, 140), true)
					local car = part("Car", Vector3.new(2, 1, 2), Vector3.new(-30, 6, 140), false)
					local pc = Instance.new("PrismaticConstraint"); pc.Name = "Slide"
					pc.Attachment0 = att(rail, Vector3.new(0, 0, 0)); pc.Attachment1 = att(car, Vector3.new(0, 0, 0))
					pc.ActuatorType = Enum.ActuatorType.Servo; pc.TargetPosition = 3; pc.Speed = 4; pc.ServoMaxForce = 100000
					pc.Parent = rail
					local drum = part("Drum", Vector3.new(1, 4, 1), Vector3.new(-30, 6, 150), true)
					local barrel = part("Barrel", Vector3.new(2, 2, 2), Vector3.new(-30, 6, 150), false)
					local cc = Instance.new("CylindricalConstraint"); cc.Name = "Spin"
					cc.Attachment0 = att(drum, Vector3.new(0, 0, 0)); cc.Attachment1 = att(barrel, Vector3.new(0, 0, 0))
					cc.AngularActuatorType = Enum.ActuatorType.Servo; cc.TargetAngle = 60; cc.AngularSpeed = 2; cc.ServoMaxTorque = 100000
					cc.Parent = drum
					task.delay(3, function()
						print("prismatic", math.abs(car.Position.X + 27) < 0.6, math.abs(car.Position.Y - 6) < 0.3, math.abs(pc.CurrentPosition - 3) < 0.6)
						print("cylindrical", math.abs(cc.CurrentAngle - 60) < 10, math.abs(barrel.Position.Y - 6) < 0.3)
					end)
					-- a material rubs as Roblox says it does: Ice slides where Plastic stops
					local rink = part("Rink", Vector3.new(40, 1, 8), Vector3.new(-20, -0.5, 120), true); rink.Material = Enum.Material.Ice
					part("Rough", Vector3.new(40, 1, 8), Vector3.new(-20, -0.5, 130), true)
					local puck = part("Puck", Vector3.new(2, 2, 2), Vector3.new(-34, 1, 120), false); puck.Material = Enum.Material.Ice
					local box = part("Box", Vector3.new(2, 2, 2), Vector3.new(-34, 1, 130), false)
					puck.AssemblyLinearVelocity = Vector3.new(14, 0, 0)
					box.AssemblyLinearVelocity = Vector3.new(14, 0, 0)
					task.delay(2.5, function()
						print("slide", puck.Position.X - box.Position.X > 5, math.abs(box:GetMass() - 5.6) < 0.01, math.abs(puck.Position.Y - 1) < 0.2)
					end)
					-- a VehicleSeat drives its assembly: Throttle along its look vector, Steer turning it
					local road = part("Road", Vector3.new(40, 1, 8), Vector3.new(-20, -0.5, 110), true)
					local kart = Instance.new("VehicleSeat"); kart.Name = "Kart"; kart.Size = Vector3.new(4, 1, 2); kart.Position = Vector3.new(-34, 0.5, 110)
					kart.Orientation = Vector3.new(0, -90, 0); kart.MaxSpeed = 8; kart.Parent = workspace
					local hood = part("Hood", Vector3.new(2, 0.5, 2), Vector3.new(-31, 0.25, 110), false)
					local bolt = Instance.new("WeldConstraint"); bolt.Part0 = kart; bolt.Part1 = hood; bolt.Parent = kart
					kart.Throttle = 1
					task.delay(1, function()
						print("drive", kart.Position.X > -30 and kart.Position.X < -24, math.abs(kart.Position.Y - 0.5) < 0.2, math.abs(hood.Position.X - kart.Position.X - 3) < 0.3, kart.Position)
						kart.Throttle = 0; kart.Steer = 1
						task.delay(0.6, function() print("steer", math.abs(kart.Orientation.Y + 90) > 10 and math.abs(kart.Orientation.Y + 90) < 60) end)
					end)
					local rodOff, tipLow = 0, 99
					game:GetService("RunService").Heartbeat:Connect(function()
						rodOff = math.max(rodOff, math.abs((tip.Position - beam.Position).Magnitude - 5)); tipLow = math.min(tipLow, tip.Position.Y)
					end)
					task.delay(3, function()
						local r1 = function(v) return math.floor(v * 10 + 0.5) / 10 end
						print("rope", r1((load.Position + Vector3.new(0, 0.5, 0) - crane.Position).Magnitude), r1(rope.CurrentDistance), load.Position.Y < 9)
						print("rod", rodOff < 0.5, tipLow < 10, r1(rod.CurrentDistance))
						print("spring", r1((weight.Position - ceiling.Position).Magnitude), r1(spring.CurrentLength))
						print("hover", hover:GetMass(), math.abs(hover.Position.Y - 6) < 0.5, hover.Orientation.Y > 30 and hover.Orientation.Y < 150)
						print("align", r1((follower.Position - Vector3.new(-20, 10, 100)).Magnitude) < 0.3, math.abs(follower.Orientation.Y - 90) < 3)
					end)
					local swung = 0
					game:GetService("RunService").Heartbeat:Connect(function() swung = math.max(swung, math.abs(bob.Position.X - hook.Position.X)) end)
					task.delay(3, function()
						print("door", h.CurrentAngle > 5 and h.CurrentAngle <= 92, math.abs(door.Orientation.Y) > 5, math.floor(s.CurrentAngle + 0.5) >= 55 and math.floor(s.CurrentAngle + 0.5) <= 65)
						print("pendulum", math.floor((bob.Position - hook.Position).Magnitude + 0.5), swung > 1, bob.Position.Y < 12)
					end)
				""")
				moveto_at = t
				phase = 6
		6:
			# the nearest it got: a falling brick can shove it on after it arrives
			if body: moveto_best = minf(moveto_best, Vector2(body.global_position.x - 5, body.global_position.z - 4).length())
			if _has("moveto\t") or t > moveto_at + 4.0:
				check(_has("moveto	true	0"), "Humanoid:MoveTo fires MoveToFinished(true) once there, MoveDirection back to zero")
				check(moveto_best < 0.8, "and the character walked to the point: %.2f studs off at the nearest" % moveto_best)
				# any Model with a Humanoid and a HumanoidRootPart is a character the engine moves
				world.run_chunk("npc", """
					local npc = Instance.new("Model"); npc.Name = "Guard"
					Instance.new("Humanoid").Parent = npc
					local root = Instance.new("Part"); root.Name = "HumanoidRootPart"; root.Size = Vector3.new(2, 2, 1)
					root.Position = Vector3.new(-4, 7, 4); root.Transparency = 1; root.Parent = npc
					local torso = Instance.new("Part"); torso.Name = "Torso"; torso.Size = Vector3.new(2, 2, 1); torso.Position = root.Position; torso.Parent = npc
					npc.PrimaryPart = root
					npc.Parent = workspace
					npc.Humanoid:Move(Vector3.new(0, 0, -1))
					task.delay(1, function() print("guard", math.round(root.Position.X), math.round(root.Position.Z), npc.Humanoid.MoveDirection.Z) end)
				""")
				# an R15 rig is these fifteen part names; the engine turns each limb as one chain
				world.run_chunk("r15", """
					local npc = Instance.new("Model"); npc.Name = "Runner"
					Instance.new("Humanoid").Parent = npc
					local at = Vector3.new(-9, 7.2, 4)
					local limbs = {
						{"HumanoidRootPart", Vector3.new(2, 2, 1), Vector3.new(0, 0, 0)},
						{"Head", Vector3.new(2, 1, 1), Vector3.new(0, 2.3, 0)},
						{"UpperTorso", Vector3.new(2, 1.6, 1), Vector3.new(0, 1, 0)},
						{"LowerTorso", Vector3.new(2, 0.4, 1), Vector3.new(0, 0, 0)},
						{"RightUpperArm", Vector3.new(1, 1.2, 1), Vector3.new(1.5, 1, 0)},
						{"RightLowerArm", Vector3.new(1, 1.2, 1), Vector3.new(1.5, -0.2, 0)},
						{"RightHand", Vector3.new(1, 0.4, 1), Vector3.new(1.5, -1, 0)},
						{"LeftUpperArm", Vector3.new(1, 1.2, 1), Vector3.new(-1.5, 1, 0)},
						{"LeftLowerArm", Vector3.new(1, 1.2, 1), Vector3.new(-1.5, -0.2, 0)},
						{"LeftHand", Vector3.new(1, 0.4, 1), Vector3.new(-1.5, -1, 0)},
						{"RightUpperLeg", Vector3.new(1, 1.4, 1), Vector3.new(0.5, -0.9, 0)},
						{"RightLowerLeg", Vector3.new(1, 1.4, 1), Vector3.new(0.5, -2.3, 0)},
						{"RightFoot", Vector3.new(1, 0.2, 1), Vector3.new(0.5, -3.1, 0)},
						{"LeftUpperLeg", Vector3.new(1, 1.4, 1), Vector3.new(-0.5, -0.9, 0)},
						{"LeftLowerLeg", Vector3.new(1, 1.4, 1), Vector3.new(-0.5, -2.3, 0)},
						{"LeftFoot", Vector3.new(1, 0.2, 1), Vector3.new(-0.5, -3.1, 0)},
					}
					for _, l in limbs do
						local p = Instance.new("Part"); p.Name = l[1]; p.Size = l[2]; p.Position = at + l[3]
						if l[1] == "HumanoidRootPart" then p.Transparency = 1 end
						p.Parent = npc
					end
					npc.PrimaryPart = npc.HumanoidRootPart
					npc.Parent = workspace
					npc.Humanoid:Move(Vector3.new(0, 0, -1))
				""")
				npc_at = t
				world.run_client_chunk("cam", "workspace.CurrentCamera.CameraSubject = workspace:WaitForChild('Guard'):WaitForChild('Humanoid')")
				phase = 7
		7:
			if (_has("guard\t") and _has("welded\t") and _r15_arm_swung()) or t > npc_at + 6.0:
				var guard := ""
				for l in logs: if l.find("guard	") != -1: guard = l.substr(l.find("guard	") + 6)
				var f := guard.split("	")
				check(f.size() == 3 and absi(f[0].to_int() + 4) <= 3 and f[1].to_int() < -2 and f[2] == "-1", "Humanoid:Move walks an NPC that way until told otherwise: %s" % guard)
				var ws_id := 0
				for sid in world.get_child_ids(0):
					if world.get_instance(sid).get("class_name", "") == "Workspace": ws_id = sid
				var guard_id := 0
				for kid in world.get_child_ids(ws_id):
					if world.get_instance(kid).get("name", "") == "Guard": guard_id = kid
				var groot_id := 0
				for kid in world.get_child_ids(guard_id):
					if world.get_instance(kid).get("name", "") == "HumanoidRootPart": groot_id = kid
				var gnode: Node3D = world.get_part_node(groot_id)
				var cam3d: Camera3D = world.get_camera()
				var dcam: float = cam3d.global_position.distance_to(gnode.global_position) if gnode != null else 1e9
				var aim: float = 0.0
				if gnode != null:
					aim = (-cam3d.global_transform.basis.z).dot((gnode.global_position + Vector3(0, 1.5, 0) - cam3d.global_position).normalized())
				# Default 12.5 back from the focus; nearer when the map is in the way, as on Roblox.
				check(dcam > 3.0 and dcam < 16.0 and aim > 0.98, "CameraSubject = the guard's Humanoid: the camera orbits the guard, %.1f studs off and looking at him (%.3f)" % [dcam, aim])
				world.run_client_chunk("cam2", "workspace.CurrentCamera.CameraSubject = game.Players.LocalPlayer.Character.Humanoid")
				var upper: MeshInstance3D = null
				var lower: MeshInstance3D = null
				var hand: MeshInstance3D = null
				for c in world.get_children():
					if c is CharacterBody3D:
						for m in c.get_children():
							if m is MeshInstance3D and m.name == "RightUpperArm": upper = m
							if m is MeshInstance3D and m.name == "RightLowerArm": lower = m
							if m is MeshInstance3D and m.name == "RightHand": hand = m
				var turn := absf(upper.transform.basis.get_euler().x) if upper else 0.0
				check(upper != null and lower != null and hand != null, "an R15 rig's fifteen parts ride the body")
				check(turn > 0.1 and lower != null and absf(lower.transform.basis.get_euler().x - upper.transform.basis.get_euler().x) < 0.01
					and hand != null and absf(hand.transform.basis.get_euler().x - upper.transform.basis.get_euler().x) < 0.01,
					"and its arm swings as one chain from the shoulder: %.2f rad" % turn)
				var sun: DirectionalLight3D = world.get_node("Sun")
				var env: WorldEnvironment = world.get_node("Lighting")
				check(sun != null and env != null and env.environment != null, "the world brings a sun and an environment")
				noon_energy = sun.light_energy if sun else 0.0
				var head: Node = body.get_node_or_null("Head") if body else null
				check(head != null and head.get_node_or_null("face") is MeshInstance3D and head.get_node("face").visible, "a Decal in the character's Head rides the head")
				var head_box: AABB = head.mesh.get_aabb() if head is MeshInstance3D and head.mesh != null else AABB()
				var head_tris: int = head.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX].size() / 3 if head is MeshInstance3D and head.mesh != null and head.mesh.get_surface_count() > 0 else 0
				check(absf(head_box.size.x - 2.0) < 0.01 and absf(head_box.size.y - 1.732) < 0.01 and absf(head_box.size.z - 1.0) < 0.01 and absf(head_box.position.y + 0.866 - 0.37) < 0.01 and head_tris == 20,
					"the Head is the PulseChain hexagon made solid (Head.Mesh, MeshType Head): a flat-topped prism 2 wide, 1.73 tall, 1 deep, lifted 0.37 to sit on the torso, 20 flat triangles: %s, %d" % [head_box, head_tris])
				var torch: Node3D = world.get_node_or_null("Torch")
				var flame: GPUParticles3D = null
				var burst: GPUParticles3D = null
				var fire: GPUParticles3D = null
				var puff: GPUParticles3D = null
				if torch: for c in torch.get_children(): if c is MeshInstance3D: flame = c.get_node_or_null("Flame"); burst = c.get_node_or_null("Flame Burst"); fire = c.get_node_or_null("Fire"); puff = c.get_node_or_null("Puff")
				check(flame != null and flame.emitting and flame.amount == 60 and is_equal_approx(flame.lifetime, 2.0) and flame.process_material is ParticleProcessMaterial and flame.process_material.direction == Vector3(0, 1, 0) and flame.process_material.initial_velocity_min == 2.0 and flame.process_material.initial_velocity_max == 4.0, "a ParticleEmitter is a GPUParticles3D under the part: Rate x Lifetime particles, going EmissionDirection at Speed")
				check(flame != null and flame.process_material.scale_curve != null and flame.process_material.color_ramp != null and flame.draw_pass_1 is QuadMesh and flame.draw_pass_1.material.blend_mode == BaseMaterial3D.BLEND_MODE_ADD, "Size / Color / Transparency are its curves; LightEmission blends additively")
				check(burst != null and burst.one_shot and burst.amount == 10, "Emit(10) bursts ten from a one-shot twin")
				check(fire != null and fire.emitting and puff != null and puff.emitting and is_equal_approx(puff.position.y, 2.0), "Fire in the part, Smoke in an Attachment at its Position")
				check(head != null and head.get_node_or_null("Glitter") is GPUParticles3D, "Sparkles in the character's Head ride the head")
				world.run_chunk("lt", """
					local L = game:GetService("Lighting")
					L.TimeOfDay = "07:00:00"
					L.FogEnd = 300
					L.FogColor = Color3.new(1, 0, 0)
					L.GlobalShadows = false
					task.wait()
					print("clock", L.ClockTime)
					local config = game.ReplicatedStorage:FindFirstChild("Config")
					print("project", L.Ambient == Color3.new(0.2, 0.2, 0.25), config and config.ClassName, config and config:GetAttribute("MaxPlayers"))
					local fx = game.ReplicatedStorage:FindFirstChild("Fixtures")
					print("rbxm", fx and fx.ClassName, fx and fx:GetAttribute("String"), fx and fx:GetAttribute("Vector3"), fx and fx:GetAttribute("UDim2"))
					local bloom = Instance.new("BloomEffect", L)
					bloom.Intensity = 1
					local cc = Instance.new("ColorCorrectionEffect", L)
					cc.Saturation = -1
					cc.TintColor = Color3.fromRGB(255, 200, 150)
					Instance.new("DepthOfFieldEffect", L).FocusDistance = 30
					Instance.new("BlurEffect", L).Size = 8
					Instance.new("BlurEffect", L).Enabled = false
				""")
				lt_at = t
				phase = 8
		8:
			if (_has("clock\t") and world.get_node_or_null("Blur") != null) or t > lt_at + 5.0:
				var sun: DirectionalLight3D = world.get_node("Sun")
				var env: WorldEnvironment = world.get_node("Lighting")
				var dir: Vector3 = -sun.global_transform.basis.z
				check(_has("clock	7"), "TimeOfDay writes ClockTime")
				check(_has("project	true	Configuration	8"), "default.project.json's $properties / $className / $attributes reached Lighting and ReplicatedStorage.Config")
				# a Studio .rbxmx in src/map
				var altar: Node3D = world.get_node_or_null("Altar")
				var glow: OmniLight3D = altar.find_child("Glow", true, false) if altar else null
				check(altar != null and altar.global_position.is_equal_approx(Vector3(30, 1.5, 30)) and glow != null and glow.omni_range == 12.0, "Shrine.rbxmx: the Altar stands at (30, 1.5, 30) with its PointLight")
				check(_has("shrine	Altar	Neon	30, 1.5, 30"), "and the Script inside the model ran")
				check(_has("rbxm	Folder	Hello, world!	1, 2, 3	{0.5, 10}, {0.7, 30}"), "Fixtures.rbxm (binary) in src/shared: a Folder with its attributes")
				check(dir.y < -0.2 and dir.x < -0.9 and not sun.shadow_enabled, "at 7:00 the sun is low in the east, GlobalShadows off: %s" % dir)
				check(sun.light_energy > 0.3 and sun.light_energy < 1.0, "and dimmer than at 14:00 (%.2f): %.2f" % [noon_energy, sun.light_energy])
				check(env.environment.fog_enabled and env.environment.fog_depth_end == 300.0 and env.environment.fog_light_color.r == 1.0, "FogEnd / FogColor reach the environment's fog")
				check(env.environment.glow_enabled and env.environment.glow_intensity == 2.0 and env.environment.get_glow_level(2) == 1.0 and env.environment.get_glow_level(3) == 0.0, "a BloomEffect in Lighting is the environment's glow (Intensity 1 -> 2, Size 24 -> the first 3 of Godot's levels 0..6)")
				check(env.environment.adjustment_enabled and env.environment.adjustment_saturation == 0.0 and env.environment.adjustment_color_correction != null, "a ColorCorrectionEffect: Saturation -1 greys it, the TintColor is a ramp")
				var cam: Camera3D = world.get_node("Camera")
				check(cam.attributes != null and cam.attributes.dof_blur_far_enabled and cam.attributes.dof_blur_far_distance == 40.0 and cam.attributes.dof_blur_near_distance == 20.0, "a DepthOfFieldEffect: focus at 30 +- 10 on the camera")
				var blur: CanvasLayer = world.get_node_or_null("Blur")
				check(blur != null and blur.visible and blur.layer == -1 and blur.get_child(0).material.get_shader_parameter("lod") == 3.0, "a BlurEffect: a screen quad under the GUIs, Size 8 -> mip 3 (the disabled one is skipped)")
				world.run_chunk("atmo", """
					local L = game:GetService("Lighting")
					L.BloomEffect:Destroy()
					L.BlurEffect.Size = 0
					local a = Instance.new("Atmosphere", L)
					a.Density = 0.5
					a.Haze = 5
					a.Color = Color3.new(0, 0, 1)
					Instance.new("Sky", L).SunAngularSize = 60
					workspace.Billboard.Dim.Enabled = false
					local crate = Instance.new("Part"); crate.Name = "Crate"; crate.Size = Vector3.new(2, 2, 2); crate.CFrame = CFrame.new(-50, 1, 0); crate.Parent = workspace
					local bomb = Instance.new("Explosion"); bomb.Name = "Boom"; bomb.Position = Vector3.new(-50, 0, 1.5); bomb.BlastRadius = 8
					bomb.Hit:Connect(function(part, dist) if part == crate then print("boomhit", math.floor(dist)) end end)
					bomb.Parent = workspace
					task.delay(0.6, function() print("flung", crate.Position.Y > 2 or crate.Position.Z < -1, bomb.Parent) end)
				""")
				# writes to data_store_path; the second world in phase 9 reads it back
				world.run_chunk("ds", """
					local ds = game:GetService("DataStoreService"):GetDataStore("PlayerData")
					ds:SetAsync("7", {coins = ds:IncrementAsync("visits") * 10})
					print("stored", ds:GetAsync("7").coins)
				""")
				phase = 40
		40:
			# W is held under the open menu: the character must not hear it
			_key(KEY_ESCAPE, true); _key(KEY_ESCAPE, false)
			_key(KEY_W, true)
			menu_at = t
			if body: menu_pos = body.global_position
			phase = 41
		41:
			if (t > menu_at + 0.4 and _has("menu\topened\ttrue")) or t > menu_at + 3.0:
				_key(KEY_W, false)
				var menu: Control = world.get_node_or_null("CoreGui/Menu")
				var topbar: Control = world.get_node_or_null("CoreGui/Topbar")
				check(world.is_menu_open() and menu != null and menu.visible, "Escape opens the menu")
				check(topbar != null and topbar.visible and topbar.get_node("Buttons/Menu") != null and topbar.get_node("Buttons/Chat") != null, "the top bar has the menu and chat buttons")
				check(_has("menu	opened	true"), "GuiService.MenuOpened fired on the client, MenuIsOpen true")
				var moved := (body.global_position - menu_pos).length() if body else 99.0
				check(moved < 0.5, "W under the menu goes nowhere: moved %.2f" % moved)
				var boom: GPUParticles3D = world.get_node_or_null("Boom")
				check(boom != null and boom.one_shot and boom.position == Vector3(-50, 0, 1.5) and boom.process_material is ParticleProcessMaterial, "an Explosion in the Workspace is a one-shot burst at its Position")
				var names: Array[String] = []
				for c in menu.get_node("Panel/Box/Players").get_children(): names.append(c.text)
				check(names == ["Ada (you)"], "the menu lists the players: %s" % str(names))
				menu.get_node("Panel/Box/Buttons/Reset").emit_signal("pressed")
				check(menu.get_node("Confirm").visible and menu.get_node("Confirm").get_node_or_null("Box/Question") != null, "Reset Character asks first")
				menu.get_node("Confirm/Box/Buttons/Yes").emit_signal("pressed")
				check(not world.is_menu_open() and not menu.visible, "and the menu closes on Reset")
				old_body = body
				phase = 42
		42:
			if (_count("ouch") >= 2 and _has("menu\tclosed\tfalse")) or t > menu_at + 8.0:
				check(_count("ouch") >= 2, "the character died: Reset Character reached the server")
				check(_has("menu	closed	false"), "MenuClosed fired, MenuIsOpen false")
				reset_at = t
				phase = 43
		43:
			if (body != null and body != old_body) or t > reset_at + 10.0:
				check(body != null and body != old_body and is_instance_valid(body), "a new character after Reset Character")
				world.run_chunk("seat", """
					-- a Seat: Sit() pins the character on its top, a jump stands it up
					local bench = Instance.new("Seat"); bench.Name = "Bench"; bench.Size = Vector3.new(4, 1, 2); bench.Position = Vector3.new(10, 0.5, 10); bench.Anchored = true; bench.Parent = workspace
					local char = game.Players:GetPlayers()[1].Character
					local hum = char.Humanoid
					game.ReplicatedStorage.Pick.Adornee = char
					hum.Seated:Connect(function(active, part) print("seated", active, part and part.Name or "nil") end)
					bench:Sit(hum)
					task.delay(0.4, function()
						local d = char.HumanoidRootPart.Position - bench.Position
						print("sat", hum.Sit, hum.SeatPart.Name, bench.Occupant == hum, math.floor(d.X * 10 + 0.5) / 10, math.floor(d.Y * 10 + 0.5) / 10, math.floor(d.Z * 10 + 0.5) / 10, bench.SeatWeld.Part1.Name)
						hum.Jump = true
						task.delay(0.3, function() print("stood", hum.Sit, hum.SeatPart, bench.Occupant, char.HumanoidRootPart.Position.Y > 3) end)
					end)
				""")
				seat_at = t
				phase = 44
		44:
			if _has("stood\t") or t > seat_at + 4.0:
				check(_has("seated	true	Bench") and _has("sat	true	Bench	true	0	1.5	0	HumanoidRootPart"), "Seat:Sit(humanoid): Seated(true), Sit / SeatPart / Occupant; the engine pins the HumanoidRootPart on the seat's top")
				check(_has("seated	false	nil") and _has("stood	false	nil	nil	true"), "Jump = true stands it up: Seated(false), the seat free, the character hopping off")
				world.run_chunk("kerb", """
					-- Roblox's humanoid steps up onto anything under 2.5 studs without a jump, and is stopped by more
					local function block(name, size, pos) local p = Instance.new("Part"); p.Name = name; p.Size = size; p.Position = pos; p.Anchored = true; p.Parent = workspace; return p end
					block("KerbFloor", Vector3.new(40, 1, 40), Vector3.new(200, -0.5, 200))
					block("Kerb", Vector3.new(6, 2, 8), Vector3.new(200, 1, 208))       -- 2 studs up, from z 204 to 212
					block("Wall", Vector3.new(6, 5, 4), Vector3.new(200, 2.5, 214))     -- its top 3 above the kerb, at z 212: too much
					local char = game.Players:GetPlayers()[1].Character
					local hum, root = char.Humanoid, char.HumanoidRootPart
					root.CFrame = CFrame.new(200, 3.2, 196)
					task.wait(0.5)
					hum:MoveTo(Vector3.new(200, 0, 220))
					task.wait(3)
					print("kerb", root.Position.Y > 4.5, root.Position.Z > 205 and root.Position.Z < 212.5, root.Position, hum.WalkSpeed, hum.MoveDirection)
					-- a toolbox chair: the Seat behind a lip at its own height; you walk into it, step onto it, and it takes you
					local seat = Instance.new("Seat"); seat.Name = "Chair"; seat.Size = Vector3.new(3, 0.4, 1.4); seat.Position = Vector3.new(190, 1.6, 200); seat.Anchored = true; seat.Parent = workspace
					block("Lip", Vector3.new(3, 0.4, 0.8), Vector3.new(190, 1.6, 201.1))
					root.CFrame = CFrame.new(190, 3.2, 207)
					task.wait(0.5)
					hum:MoveTo(seat.Position)
					task.wait(3)
					print("chair", hum.SeatPart == seat, seat.Occupant == hum, root.Position)
				""")
				seat_at = t
				phase = 45
		45:
			if _has("chair\t") or t > seat_at + 12.0:
				check(_has("kerb	true	true"), "walking, the character steps up a 2-stud kerb and is stopped by a 3-stud wall, as Roblox's humanoid is")
				check(_has("chair	true	true"), "walked into, a toolbox chair (Seat behind a lip at its height) seats the character: it steps onto the Seat and Touched takes it")
				world.run_chunk("rig", """
					-- a rig that is not a character: an AnimationController's puppet plays a KeyframeSequence
					-- through its Motor6Ds' Transform, and a script may write Transform itself
					local rig = Instance.new("Model"); rig.Name = "Puppet"
					local root = Instance.new("Part"); root.Name = "HumanoidRootPart"; root.Size = Vector3.new(2, 2, 1); root.Position = Vector3.new(220, 5, 260); root.Anchored = true; root.Parent = rig
					local arm = Instance.new("Part"); arm.Name = "Right Arm"; arm.Size = Vector3.new(1, 2, 1); arm.Position = Vector3.new(221.5, 5, 260); arm.Parent = rig
					local m = Instance.new("Motor6D"); m.Name = "Right Shoulder"; m.Part0 = root; m.Part1 = arm; m.C0 = CFrame.new(1, 0.5, 0); m.C1 = CFrame.new(-0.5, 0.5, 0); m.Parent = root
					local ac = Instance.new("AnimationController"); ac.Parent = rig
					local animator = Instance.new("Animator"); animator.Parent = ac
					rig.Parent = workspace
					local seq = Instance.new("KeyframeSequence"); seq.Name = "Swing"
					for i, t in ipairs({0, 1}) do
						local kf = Instance.new("Keyframe"); kf.Time = t; kf.Parent = seq
						local rp = Instance.new("Pose"); rp.Name = "HumanoidRootPart"; rp.Parent = kf
						local ap = Instance.new("Pose"); ap.Name = "Right Arm"; ap.CFrame = CFrame.Angles(0, 0, math.rad(90 * (i - 1))); ap.Parent = rp
					end
					seq.Parent = game.ReplicatedStorage
					local anim = Instance.new("Animation"); anim.AnimationId = "ReplicatedStorage.Swing"
					local track = animator:LoadAnimation(anim); track.Looped = false; track:Play()
					task.wait(0.4)
					local o = arm.Orientation.Z   -- somewhere in the swing (a slow frame, and the mirror's frame of lag, move the reading)
					local p = arm.Position
					print("rigplay", o > 15 and o < 80, math.abs((p - Vector3.new(221, 5.5, 260)).Magnitude - 0.707) < 0.3, o, p)
					track:Stop(0)
					task.wait(0.3)
					print("rigrest", math.abs(arm.Orientation.Z) < 2, (arm.Position - Vector3.new(221.5, 5, 260)).Magnitude < 0.1)
					m.Transform = CFrame.Angles(0, 0, math.rad(90))
					task.wait(0.3)
					print("rigxf", math.abs(arm.Orientation.Z - 90) < 2, (arm.Position - Vector3.new(221.5, 6, 260)).Magnitude < 0.1)
					-- an EditableImage: drawn into from a buffer and with DrawRectangle, read back, shown by an ImageLabel's ImageContent
					local AssetService = game:GetService("AssetService")
					local ei = AssetService:CreateEditableImage({ Size = Vector2.new(4, 4) })
					local buf = buffer.create(4 * 4 * 4)
					for i = 0, 15 do buffer.writeu8(buf, i * 4, 255); buffer.writeu8(buf, i * 4 + 3, 255) end   -- red, opaque
					ei:WritePixelsBuffer(Vector2.zero, Vector2.new(4, 4), buf)
					ei:DrawRectangle(Vector2.new(2, 2), Vector2.new(2, 2), Color3.new(0, 0, 1), 0, Enum.ImageCombineType.Overwrite)
					local back = ei:ReadPixelsBuffer(Vector2.new(3, 3), Vector2.new(1, 1))
					local sg = Instance.new("ScreenGui"); sg.Name = "Canvas"
					local label = Instance.new("ImageLabel"); label.Name = "Pixels"; label.Size = UDim2.new(0, 64, 0, 64); label.ResampleMode = Enum.ResamplerMode.Pixelated
					label.ImageContent = Content.fromObject(ei)
					label.Parent = sg
					sg.Parent = game.Players:GetPlayers()[1].PlayerGui
					print("editable", ei.Size, buffer.readu8(back, 0), buffer.readu8(back, 2), typeof(label.ImageContent), label.ImageContent.Object == ei, tostring(label.ImageContent.SourceType))
					-- PluginSecurity: this chunk is the host's (the command bar) and may set CollisionFidelity; a game's
					-- Script may not, directly, from a coroutine it made, or from a task it spawned.
					local probe = Instance.new("MeshPart"); probe.Name = "SecProbe"; probe.Anchored = true; probe.Position = Vector3.new(300, 5, 300)
					local hostOk = pcall(function() probe.CollisionFidelity = Enum.CollisionFidelity.Box end)
					probe.Parent = workspace
					local sec = Instance.new("Script"); sec.Name = "NotAPlugin"
					sec.Source = "local p = workspace:WaitForChild('SecProbe') " ..
						"local function try() return (pcall(function() p.CollisionFidelity = Enum.CollisionFidelity.Hull end)) end " ..
						"local direct = try() local wrapped = true coroutine.wrap(function() wrapped = try() end)() " ..
						"local spawned = true task.spawn(function() spawned = try() end) " ..
						"print('pluginsec', HOSTOK, direct, wrapped, spawned, p.CollisionFidelity.Name)"
					sec.Source = string.gsub(sec.Source, "HOSTOK", tostring(hostOk))
					sec.Parent = game:GetService("ServerScriptService")
					-- the character's own Motor6Ds: Roblox's joints in the Torso, and a script's Transform swinging the arm about the shoulder
					local char = game.Players:GetPlayers()[1].Character
					local rs = char.Torso:FindFirstChild("Right Shoulder")
					print("motors", rs ~= nil and rs.Part0 == char.Torso and rs.Part1 == char["Right Arm"], char.HumanoidRootPart:FindFirstChild("RootJoint") ~= nil, char.Torso:FindFirstChild("Left Hip") ~= nil and char.Torso:FindFirstChild("Neck") ~= nil)
					if rs then rs.Transform = CFrame.Angles(0, 0, math.rad(90)) end
					task.wait(0.4)
					print("motorxf", rs ~= nil)
					task.delay(2, function() if rs then rs.Transform = CFrame.new() end end)
				""")
				seat_at = t
				phase = 46
		46:
			if _has("motorxf\t") or t > seat_at + 8.0:
				check(_has("rigplay	true	true	"), "an AnimationController rig plays a KeyframeSequence: halfway through, its arm is turned about its shoulder by the Pose")
				check(_has("rigrest	true	true"), "stopped, the limb the track let go of goes back to rest")
				check(_has("rigxf	true	true"), "a script's own Motor6D.Transform poses the limb the same way (Part1 = Part0 * C0 * Transform * C1^-1)")
				check(_has("motors	true	true	true"), "a character carries Roblox's Motor6Ds: Torso.Right Shoulder (Part0 Torso, Part1 Right Arm), HumanoidRootPart.RootJoint, Left Hip, Neck")
				check(_has("pluginsec	true	false	false	false	Box"), "PluginSecurity: the host may set CollisionFidelity, a game Script may not -- not directly, not from coroutine.wrap, not from task.spawn: %s" % [logs.filter(func(l): return l.contains("pluginsec"))])
				check(_has("editable	4, 4	0	255	Content	true	Enum.ContentSourceType.Object"), "an EditableImage takes a buffer of pixels and DrawRectangle, reads them back, and ImageContent = Content.fromObject(it) is a Content of it")
				var canvas_layer: CanvasLayer = world.get_node_or_null("Canvas")
				var pix_rect: TextureRect = null
				if canvas_layer:
					for tr in canvas_layer.find_children("*", "TextureRect", true, false):
						pix_rect = tr
				var pix_img: Image = pix_rect.texture.get_image() if pix_rect and pix_rect.texture else null
				check(pix_img != null and pix_img.get_width() == 4 and pix_img.get_pixel(0, 0).is_equal_approx(Color(1, 0, 0, 1)) and pix_img.get_pixel(3, 3).is_equal_approx(Color(0, 0, 1, 1)),
					"the ImageLabel shows the EditableImage's pixels, red with a blue corner: %s" % [pix_img.get_pixel(3, 3) if pix_img else "no image"])
				var arm_m: MeshInstance3D = null
				if body:
					for c in body.get_children():
						if c is MeshInstance3D and c.name == "Right Arm":
							arm_m = c
				var ao: Vector3 = arm_m.transform.origin if arm_m else Vector3.ZERO
				var ay: Vector3 = arm_m.transform.basis.y if arm_m else Vector3.UP
				# swung level, the arm's middle stands out by half its length less the half
				# stud the shoulder sits in from its top
				var out: float = (arm_m.get_aabb().size.y / 2.0 - 0.5) if arm_m else 0.5
				check(arm_m != null and absf(ay.y) < 0.15 and absf(ao.x - 1.5) < 0.1 and absf(ao.y - 0.5) < 0.1 and absf(absf(ao.z) - out) < 0.1,
					"Right Shoulder.Transform = Angles(0, 0, 90 deg) swings the character's own arm level about the shoulder (Part0 * C0 * Transform * C1^-1): %s %s" % [ao, ay])
				world.set_breakpoint("ServerScriptService.Debuggee", 3, true)
				world.script_paused.connect(func(sc, ln, fr): pauses.append({"script": sc, "line": ln, "frames": fr}))
				world.script_resumed.connect(func(): resumes += 1)
				world.run_chunk("dbg", """
					local s = Instance.new("Script")
					s.Name = "Debuggee"
					s.Source = [[local a = 1
local b = a + 1
print('bp', a, b)
local function twice(x)
	return x * 2
end
local c = twice(b)
print('after', c)]]
					s.Parent = game.ServerScriptService
				""")
				dbg_at = t
				phase = 47
		47:
			if dbg_step == 0 and pauses.size() >= 1:
				var p0: Dictionary = pauses[0]
				check(p0.script == "ServerScriptService.Debuggee" and p0.line == 3 and not _has("bp	"), "a breakpoint set before the script existed stops it at line 3, before that line runs: %s:%s" % [p0.script, p0.line])
				var f0: Dictionary = p0.frames[0] if p0.frames.size() > 0 else {}
				check(f0.get("function", "") == "(main)" and f0.get("locals", {}).get("a", "") == "1" and f0.get("locals", {}).get("b", "") == "2", "the top frame is the chunk with its locals so far: %s" % [f0])
				world.debug_step(2)
				dbg_step = 1
			elif dbg_step == 1 and pauses.size() >= 2:
				check(pauses[1].line == 4 and _has("bp	1	2"), "Step Over runs the print and stops on the chunk's next line: %s" % pauses[1].line)
				world.debug_step(1)
				dbg_step = 2
			elif dbg_step == 2 and pauses.size() >= 3:
				check(pauses[2].line == 7, "Step Into from the function statement lands on the call: %s" % pauses[2].line)
				world.debug_step(1)
				dbg_step = 3
			elif dbg_step == 3 and pauses.size() >= 4:
				var p3: Dictionary = pauses[3]
				var f3: Dictionary = p3.frames[0] if p3.frames.size() > 0 else {}
				check(p3.line == 5 and f3.get("function", "") == "twice" and f3.get("locals", {}).get("x", "") == "2" and p3.frames.size() >= 2 and int(p3.frames[1].get("line", 0)) == 7,
					"Step Into enters twice(): line 5, x = 2, the chunk below it at line 7: %s %s" % [p3.line, f3])
				world.debug_step(3)
				dbg_step = 4
			elif dbg_step == 4 and pauses.size() >= 5:
				var p4: Dictionary = pauses[4]
				check(p4.line >= 7 and p4.line <= 8 and p4.frames.size() > 0 and p4.frames[0].get("function", "") == "(main)" and not _has("after	"), "Step Out returns to the chunk, the rest still to run: %s" % p4.line)
				world.debug_continue()
				dbg_step = 5
			elif dbg_step == 5 and _has("after	4"):
				check(resumes >= 1, "Continue runs it to the end: 'after 4' printed, script_resumed fired %d" % resumes)
				world.run_chunk("water", """
					-- a pool of terrain water: a plastic cube floats, a metal one sinks, the character swims
					local T = workspace.Terrain
					T:FillBlock(CFrame.new(-200, -4, -200), Vector3.new(24, 8, 24), Enum.Material.Water)
					local floor = Instance.new("Part"); floor.Name = "PoolFloor"; floor.Size = Vector3.new(30, 1, 30); floor.Position = Vector3.new(-200, -8.5, -200); floor.Anchored = true; floor.Parent = workspace
					local cork = Instance.new("Part"); cork.Name = "Cork"; cork.Size = Vector3.new(2, 2, 2); cork.Position = Vector3.new(-204, 6, -200); cork.Parent = workspace
					local anvil = Instance.new("Part"); anvil.Name = "Anvil"; anvil.Size = Vector3.new(2, 2, 2); anvil.Material = Enum.Material.Metal; anvil.Position = Vector3.new(-196, 6, -200); anvil.Parent = workspace
					local char = game.Players:GetPlayers()[1].Character
					local root = char.HumanoidRootPart
					char.Humanoid.Jump = true      -- off the chair the earlier check left it in
					task.wait(0.6)
					root.CFrame = CFrame.new(-200, 6, -206)
					task.wait(3)
					print("water", cork.Position.Y > -3, anvil.Position.Y < -6, root.Position.Y > -4 and root.Position.Y < 4.5, char.Humanoid:GetState().Name, cork.Position.Y, anvil.Position.Y, root.Position)
				""")
				dbg_at = t
				phase = 48
		48:
			if _has("water\t") or t > dbg_at + 8.0:
				check(_has("water	true	true	true	Swimming"), "terrain water: the plastic cube floats, the metal one sinks to the pool floor, the character swims near the top in state Swimming")
				phase = 9
			elif t > dbg_at + 20.0:
				check(false, "debugger: timed out at step %d with %d stops: %s" % [dbg_step, pauses.size(), pauses.map(func(p): return p.line)])
				phase = 9
		9:
			if (_has("stored\t") and _has("flung\t") and _has("moved\t")
					and _has("pendulum\t") and _has("align\t")) or t > moveto_at + 8.0:
				check(_has("stored	10"), "DataStoreService stores from a server Script")
				var envr: Environment = world.get_node("Lighting").environment
				# with no BloomEffect the glow is Neon's own, past white, and only at quality 8 and up
				check(envr.glow_enabled == (world.get_quality_level() >= 8) and is_equal_approx(envr.glow_hdr_threshold, 1.0) and is_equal_approx(envr.glow_intensity, 0.8) and not world.get_node("Blur").visible,
					"destroying the BloomEffect leaves only Neon's own glow; a Blur Size of 0 turns it off (quality %d): %s %s %s %s" % [world.get_quality_level(), envr.glow_enabled, envr.glow_hdr_threshold, envr.glow_intensity, world.get_node("Blur").visible])
				check(envr.fog_enabled and envr.fog_mode == Environment.FOG_MODE_EXPONENTIAL and is_equal_approx(envr.fog_density, 0.06 * pow(0.5, 5)) and envr.fog_sky_affect == 0.5 and envr.fog_light_color.b == 1.0, "an Atmosphere replaces FogEnd's fog: exponential, Density, Haze, Color")
				# the sky shader takes SunAngularSize as sun_cos, the cosine of half the angle across
				var sky_mat = envr.sky.sky_material
				check(sky_mat is ShaderMaterial and is_equal_approx(float(sky_mat.get_shader_parameter("sun_cos")), cos(deg_to_rad(30.0))), "a Sky's SunAngularSize is the sun's disk: 60 degrees across")
				var torch9: Node3D = world.get_node_or_null("Torch")
				var head9: Node = body.get_node_or_null("Head") if body else null
				var sign9: Node3D = world.get_node_or_null("Billboard")
				var glow_fill: MeshInstance3D = null
				var glow_line: MeshInstance3D = null
				if torch9: for c in torch9.get_children(): if c is MeshInstance3D: glow_fill = c.get_node_or_null("Highlight Fill"); glow_line = c.get_node_or_null("Highlight Outline")
				check(glow_fill != null and glow_fill.mesh == glow_fill.get_parent().mesh and glow_fill.material_override.albedo_color == Color(0, 0, 1, 0.75) and glow_fill.material_override.no_depth_test, "a Highlight in a part tints a copy of its mesh FillColor at FillTransparency, over everything")
				check(glow_line != null and glow_line.material_override.grow and glow_line.material_override.cull_mode == BaseMaterial3D.CULL_FRONT and glow_line.material_override.albedo_color == Color(1, 1, 0, 1), "its outline is the mesh grown and turned inside out, OutlineColor")
				var arm: Node = body.get_node_or_null("Left Arm") if body else null
				var pick_fill: MeshInstance3D = head9.get_node_or_null("Highlight Fill") if head9 else null
				check(pick_fill != null and arm != null and arm.get_node_or_null("Highlight Outline") != null and not pick_fill.material_override.no_depth_test, "a Highlight whose Adornee is the character Model covers every limb; Occluded hides behind walls")
				var sign_hl := false
				if sign9: for c in sign9.get_children(): if c is MeshInstance3D and c.get_node_or_null("Highlight Fill"): sign_hl = true
				check(not sign_hl, "Enabled = false takes it off")
				check(_has("boomhit	1") and _has("flung	true	Workspace"), "Hit(part, distance) fired for the crate beside it, which BlastPressure flung; it is still there at 0.6s")
				var wframe: Node3D = world.get_node_or_null("WFrame")
				check(wframe is StaticBody3D and wframe.get_node_or_null("WPanel") is MeshInstance3D and wframe.get_node_or_null("WPanel Collision") is CollisionShape3D and world.get_node_or_null("WPanel") == null, "a WeldConstraint's Part1 is a mesh and a collision shape under its anchored Part0's body")
				check(_has("welded	34, 10, 0	true	4, 0, 0	true"), "the welded part stays put; a Weld'd unanchored pair falls as one body, C0 apart")
				check(_has("moved	30, 12, -4	30, 12, 0"), "moving the root's CFrame carries the welded part around it")
				check(_has("door	true	true	true"), "a HingeConstraint's Motor swings the door about Attachment0's axis, CurrentAngle within the limits; a Servo turns the flap to TargetAngle")
				check(_has("pendulum	3	true	true"), "a BallSocketConstraint hangs the bob from the hook: it swings, the attachments together")
				check(_has("rope	5	5	true"), "a RopeConstraint catches the falling load at Length, CurrentDistance reported")
				check(_has("rod	true	true	5"), "a RodConstraint keeps the tip Length from the beam as it swings down")
				check(_has("spring	4.7	4.7"), "a SpringConstraint stretches under the weight (0.7 mass, Stiffness 200): 4.69, Damping settling it")
				check(_has("hover	5.6	true	true"), "a VectorForce of GetMass() * Gravity holds a 4x1x2 part where it is; an AngularVelocity turns it")
				check(_has("belt	true	true"), "a LinearVelocity carries the crate at 4 studs/s, level: MaxForce holds it against gravity")
				var trail_node: MeshInstance3D = null
				for kid in world.get_children():
					if kid is MeshInstance3D and str(kid.name).begins_with("Trail"):
						trail_node = kid
				var trail_verts := 0
				var trail_uvs := 0
				var trail_tail := 1.0
				var trail_head := 0.0
				if trail_node != null and trail_node.mesh != null and trail_node.mesh.get_surface_count() > 0:
					var tarr: Array = trail_node.mesh.surface_get_arrays(0)
					var tv: PackedVector3Array = tarr[Mesh.ARRAY_VERTEX]
					trail_verts = tv.size()
					if tarr[Mesh.ARRAY_TEX_UV] != null:
						trail_uvs = tarr[Mesh.ARRAY_TEX_UV].size()
					if trail_verts >= 4:
						trail_tail = tv[0].distance_to(tv[1])
						trail_head = tv[trail_verts - 2].distance_to(tv[trail_verts - 1])
				check(trail_verts >= 8, "a Trail between two attachments on the moving crate is a ribbon behind it: %d vertices" % trail_verts)
				var trail_tex: bool = trail_node != null and trail_node.material_override is StandardMaterial3D and trail_node.material_override.albedo_texture != null
				check(trail_tex and trail_uvs == trail_verts and trail_tail < trail_head * 0.5,
					"its Texture is on the ribbon with a u along it, and WidthScale (1 -> 0) tapers it toward the tail: %.2f wide at the tail, %.2f at the attachments" % [trail_tail, trail_head])
				var arc_node: MeshInstance3D = null
				for kid in world.get_children():
					if kid is MeshInstance3D and str(kid.name) == "Arc":
						arc_node = kid
				var arc_verts := 0
				var arc_box := AABB()
				var arc_uspan := 0.0
				if arc_node != null and arc_node.mesh != null and arc_node.mesh.get_surface_count() > 0:
					var aarr: Array = arc_node.mesh.surface_get_arrays(0)
					arc_verts = aarr[Mesh.ARRAY_VERTEX].size()
					arc_box = arc_node.mesh.get_aabb()
					if aarr[Mesh.ARRAY_TEX_UV] != null:
						var lo := 1e9
						var hi := -1e9
						for uv in aarr[Mesh.ARRAY_TEX_UV]:
							lo = minf(lo, uv.x)
							hi = maxf(hi, uv.x)
						arc_uspan = hi - lo
				check(arc_verts == 18 and arc_box.position.x < -39.0 and arc_box.end.x > -31.0 and arc_box.end.y > 11.5,
					"a Beam is a ribbon of Segments quads from Attachment0 to Attachment1, bowed up by CurveSize0 along Attachment0's X: %d vertices, box %s" % [arc_verts, arc_box])
				var arc_tex: bool = arc_node != null and arc_node.material_override is StandardMaterial3D and arc_node.material_override.albedo_texture != null
				check(arc_tex and arc_uspan > 4.0 and arc_uspan < 9.0,
					"its Texture wraps every TextureLength (2) studs of the curve: u runs %.1f over a curve some 10 to 14 studs long" % arc_uspan)
				check(_has("door	true	true	true"), "a Rotate joint is a free hinge: pushed, the door swings on its jamb, its hinge edge staying put")
				check(_has("rotatev	true	true	true"), "a RotateV spins its blade at the Motor face's ParamB (SurfaceInput Constant); a RotateP serves its blade to DesiredAngle; both stay on their hubs")
				check(_has("impulse	true	true	"), "ApplyImpulse throws a part up at impulse / mass; ApplyAngularImpulse spins one")
				check(_has("early	kept") and _has("early	too"), "a RemoteEvent fired before the client connected is kept and delivered, in order, once it does")
				check(_has("assembly	5.6	6.3	true	1	true"), "Mass, AssemblyMass over a WeldConstraint, AssemblyRootPart (the anchored one), GetConnectedParts, GetRootPart")
				check(_has("nocollide	true	true"), "a NoCollisionConstraint's part falls through the shelf it names; the one beside it lands on its shelf")
				check(_has("bodymovers	true	true	true	true	true	true"), "the legacy BodyMovers act on their part: BodyPosition + BodyGyro hold it at a point, upright; BodyVelocity carries a crate level; BodyForce hovers a part BodyAngularVelocity spins")
				check(_has("align	true	true"), "a rigid AlignPosition / AlignOrientation puts the part at Position, turned to CFrame")
				check(_has("drive	true	true	true	"), "Throttle = 1 drives the VehicleSeat along its look vector up to MaxSpeed, the welded hood along")
				check(_has("steer	true"), "Steer = 1 turns it at TurnSpeed")
				check(_has("welded touch	true	true	true"), "Touched names the welded part that was hit, on both sides, not the assembly's root")
				check(_has("prismatic	true	true	true"), "a PrismaticConstraint's Servo slides the car to TargetPosition and holds it on the axis")
				check(_has("cylindrical	true	true"), "a CylindricalConstraint's angular Servo turns the barrel to TargetAngle")
				check(_has("slide	true	true	true"), "Ice slides where Plastic stops (a material's friction), and a part weighs its material's density")
				var hinge: Node = world.get_node_or_null("DoorHinge")
				check(hinge is HingeJoint3D and hinge.get_flag(HingeJoint3D.FLAG_ENABLE_MOTOR) and hinge.get_flag(HingeJoint3D.FLAG_USE_LIMIT) and world.get_node_or_null("BallSocketConstraint") is PinJoint3D, "a HingeJoint3D with its motor and limits on, a PinJoint3D, under the world")
				var sign: Node3D = world.get_node_or_null("Billboard")
				var logo: MeshInstance3D = null
				var tile: MeshInstance3D = null
				if sign: for c in sign.get_children(): if c is MeshInstance3D: logo = c.get_node_or_null("Logo"); tile = c.get_node_or_null("Tile")
				check(logo != null and logo.visible and logo.mesh.size == Vector2(6, 3) and is_equal_approx(logo.position.z, -0.51) and logo.material_override.albedo_texture != null and logo.material_override.albedo_color.a == 0.75, "a Decal is a quad on the part's Front face, sized to it, its Transparency the alpha")
				check(tile != null and is_equal_approx(tile.position.y, 1.51) and tile.mesh.size == Vector2(6, 1) and tile.material_override.uv1_scale == Vector3(2, 2, 1), "a Texture on the Top face repeats every StudsPerTile studs")
				var wa: Node3D = world.get_node_or_null("WA")
				check(wa is RigidBody3D and wa.get_node_or_null("WB") is MeshInstance3D and world.get_node_or_null("WB") == null, "the Weld's Part1 rides Part0's RigidBody3D")
				world.queue_free()
				var second := PulseBlockzWorld.new()
				# Main.gd's budgets: the per-call 4 ms is what kills the demo's runaway Evil script, every frame
				second.max_millis_per_call = 4.0
				second.max_frame_millis = 8.0
				second.auto_join = false
				second.data_store_path = world.data_store_path
				root.add_child(second)
				second.run_chunk("ds2", """
					local ds = game:GetService("DataStoreService"):GetDataStore("PlayerData")
					print("read", ds:GetAsync("7").coins, ds:GetAsync("visits"))
				""")
				world = second
				world.script_print.connect(func(n, t2): logs.append(n + ": " + t2))
				world.script_error.connect(func(n, e): logs.append("ERROR " + n + ": " + e))
				second_at = t
				phase = 10
		10:
			if _has("read\t") or t > second_at + 5.0:
				check(_has("read	10	1"), "and a fresh world reads them back from the file")
				DirAccess.remove_absolute(ProjectSettings.globalize_path(world.data_store_path))
				return _finish()
	return false

var moveto_at := 0.0
var menu_at := 0.0
var reset_at := 0.0
var second_at := 0.0
var seat_at := 0.0
var pauses: Array = []      # the debugger's stops: {script, line, frames}
var resumes := 0
var dbg_step := 0
var dbg_at := 0.0
var menu_pos := Vector3()
var moveto_best := 1e9
var vp_size := Vector2()
var noon_energy := 0.0

var old_body: Node3D
var died_at := 0.0
var board_at := Vector3()
var board_eye := Vector3()
var board_moved_at := 0.0
var clicked_board_at := 0.0
var tool_at := 0.0
func _count(s: String) -> int:
	var n := 0
	for l in logs: if l.find(s) != -1: n += 1
	return n

# Limbs, what the character wears and what it holds. Dirty limb offsets are recomputed in
# the pass that drains the prints, so counting here also waits for the grip weld.
var mesh_names: Array[String] = []   # what the last count counted, for a failure to print
func _meshes(n: Node3D) -> int:
	var c := 0
	mesh_names = []
	if n:
		for m in n.get_children():
			# not the spawn ForceField's bubble, drawn on the root for its ten seconds
			if m is MeshInstance3D and not str(m.name).contains("ForceField"):
				c += 1
				mesh_names.append(str(m.name))
	return c

# The Sign's BillboardGui label. "Clicks: 0" is what the model file ships.
func _sign_clicks() -> String:
	var tag := world.find_child("Tag", true, false) as Control
	if tag:
		for c in tag.get_children():
			for cc in c.get_children():
				if cc is Label: return cc.text
				for ccc in cc.get_children():
					if ccc is Label: return ccc.text
	return ""

func _part_at(name: String, at: Vector3, eps := 0.05) -> bool:
	var n := world.get_node_or_null(name) as Node3D
	return n != null and n.global_position.distance_to(at) < eps

# The R15 NPC's right arm mid-swing. The walk cycle is a sine through zero about 1.4
# times a second, so a fixed sample catches it flat about one run in fourteen.
func _r15_arm_swung() -> bool:
	for c in world.get_children():
		if c is CharacterBody3D:
			for m in c.get_children():
				if m is MeshInstance3D and m.name == "RightUpperArm":
					if _turn(m.transform.basis) > 0.1: return true
	return false

# how far a limb is turned from rest, whatever the axis
func _turn(b: Basis) -> float:
	var a: float = b.get_rotation_quaternion().get_angle()
	return minf(a, TAU - a)

func _has(s: String) -> bool:
	for l in logs: if l.find(s) != -1: return true
	return false

func _finish() -> bool:
	for l in logs:
		if l.begins_with("ERROR"): check(false, l)
	var has := func(s: String) -> bool:
		for l in logs: if l.find(s) != -1: return true
		return false
	check(has.call("client up"), "LocalScript ran on the client")
	check(has.call("Workspace.Camera"), "the client has workspace.CurrentCamera")
	check(has.call("my character is here"), "LocalPlayer.Character reached the client")
	check(has.call("dropped a brick"), "RemoteEvent client -> server")
	check(has.call("key	W	false") and has.call("use	DropBrick	E") and has.call("key	E	true"), "keys reach ContextActionService, then UserInputService, on the client")
	check(has.call("jump requested"), "Space fires UserInputService.JumpRequest")
	check(has.call("hud	Activated	MouseButton1	1"), "a click on the TextButton fires Activated on the client")
	check(has.call("mouse target	Sign"), "Player:GetMouse().Target through the engine's camera and viewport")
	check(has.call("sign clicked by	Ada"), "a ClickDetector's MouseClick reaches the server Script with the Player")
	check(has.call("wand away"), "the held tool's key puts it back: Unequipped")
	var patrol := 0
	for l in logs: if l.find("patrol	Success	") != -1: patrol = maxi(patrol, l.substr(l.find("patrol	Success	") + 15).to_int())
	check(patrol >= 12, "the Patrol's Path computes on the server, round the lava: %d waypoints" % patrol)
	check(has.call("post	1	false	true"), "and the guard walked it, waypoint by waypoint, round the lava")
	# 36 pixels of topbar inset off the top: the hud does not set IgnoreGuiInset
	check(has.call("hudsize	%d	%d	160	40" % [vp_size.x, vp_size.y - 36]), "AbsoluteSize reaches the scripts: the ScreenGui is the viewport (%s) under the topbar inset, the button its pixels" % vp_size)
	print("play solo: %s" % ("PASS" if failed == 0 else "%d FAILED" % failed))
	quit(1 if failed else 0)
	return true
