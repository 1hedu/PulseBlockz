# The Animation Editor: pose a rig's Motor6D joints over a timeline and save the result as a
# KeyframeSequence under the rig's AnimSaves folder, where Roblox's editor keeps its own -- an
# Animation whose AnimationId is "Workspace.Rig.AnimSaves.Wave" plays what this writes.
class_name AnimationEditor
extends PanelContainer

signal closed

# Posing is not an edit of the place: what the Handles write goes to the world, with no undo step.
class PoseHistory extends History:
	func set_property(id: int, name: String, value) -> bool:
		return world.set_property(id, name, value)
	func begin(_label: String) -> void: pass
	func end() -> void: pass

const FPS := 30.0
const ROW := 20
const LABELS := 150.0
const RULER := 22

var studio: Node = null
var world: PulseBlockzWorld = null
var rig: int = 0
var rig_name := ""
var root_part: int = 0
var joints: Array = []               # tree order from the root; {name, id, part0, part1, c0, c1}
var _by_part: Dictionary = {}        # Part1 id -> index into joints
var _rest: Dictionary = {}           # part id -> {position, orientation} as the rig stood
var keys: Dictionary = {}            # joint name -> Array of {t, tf} sorted by t
var time := 0.0
var length := 1.0
var playing := false
var looping := true
var _selected_joint := -1
var _selected_key := {}              # {joint: name, t: float} or empty
var _frames: Dictionary = {}         # part id -> Transform3D, as last posed
var _pose_history: PoseHistory
var _real_history: History
var _dragging_time := false
var _dragging_key := false

var _title: Label
var _name: LineEdit
var _list: ItemList
var _timeline: Control
var _play: Button
var _loop: CheckBox
var _length: SpinBox
var _saves: OptionButton
var _time_label: Label

func _init() -> void:
	name = "AnimationEditor"
	custom_minimum_size = Vector2(0, 230)
	var box := VBoxContainer.new()
	add_child(box)
	var bar := HBoxContainer.new()
	bar.add_theme_constant_override("separation", 6)
	box.add_child(bar)
	_title = Label.new()
	_title.text = "Animation Editor"
	bar.add_child(_title)
	_name = LineEdit.new()
	_name.placeholder_text = "Animation name"
	_name.text = "Animation"
	_name.custom_minimum_size = Vector2(140, 0)
	bar.add_child(_name)
	_button(bar, "Save", func(): save(_name.text))
	_saves = OptionButton.new()
	_saves.custom_minimum_size = Vector2(120, 0)
	_saves.item_selected.connect(_on_save_picked)
	bar.add_child(_saves)
	bar.add_child(VSeparator.new())
	_play = _button(bar, "Play", func(): set_playing(not playing))
	_button(bar, "|<", func(): set_time(0.0))
	_button(bar, "Add Key", func(): add_key_all())
	_button(bar, "Delete Key", func(): delete_selected_key())
	_loop = CheckBox.new()
	_loop.text = "Loop"
	_loop.button_pressed = true
	_loop.toggled.connect(func(on): looping = on)
	bar.add_child(_loop)
	var len_label := Label.new()
	len_label.text = "Length"
	bar.add_child(len_label)
	_length = SpinBox.new()
	_length.min_value = 1.0 / FPS
	_length.max_value = 60
	_length.step = 1.0 / FPS
	_length.value = 1.0
	_length.value_changed.connect(_on_length_changed)
	bar.add_child(_length)
	_time_label = Label.new()
	_time_label.text = "0.000 s"
	_time_label.custom_minimum_size = Vector2(70, 0)
	bar.add_child(_time_label)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.add_child(spacer)
	_button(bar, "Close", func(): close())
	var split := HBoxContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(split)
	_list = ItemList.new()
	_list.custom_minimum_size = Vector2(LABELS, 0)
	_list.item_selected.connect(_on_joint_picked)
	split.add_child(_list)
	_timeline = Control.new()
	_timeline.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_timeline.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_timeline.clip_contents = true
	_timeline.draw.connect(_draw_timeline)
	_timeline.gui_input.connect(_timeline_input)
	_timeline.focus_mode = Control.FOCUS_CLICK
	split.add_child(_timeline)
	visible = false

func _on_save_picked(i: int) -> void:
	if i > 0:
		load_named(_saves.get_item_text(i))

func _on_length_changed(v: float) -> void:
	length = v
	_timeline.queue_redraw()

func _button(into: Control, text: String, action: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(action)
	into.add_child(b)
	return b

# ---- the rig -----------------------------------------------------------------------

static func rig_of(w: PulseBlockzWorld, id: int) -> int:
	var at := id
	for hop in 32:
		if at == 0:
			return 0
		var info := w.get_instance(at)
		if info.is_empty():
			return 0
		if info.get("class_name", "") == "Model" and not _motors_under(w, at).is_empty():
			return at
		at = int(info.get("parent", 0))
	return 0

static func _motors_under(w: PulseBlockzWorld, id: int, out: Array = []) -> Array:
	for c in w.get_child_ids(id):
		var cls: String = w.get_instance(c).get("class_name", "")
		if cls == "Motor6D" or cls == "Motor":
			out.append(c)
		_motors_under(w, c, out)
	return out

static func _parts_under(w: PulseBlockzWorld, id: int, out: Array = []) -> Array:
	for c in w.get_child_ids(id):
		if w.get_part_node(c) != null:
			out.append(c)
		_parts_under(w, c, out)
	return out

func _props(id: int) -> Dictionary:
	var out := {}
	for p in world.get_properties(id, true):
		out[p.name] = p.value
	return out

static func _xf(position: Vector3, orientation: Vector3) -> Transform3D:
	return Transform3D(Handles._euler_basis(orientation), position)

func _frame(id: int) -> Transform3D:
	var p := _props(id)
	return _xf(p.get("Position", Vector3.ZERO), p.get("Orientation", Vector3.ZERO))

func open_for(rig_id: int) -> bool:
	if studio == null or world == null or rig_id == 0:
		return false
	if visible:
		close()
	var motors := _motors_under(world, rig_id)
	if motors.is_empty():
		return false
	rig = rig_id
	rig_name = str(world.get_instance(rig).get("name", "Rig"))
	# the root is the Part0 that is nobody's Part1
	var by_part1 := {}
	var part1s := {}
	for m in motors:
		var p := _props(m)
		var p0 := int(p.get("Part0", 0))
		var p1 := int(p.get("Part1", 0))
		if p0 == 0 or p1 == 0 or world.get_part_node(p0) == null or world.get_part_node(p1) == null:
			continue
		by_part1[p1] = {"name": str(world.get_instance(p1).get("name", "")), "id": m, "part0": p0, "part1": p1,
			"c0": _xf(p.get("C0Position", Vector3.ZERO), p.get("C0Orientation", Vector3.ZERO)),
			"c1": _xf(p.get("C1Position", Vector3.ZERO), p.get("C1Orientation", Vector3.ZERO))}
		part1s[p1] = true
	root_part = 0
	for p1 in by_part1:
		var p0: int = by_part1[p1].part0
		if not part1s.has(p0):
			root_part = p0
			break
	if root_part == 0:
		return false
	# tree order from the root: apply_pose needs a parent limb's frame before the limbs on it
	joints.clear()
	_by_part.clear()
	var queue := [root_part]
	while not queue.is_empty():
		var cur: int = queue.pop_front()
		for p1 in by_part1:
			if by_part1[p1].part0 == cur and not _by_part.has(p1):
				_by_part[p1] = joints.size()
				joints.append(by_part1[p1])
				queue.append(p1)
	_rest.clear()
	for pid in _parts_under(world, rig):
		var p := _props(pid)
		_rest[pid] = {"position": p.get("Position", Vector3.ZERO), "orientation": p.get("Orientation", Vector3.ZERO)}
	keys.clear()
	for j in joints:
		keys[j.name] = []
	time = 0.0
	length = 1.0
	_length.value = 1.0
	playing = false
	_play.text = "Play"
	_selected_joint = -1
	_selected_key = {}
	_title.text = "Animation Editor -- %s" % rig_name
	_list.clear()
	for j in joints:
		_list.add_item(j.name)
	_fill_saves()
	if studio.get("_handles") != null:
		_real_history = studio._handles.history
		_pose_history = PoseHistory.new()
		_pose_history.world = world
		studio._handles.history = _pose_history
		if not studio._handles.changed.is_connected(_on_handles_changed):
			studio._handles.changed.connect(_on_handles_changed)
	visible = true
	apply_pose()
	_timeline.queue_redraw()
	return true

func close() -> void:
	if not visible and rig == 0:
		return
	playing = false
	if world != null:
		for pid in _rest:
			world.set_property(pid, "Position", _rest[pid].position)
			world.set_property(pid, "Orientation", _rest[pid].orientation)
	if studio != null and studio.get("_handles") != null:
		if _real_history != null:
			studio._handles.history = _real_history
		if studio._handles.changed.is_connected(_on_handles_changed):
			studio._handles.changed.disconnect(_on_handles_changed)
	_real_history = null
	rig = 0
	joints.clear()
	_by_part.clear()
	_rest.clear()
	keys.clear()
	_frames.clear()
	visible = false
	closed.emit()

# ---- keys and poses ------------------------------------------------------------------

func _snap(t: float) -> float:
	return clampf(roundf(t * FPS) / FPS, 0.0, length)

func key_times(joint_name: String) -> Array:
	var out := []
	for k in keys.get(joint_name, []):
		out.append(k.t)
	return out

func _key_at(joint_name: String, t: float) -> int:
	var arr: Array = keys.get(joint_name, [])
	for i in arr.size():
		if absf(arr[i].t - t) < 0.5 / FPS:
			return i
	return -1

func set_key(joint_name: String, t: float, tf: Transform3D) -> void:
	if not keys.has(joint_name):
		return
	t = _snap(t)
	var arr: Array = keys[joint_name]
	var i := _key_at(joint_name, t)
	if i >= 0:
		arr[i].tf = tf
	else:
		arr.append({"t": t, "tf": tf})
		arr.sort_custom(func(a, b): return a.t < b.t)
	_timeline.queue_redraw()

func delete_key(joint_name: String, t: float) -> void:
	var i := _key_at(joint_name, t)
	if i >= 0:
		keys[joint_name].remove_at(i)
		apply_pose()
		_timeline.queue_redraw()

func delete_selected_key() -> void:
	if not _selected_key.is_empty():
		delete_key(_selected_key.joint, _selected_key.t)
		_selected_key = {}

# No keys means the identity transform, the rest pose; outside the keys the pose holds flat.
func pose_of(joint_name: String, t: float) -> Transform3D:
	var arr: Array = keys.get(joint_name, [])
	if arr.is_empty():
		return Transform3D()
	if t <= arr[0].t:
		return arr[0].tf
	for i in range(1, arr.size()):
		if t <= arr[i].t:
			var a: Dictionary = arr[i - 1]
			var b: Dictionary = arr[i]
			var w: float = 0.0 if float(b.t) <= float(a.t) else (t - float(a.t)) / (float(b.t) - float(a.t))
			return (a.tf as Transform3D).interpolate_with(b.tf, w)
	return arr.back().tf

func add_key_all() -> void:
	for j in joints:
		set_key(j.name, time, pose_of(j.name, time))

# orientation is a Roblox Orientation, in degrees; the pose's shift is kept
func set_joint_rotation(joint_name: String, orientation: Vector3) -> void:
	var was := pose_of(joint_name, time)
	set_key(joint_name, time, Transform3D(Handles._euler_basis(orientation), was.origin))
	apply_pose()

func set_joint_offset(joint_name: String, offset: Vector3) -> void:
	var was := pose_of(joint_name, time)
	set_key(joint_name, time, Transform3D(was.basis, offset))
	apply_pose()

# Part1 = Part0 * C0 * Transform * C1:inverse(), Roblox's Motor6D, root limb down
func apply_pose() -> void:
	if rig == 0 or world == null:
		return
	_frames.clear()
	var r: Dictionary = _rest.get(root_part, {})
	_frames[root_part] = _xf(r.get("position", Vector3.ZERO), r.get("orientation", Vector3.ZERO))
	for j in joints:
		var p0: Transform3D = _frames.get(j.part0, _frame(j.part0))
		var p1: Transform3D = p0 * j.c0 * pose_of(j.name, time) * (j.c1 as Transform3D).affine_inverse()
		_frames[j.part1] = p1
		world.set_property(j.part1, "Position", p1.origin)
		world.set_property(j.part1, "Orientation", Handles._basis_euler(p1.basis))
	_time_label.text = "%.3f s" % time

func set_time(t: float) -> void:
	time = clampf(t, 0.0, length)
	apply_pose()
	_timeline.queue_redraw()

func set_playing(on: bool) -> void:
	playing = on and rig != 0
	_play.text = "Pause" if playing else "Play"

func _process(delta: float) -> void:
	if not visible or not playing:
		return
	var t := time + delta
	if t > length:
		if looping:
			t = fmod(t, maxf(length, 1.0 / FPS))
		else:
			t = length
			set_playing(false)
	set_time(t)

# The handles turn a limb about its own middle: keeping the new basis with the pose's old
# origin puts the turn about the joint instead. A handle drag keys the joint at the current
# time without Add Key, matching Roblox's animation editor.
func _on_handles_changed() -> void:
	if rig == 0 or studio == null:
		return
	var handles = studio._handles
	if handles == null or handles.ids.is_empty():
		return
	var part1: int = handles.ids[0]
	if not _by_part.has(part1):
		return
	var j: Dictionary = joints[_by_part[part1]]
	var p0: Transform3D = _frames.get(j.part0, _frame(j.part0))
	var now := _frame(part1)
	var t: Transform3D = (j.c0 as Transform3D).affine_inverse() * p0.affine_inverse() * now * j.c1
	var was := pose_of(j.name, time)
	if handles.mode == Handles.Mode.ROTATE:
		t = Transform3D(t.basis.orthonormalized(), was.origin)
	elif handles.mode == Handles.Mode.MOVE:
		t = Transform3D(was.basis, t.origin)
	else:
		return
	set_key(j.name, time, t)
	_selected_joint = _by_part[part1]
	_list.select(_selected_joint)
	apply_pose()

func _on_joint_picked(index: int) -> void:
	_selected_joint = index
	if studio != null and index >= 0 and index < joints.size():
		studio._select_one(joints[index].part1)
	_timeline.queue_redraw()

# ---- save and load: a KeyframeSequence under the rig's AnimSaves ------------------------

func _child_named(parent: int, child_name: String, cls := "") -> int:
	for c in world.get_child_ids(parent):
		var info := world.get_instance(c)
		if info.get("name", "") == child_name and (cls == "" or info.get("class_name", "") == cls):
			return c
	return 0

func _fill_saves() -> void:
	_saves.clear()
	_saves.add_item("Load...")
	var folder := _child_named(rig, "AnimSaves")
	if folder == 0:
		return
	for c in world.get_child_ids(folder):
		var info := world.get_instance(c)
		if info.get("class_name", "") == "KeyframeSequence":
			_saves.add_item(str(info.get("name", "")))

# Roblox's layout: Keyframe (Time) -> a Pose named for the root part -> a Pose per limb,
# nested as the joints are, each holding that joint's Transform.
func save(anim_name: String) -> int:
	if rig == 0 or studio == null or anim_name.strip_edges().is_empty():
		return 0
	anim_name = anim_name.strip_edges()
	var history: History = studio.history
	history.begin("save animation %s" % anim_name)
	var folder := _child_named(rig, "AnimSaves")
	if folder == 0:
		folder = await history.insert("Folder", rig)
		history.set_property(folder, "Name", "AnimSaves")
	var old := _child_named(folder, anim_name, "KeyframeSequence")
	if old != 0:
		history.destroy(old)
	var seq: int = await history.insert("KeyframeSequence", folder)
	history.set_property(seq, "Name", anim_name)
	history.set_property(seq, "Loop", looping)
	var times := {}
	for jn in keys:
		for k in keys[jn]:
			times[k.t] = true
	if times.is_empty():
		times[0.0] = true
	var sorted := times.keys()
	sorted.sort()
	for t in sorted:
		var kf: int = await history.insert("Keyframe", seq)
		history.set_property(kf, "Name", "Keyframe")
		history.set_property(kf, "Time", t)
		var pose_of_part := {}
		var root_pose: int = await history.insert("Pose", kf)
		history.set_property(root_pose, "Name", str(world.get_instance(root_part).get("name", "HumanoidRootPart")))
		pose_of_part[root_part] = root_pose
		for j in joints:
			var under: int = pose_of_part.get(j.part0, root_pose)
			var pose: int = await history.insert("Pose", under)
			var tf := pose_of(j.name, t)
			history.set_property(pose, "Name", j.name)
			history.set_property(pose, "CFramePosition", tf.origin)
			history.set_property(pose, "CFrameOrientation", Handles._basis_euler(tf.basis))
			pose_of_part[j.part1] = pose
	history.end()
	_name.text = anim_name
	_fill_saves()
	return seq

func load_named(anim_name: String) -> bool:
	var folder := _child_named(rig, "AnimSaves")
	var seq := _child_named(folder, anim_name, "KeyframeSequence") if folder != 0 else 0
	if seq == 0:
		return false
	for jn in keys:
		keys[jn] = []
	var longest := 0.0
	for kf in world.get_child_ids(seq):
		if world.get_instance(kf).get("class_name", "") != "Keyframe":
			continue
		var t: float = float(_props(kf).get("Time", 0.0))
		longest = maxf(longest, t)
		_read_poses(kf, t)
	length = maxf(longest, 1.0 / FPS)
	_length.value = length
	looping = bool(_props(seq).get("Loop", true))
	_loop.button_pressed = looping
	_name.text = anim_name
	set_time(0.0)
	return true

func _read_poses(under: int, t: float) -> void:
	for c in world.get_child_ids(under):
		var info := world.get_instance(c)
		if info.get("class_name", "") == "Pose":
			var pn := str(info.get("name", ""))
			if keys.has(pn):
				var p := _props(c)
				set_key(pn, t, _xf(p.get("CFramePosition", Vector3.ZERO), p.get("CFrameOrientation", Vector3.ZERO)))
			_read_poses(c, t)

# ---- the timeline ---------------------------------------------------------------------

func _px_per_s() -> float:
	return maxf(_timeline.size.x - 16.0, 40.0) / maxf(length, 1.0 / FPS)

func _x_of(t: float) -> float:
	return 8.0 + t * _px_per_s()

func _t_of(x: float) -> float:
	return _snap((x - 8.0) / _px_per_s())

func _draw_timeline() -> void:
	var c := _timeline
	var w := c.size.x
	var h := c.size.y
	var pal = studio.palette if studio != null and studio.get("palette") != null else null
	var dim: Color = pal.text_dim if pal != null else Color(0.6, 0.6, 0.6)
	var text: Color = pal.text if pal != null else Color(0.9, 0.9, 0.9)
	var accent: Color = Color(1, 0.55, 0.1)
	c.draw_rect(Rect2(0, 0, w, h), Color(0, 0, 0, 0.18))
	var font := c.get_theme_default_font()
	var fs := 11
	var step := 0.1
	while step * _px_per_s() < 40.0:
		step *= 2.0
	var t := 0.0
	while t <= length + 1e-6:
		var x := _x_of(t)
		c.draw_line(Vector2(x, RULER - 6), Vector2(x, RULER), dim)
		c.draw_string(font, Vector2(x + 2, RULER - 8), "%.2f" % t, HORIZONTAL_ALIGNMENT_LEFT, -1, fs, dim)
		t += step
	c.draw_line(Vector2(0, RULER), Vector2(w, RULER), dim)
	for i in joints.size():
		var y := RULER + i * ROW
		if i == _selected_joint:
			c.draw_rect(Rect2(0, y, w, ROW), Color(accent, 0.12))
		c.draw_line(Vector2(0, y + ROW), Vector2(w, y + ROW), Color(dim, 0.25))
		for k in keys.get(joints[i].name, []):
			var x := _x_of(k.t)
			var cy := y + ROW * 0.5
			var picked: bool = not _selected_key.is_empty() and _selected_key.joint == joints[i].name and absf(_selected_key.t - k.t) < 1e-6
			var d := 6.0
			var pts := PackedVector2Array([Vector2(x, cy - d), Vector2(x + d, cy), Vector2(x, cy + d), Vector2(x - d, cy)])
			c.draw_colored_polygon(pts, accent if picked else text)
	# the scrubber
	var sx := _x_of(time)
	c.draw_line(Vector2(sx, 0), Vector2(sx, h), accent, 2.0)

func _timeline_input(e: InputEvent) -> void:
	if e is InputEventMouseButton:
		var mb := e as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_LEFT:
			if mb.pressed:
				_timeline.grab_focus()
				var hit := _key_under(mb.position)
				if not hit.is_empty():
					_selected_key = hit
					_selected_joint = _joint_index(hit.joint)
					if _selected_joint >= 0:
						_list.select(_selected_joint)
					_dragging_key = true
					set_time(hit.t)
				else:
					_selected_key = {}
					_dragging_time = true
					set_time(_t_of(mb.position.x))
			else:
				_dragging_time = false
				_dragging_key = false
			_timeline.accept_event()
	elif e is InputEventMouseMotion:
		var mm := e as InputEventMouseMotion
		if _dragging_key and not _selected_key.is_empty():
			var to := _t_of(mm.position.x)
			if absf(to - _selected_key.t) >= 0.5 / FPS:
				var i := _key_at(_selected_key.joint, _selected_key.t)
				if i >= 0 and _key_at(_selected_key.joint, to) < 0:
					var tf: Transform3D = keys[_selected_key.joint][i].tf
					keys[_selected_key.joint].remove_at(i)
					set_key(_selected_key.joint, to, tf)
					_selected_key.t = to
					set_time(to)
		elif _dragging_time:
			set_time(_t_of(mm.position.x))
	elif e is InputEventKey and (e as InputEventKey).pressed and ((e as InputEventKey).keycode == KEY_DELETE or (e as InputEventKey).keycode == KEY_BACKSPACE):
		delete_selected_key()
		_timeline.accept_event()

func _joint_index(joint_name: String) -> int:
	for i in joints.size():
		if joints[i].name == joint_name:
			return i
	return -1

func _key_under(at: Vector2) -> Dictionary:
	for i in joints.size():
		var cy := RULER + i * ROW + ROW * 0.5
		if absf(at.y - cy) > ROW * 0.5:
			continue
		for k in keys.get(joints[i].name, []):
			if absf(at.x - _x_of(k.t)) <= 7.0:
				return {"joint": joints[i].name, "t": k.t}
	return {}
