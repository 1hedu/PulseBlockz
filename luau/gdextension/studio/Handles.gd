# Move, resize and rotate the selection in the viewport, as Studio's three tools do.
# A lone part is handled on its own axes (Size is along them); a group gets a
# world-aligned box. Picking and dragging take rays, so a test can supply its own.
class_name Handles
extends Node3D

signal changed          # a drag wrote something

# Roblox's tool order: 1 Select, 2 Move, 3 Scale, 4 Rotate.
enum Mode { SELECT, MOVE, SCALE, ROTATE }

const AXIS_COLOUR := Palette.AXIS      # world axes, not the UI theme
const GRAB_PIXELS := 20.0        # how near the pointer must be to take a handle
const MIN_SIZE := 0.05           # Roblox's smallest part
const RING := 1.6                # rotate ring clearance outside the box, in handle sizes
const FREE := 6                  # past the six arrows: the part's own body

var world: PulseBlockzWorld
var history: History      # a drag is one undo entry
var camera: Camera3D
# The tool cannot change mid-drag: _from was taken under the starting mode, and
# the grip is studs in one mode and radians in another.
var mode := Mode.MOVE:
	set(value):
		if _held == -1:
			mode = value
var snap := 1.0                  # studs; 0 for none
var rotate_snap := 15.0          # degrees; 0 for none
var world_axes := false          # move and turn on world axes, not the part's
var surface := true              # dragging the body lands it flush on what is under it

var ids: Array = []              # the parts being handled, in selection order
var _arrows := []                # 6 [shaft, head] pairs, indexed axis * 2 + direction
var _rings := []
var _blocks := []
var _held := -1                  # axis * 2 + direction, the ring's axis when rotating, or FREE
var _from := {}                  # the selection as the drag found it

func _ready():
	for axis in 3:
		for dir in 2:
			_arrows.append([_mesh(_shaft(), axis), _mesh(_head(), axis)])
			_blocks.append(_mesh(_block(), axis))
		_rings.append(_mesh(_ring(), axis))
	visible = false

func target(selection: Array) -> void:
	if _held != -1:
		return
	ids = selection.duplicate()
	_body_for = [0]       # sentinel: forces the walk again, even for the same ids

func primary() -> int:
	return ids.back() if not ids.is_empty() else 0

# What the handles move: every BasePart under the selection. A part stands for
# itself and is not descended into -- on Roblox only a Model's members move
# together, a child part does not follow its parent. Cached: asked every frame.
# Each one takes its own Position write: nothing simulates in an edit world, so no
# weld drags a limb along behind the part that was grabbed.
var _body_cache: Array = []
var _body_for: Array = []

func bodies() -> Array:
	if _body_for != ids:
		_body_for = ids.duplicate()
		_body_cache = []
		var seen := {}
		for id in ids:
			_bodies_under(id, _body_cache, seen, 0)
	return _body_cache

# The same walk, uncached, for any id: solid modelling works on parts that are not selected.
func bodies_of(id: int) -> Array:
	var out := []
	_bodies_under(id, out, {}, 0)
	return out

func _bodies_under(id: int, out: Array, seen: Dictionary, depth: int) -> void:
	if world == null or id == 0 or depth > 12 or seen.has(id):
		return
	if is_part(id):
		seen[id] = true
		out.append(id)
		return
	for c in world.get_child_ids(id):
		_bodies_under(c, out, seen, depth + 1)

# Anything the world will give a Position and a Size for is a BasePart.
func is_part(id: int) -> bool:
	var pos := false
	var size := false
	for p in world.get_properties(id):
		if p.name == "Position": pos = true
		elif p.name == "Size": size = true
	return pos and size

# One part standing for itself: the only case with axes of its own and a Size that means something.
func _lone_part() -> bool:
	var use := bodies()
	return ids.size() == 1 and use.size() == 1 and use[0] == ids[0]

# Scale needs a single thing to scale: one part, or one Model scaled whole.
func can_scale() -> bool:
	return ids.size() == 1 and not bodies().is_empty()

func dragging() -> bool:
	return _held != -1

# ---- what is where ----------------------------------------------------------------
# The frame as drawn: middle, axes, and half the way out to each face.
func frame() -> Dictionary:
	if world == null or ids.is_empty():
		return {}
	var use := bodies()
	if use.is_empty():
		return {}
	if _lone_part():
		var one := frame_of(use[0])
		# Size is along the part's own axes, so SCALE ignores world_axes.
		if world_axes and mode != Mode.SCALE and not one.is_empty():
			var reach: Vector3 = _abs(one.basis.x) * one.half.x + _abs(one.basis.y) * one.half.y + _abs(one.basis.z) * one.half.z
			return {"middle": one.middle, "basis": Basis.IDENTITY, "half": reach}
		return one
	# A group shares no axes of its own: the world-aligned box round the lot.
	var low := Vector3.INF
	var high := -Vector3.INF
	for one in use:
		var f := frame_of(one)
		if f.is_empty():
			continue
		# How far this part reaches along each world axis, however it is turned.
		var reach: Vector3 = _abs(f.basis.x) * f.half.x + _abs(f.basis.y) * f.half.y + _abs(f.basis.z) * f.half.z
		low = low.min(f.middle - reach)
		high = high.max(f.middle + reach)
	if low.x > high.x:
		return {}
	return {"middle": (low + high) * 0.5, "basis": Basis.IDENTITY, "half": (high - low) * 0.5}

static func _abs(v: Vector3) -> Vector3:
	return Vector3(absf(v.x), absf(v.y), absf(v.z))

func frame_of(id: int) -> Dictionary:
	if world == null or id == 0:
		return {}
	var body := world.get_part_node(id)
	if body == null:
		return {}
	var drawn: MeshInstance3D = null
	var part_name: String = world.get_instance(id).get("name", "")
	for c in body.get_children():
		if c is MeshInstance3D and (c.name == part_name or drawn == null):
			drawn = c
			if c.name == part_name:
				break
	if drawn == null:
		return {}
	var t := drawn.global_transform
	var bounds := drawn.get_aabb()
	return {
		"middle": t * bounds.get_center(),
		"basis": t.basis.orthonormalized(),
		"half": (t.basis.get_scale() * bounds.size) * 0.5,
	}

# Handle size, grown with camera distance so a handle stays the same size on screen.
func _reach(middle: Vector3) -> float:
	return max(camera.global_position.distance_to(middle) * 0.045, 0.15)

func _anchor(f: Dictionary, handle: int, reach: float) -> Vector3:
	var axis: int = handle / 2
	var way := 1.0 if handle % 2 == 0 else -1.0
	var out: Vector3 = _col(f.basis, axis) * way
	return f.middle + out * (f.half[axis] + reach * 1.6)

# ---- drawing ----------------------------------------------------------------------
func _process(_delta):
	if camera == null:
		return
	var f := frame()
	visible = not f.is_empty() and mode != Mode.SELECT
	if not visible:
		return
	var reach := _reach(f.middle)
	for handle in 6:
		var axis: int = handle / 2
		var way := 1.0 if handle % 2 == 0 else -1.0
		var out: Vector3 = _col(f.basis, axis) * way
		var foot: Vector3 = f.middle + out * f.half[axis]
		var tip := _anchor(f, handle, reach)
		var aim := _aim(out)
		var shaft: MeshInstance3D = _arrows[handle][0]
		var head: MeshInstance3D = _arrows[handle][1]
		var block: MeshInstance3D = _blocks[handle]
		shaft.visible = mode == Mode.MOVE
		head.visible = mode == Mode.MOVE
		block.visible = mode == Mode.SCALE and can_scale()
		if mode == Mode.MOVE:
			shaft.global_transform = Transform3D(aim.scaled(Vector3(reach * 0.12, foot.distance_to(tip), reach * 0.12)), (foot + tip) * 0.5)
			head.global_transform = Transform3D(aim.scaled(Vector3(reach * 0.45, reach * 0.9, reach * 0.45)), tip)
		elif mode == Mode.SCALE and can_scale():
			block.global_transform = Transform3D(aim.scaled(Vector3(reach * 0.55, reach * 0.55, reach * 0.55)), foot + out * reach * 0.35)
	for axis in 3:
		var ring: MeshInstance3D = _rings[axis]
		ring.visible = mode == Mode.ROTATE
		if ring.visible:
			var radius := _radius(f, axis, reach)
			ring.mesh.inner_radius = radius - reach * 0.09
			ring.mesh.outer_radius = radius + reach * 0.09
			ring.global_transform = Transform3D(_aim(_col(f.basis, axis)), f.middle)

func _radius(f: Dictionary, axis: int, reach: float) -> float:
	var a: int = (axis + 1) % 3
	var b: int = (axis + 2) % 3
	return max(f.half[a], f.half[b]) + reach * RING

# ---- picking ----------------------------------------------------------------------
# The handle a ray aims at, or -1: arrows and blocks in screen pixels, a ring by
# where the ray crosses the ring's plane.
func pick(from: Vector3, dir: Vector3) -> int:
	var f := frame()
	if f.is_empty() or camera == null or mode == Mode.SELECT:
		return -1
	var reach := _reach(f.middle)
	if mode == Mode.ROTATE:
		var best := -1
		var nearest := INF
		for axis in 3:
			var normal: Vector3 = _col(f.basis, axis)
			var at = _plane_hit(from, dir, f.middle, normal)
			if at == null:
				continue
			var off: float = absf((at - f.middle).length() - _radius(f, axis, reach))
			if off < reach * 0.5 and off < nearest:
				nearest = off
				best = axis
		return best
	if mode == Mode.SCALE and not can_scale():
		return -1
	var at_pixel := _to_screen(from + dir)
	var best_handle := -1
	var nearest_pixels := GRAB_PIXELS
	for handle in 6:
		var spot: Vector3 = _anchor(f, handle, reach)
		if mode == Mode.SCALE:
			var axis: int = handle / 2
			var way := 1.0 if handle % 2 == 0 else -1.0
			var out: Vector3 = _col(f.basis, axis) * way
			spot = f.middle + out * (f.half[axis] + reach * 0.35)
		if camera.is_position_behind(spot):
			continue
		var away := camera.unproject_position(spot).distance_to(at_pixel)
		if away < nearest_pixels:
			nearest_pixels = away
			best_handle = handle
	return best_handle

func _to_screen(at: Vector3) -> Vector2:
	return camera.unproject_position(at) if not camera.is_position_behind(at) else Vector2.ZERO

# ---- dragging ---------------------------------------------------------------------
func begin(handle: int, from: Vector3, dir: Vector3) -> bool:
	var f := frame()
	if handle < 0 or f.is_empty():
		return false
	var parts := _parts_now()
	if parts.is_empty():
		return false
	_held = handle
	_from = {"middle": f.middle, "basis": f.basis, "half": f.half, "parts": parts,
			 "lone": _lone_part()}
	var along = _grip(from, dir)
	if along == null:
		_held = -1
		return false
	_from["grip"] = along
	history.begin(["", "move ", "scale ", "turn "][mode] + _what())
	return true

func _what() -> String:
	return world.get_instance(primary()).get("name", "") if ids.size() == 1 else "%d parts" % ids.size()

# Taking hold of the brick itself, which is Roblox's dragger: it lands on the
# surface under the pointer, while the arrows stay a plain constrained slide.
func begin_free(from: Vector3, dir: Vector3) -> bool:
	var f := frame()
	if f.is_empty() or ids.is_empty():
		return false
	var parts := _parts_now()
	if parts.is_empty():
		return false
	# Excluded by RID: the colliders still sit at the old pose while the writes are
	# in flight, so an un-excluded ray would land the part on itself.
	var skip := []
	for one in parts:
		var body := world.get_part_node(one) as CollisionObject3D
		if body != null:
			skip.append(body.get_rid())
	var hit := _cast(from, dir, skip)
	if hit.is_empty():
		return false
	# The lead part needs a drawn box to land on: a welded limb, a Tool's Handle or
	# anything outside the Workspace has a Position and a Size but no body node.
	var lead: int = primary()
	var box: Dictionary = frame_of(lead) if parts.has(lead) else {}
	if box.is_empty():
		for one in parts:
			box = frame_of(one)
			if not box.is_empty():
				lead = one
				break
	if box.is_empty():
		return false
	_held = FREE
	_from = {"middle": f.middle, "basis": f.basis, "half": f.half, "parts": parts,
			 "skip": skip, "grab": hit.position, "n0": hit.normal,
			 "lead": lead, "box": box}
	history.begin("drag " + _what())
	return true

# The bodies as they stand now. A drag snapshots this once, at begin: the writes are
# queued for the runtime thread and land a frame or two later, so a drag fed on its
# own output would chase itself.
func _parts_now() -> Dictionary:
	var parts := {}
	for one in bodies():
		var props := {}
		for p in world.get_properties(one):
			props[p.name] = p.value
		if props.has("Position") and props.has("Size"):
			parts[one] = {"position": props["Position"], "size": props["Size"],
						  "orientation": props.get("Orientation", Vector3.ZERO)}
	return parts

func _cast(from: Vector3, dir: Vector3, skip: Array) -> Dictionary:
	if camera == null:
		return {}
	var query := PhysicsRayQueryParameters3D.create(from, from + dir * 4000.0, 1 << (world.physics_layer - 1))
	query.exclude = skip
	return camera.get_world_3d().direct_space_state.intersect_ray(query)

func drag(from: Vector3, dir: Vector3) -> void:
	if _held == -1:
		return
	if _held == FREE:
		_slide_on_surface(from, dir)
		return
	var along = _grip(from, dir)
	if along == null:
		return
	if mode == Mode.ROTATE:
		_turn(along - _from.grip)
	else:
		_slide(along - _from.grip)

func finish() -> void:
	if _held != -1:
		history.end()
	_held = -1
	_from = {}

# Studs along the axis for an arrow or a block, radians round the ring for a rotate.
func _grip(from: Vector3, dir: Vector3):
	var axis: int = _held if mode == Mode.ROTATE else _held / 2
	var out: Vector3 = _col(_from.basis, axis)
	if mode == Mode.ROTATE:
		var at = _plane_hit(from, dir, _from.middle, out)
		if at == null:
			return null
		var flat: Vector3 = at - _from.middle
		var a: Vector3 = _col(_from.basis, (axis + 1) % 3)
		var b: Vector3 = _col(_from.basis, (axis + 2) % 3)
		return atan2(flat.dot(b), flat.dot(a))
	return _line_hit(from, dir, _from.middle, out)

func _slide(moved: float) -> void:
	var axis: int = _held / 2
	var way := 1.0 if _held % 2 == 0 else -1.0
	var out: Vector3 = _col(_from.basis, axis)
	if mode == Mode.MOVE:
		var step := _snapped(moved, snap)
		if step == 0.0:
			return
		for one in _from.parts:
			history.set_property(one, "Position", _from.parts[one].position + out * step)
	elif not _from.get("lone", true):
		# A Model scales whole about the box middle -- every Size and every offset
		# by the same factor -- as Roblox's Model:ScaleTo does.
		var was: float = _from.half[axis]
		if was < 1e-4:
			return
		var grown := maxf(_snapped(was + moved * way, snap), MIN_SIZE)
		var k: float = grown / was
		if is_equal_approx(k, 1.0):
			return
		for one in _from.parts:
			var st: Dictionary = _from.parts[one]
			history.set_property(one, "Size", (st.size * k).max(Vector3.ONE * MIN_SIZE))
			history.set_property(one, "Position", _from.middle + (st.position - _from.middle) * k)
	else:
		# The pulled face moves and the far one stays: the middle shifts by half the growth.
		var only: int = primary()
		var start: Dictionary = _from.parts[only]
		# The part's own axis, not the drawn box's: a Cylinder lies along X inside its
		# body and a Wedge is turned a quarter, so the drawn axis picks the wrong Size.
		var pulled: Vector3 = out * way
		var own: int = _down_face(_euler_basis(start.orientation), pulled)[0]
		var was: float = start.size[own]
		var grown := maxf(_snapped(was + moved * way, snap), MIN_SIZE)
		if grown == was:
			return
		var size: Vector3 = start.size
		size[own] = grown
		history.set_property(only, "Size", size)
		history.set_property(only, "Position", start.position + pulled * (grown - was) * 0.5)
	changed.emit()

# Land the selection flush on what is under the pointer. The turn is the shortest
# swing from the face it started on to the face it is on now, applied to the
# orientation it had, so a yawed brick keeps that yaw as roll -- as on Roblox.
func _slide_on_surface(from: Vector3, dir: Vector3) -> void:
	var hit := _cast(from, dir, _from.skip)
	if hit.is_empty():
		return
	var n: Vector3 = hit.normal
	var lead: int = _from.lead
	if not _from.parts.has(lead):
		return
	var start: Dictionary = _from.parts[lead]
	var spin := _swing(_from.n0, n, _col(_euler_basis(start.orientation), 1))
	var turned := spin * _euler_basis(start.orientation)

	# Stand-off along the normal, from the drawn box: Size/2 leaves a Ball or a
	# Cylinder floating, their mesh being only as wide as Size's smallest component.
	var box: Basis = spin * _from.box.basis
	var reach: Vector3 = _from.box.half
	var lift: float = absf(box.x.dot(n)) * reach.x 					+ absf(box.y.dot(n)) * reach.y 					+ absf(box.z.dot(n)) * reach.z

	# Keep the grab: a plank taken by one end stays held by that end.
	var flat: Vector3 = spin * (start.position - _from.grab)
	flat -= n * flat.dot(n)
	if snap > 0.0:
		# Snapped on the face's own grid, so a brick lines up with a turned surface.
		var t1 := n.cross(Vector3.UP if absf(n.y) < 0.9 else Vector3.RIGHT).normalized()
		var t2 := n.cross(t1).normalized()
		flat = t1 * snappedf(flat.dot(t1), snap) + t2 * snappedf(flat.dot(t2), snap)
	var middle: Vector3 = hit.position + flat + n * lift

	for one in _from.parts:
		var st: Dictionary = _from.parts[one]
		history.set_property(one, "Orientation", _basis_euler(spin * _euler_basis(st.orientation)))
		history.set_property(one, "Position", middle + spin * (st.position - start.position))
	changed.emit()

# The face of a box most opposed to a normal, as [own axis, way out through it].
static func _down_face(basis: Basis, normal: Vector3) -> Array:
	var best := 0
	var way := 1.0
	var most := -INF
	for a in 3:
		var d: float = _col(basis, a).dot(normal)
		if absf(d) > most:
			most = absf(d)
			best = a
			way = -1.0 if d > 0.0 else 1.0
	return [best, way]

# The shortest turn taking `a` onto `b`. `spare` is the axis to spin about when
# they are exactly opposed and no shortest turn exists.
static func _swing(a: Vector3, b: Vector3, spare: Vector3) -> Basis:
	var axis := a.cross(b)
	if axis.length() < 1e-6:
		if a.dot(b) > 0.0:
			return Basis.IDENTITY
		# Any axis square to `a` flips it onto `b`, hence squaring the spare off: the
		# spare is often `a` itself, and spinning about `a` yaws instead of flipping.
		var turn := spare - a * spare.dot(a)
		if turn.length_squared() < 1e-12:
			turn = a.cross(Vector3.UP if absf(a.y) < 0.9 else Vector3.RIGHT)
		return Basis(turn.normalized(), PI)
	return Basis(axis.normalized(), a.angle_to(b))

func _turn(radians: float) -> void:
	var degrees := _snapped(rad_to_deg(radians), rotate_snap)
	if degrees == 0.0:
		return
	var out: Vector3 = _col(_from.basis, _held)
	var spin := Basis(out, deg_to_rad(degrees))
	# Each part turns on the spot and swings round the middle of the selection.
	for one in _from.parts:
		var start: Dictionary = _from.parts[one]
		history.set_property(one, "Orientation", _basis_euler(spin * _euler_basis(start.orientation)))
		var offset: Vector3 = start.position - _from.middle
		if offset.length_squared() > 1e-9:
			history.set_property(one, "Position", _from.middle + spin * offset)
	changed.emit()

# Roblox's Orientation is YXZ Euler degrees (rotY * rotX * rotZ): EULER_ORDER_YXZ.
static func _euler_basis(orientation: Vector3) -> Basis:
	return Basis.from_euler(Vector3(deg_to_rad(orientation.x), deg_to_rad(orientation.y), deg_to_rad(orientation.z)), EULER_ORDER_YXZ)

static func _basis_euler(basis: Basis) -> Vector3:
	var e := basis.orthonormalized().get_euler(EULER_ORDER_YXZ)
	return Vector3(rad_to_deg(e.x), rad_to_deg(e.y), rad_to_deg(e.z))

static func _snapped(value: float, step: float) -> float:
	return value if step <= 0.0 else snappedf(value, step)

# ---- ray maths --------------------------------------------------------------------
# How far along the axis (through `origin`) the ray comes nearest it: the nearest
# points of two skew lines.
static func _line_hit(from: Vector3, dir: Vector3, origin: Vector3, axis: Vector3):
	var w := origin - from
	var a := axis.dot(axis)
	var b := axis.dot(dir)
	var c := dir.dot(dir)
	var d := axis.dot(w)
	var e := dir.dot(w)
	var det := a * c - b * b
	if absf(det) < 1e-8:
		return null
	return (b * e - c * d) / det

static func _plane_hit(from: Vector3, dir: Vector3, origin: Vector3, normal: Vector3):
	var facing := normal.dot(dir)
	if absf(facing) < 1e-6:
		return null
	var t := normal.dot(origin - from) / facing
	return null if t <= 0.0 else from + dir * t

# GDScript's Basis has no get_column: x, y and z are the columns ([] is rows).
static func _col(b: Basis, i: int) -> Vector3:
	return b.x if i == 0 else (b.y if i == 1 else b.z)

static func _aim(y: Vector3) -> Basis:
	var up := Vector3.UP if absf(y.dot(Vector3.UP)) < 0.99 else Vector3.RIGHT
	var x := up.cross(y).normalized()
	return Basis(x, y, x.cross(y).normalized())

# ---- the meshes -------------------------------------------------------------------
func _mesh(shape: Mesh, axis: int) -> MeshInstance3D:
	var node := MeshInstance3D.new()
	node.mesh = shape
	node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.no_depth_test = true            # a handle inside a brick is still grabbable
	mat.albedo_color = AXIS_COLOUR[axis]
	node.material_override = mat
	node.visible = false
	add_child(node)
	return node

static func _shaft() -> Mesh:
	var m := CylinderMesh.new()
	m.top_radius = 1.0
	m.bottom_radius = 1.0
	m.height = 1.0
	m.radial_segments = 8
	m.rings = 0
	return m

static func _head() -> Mesh:
	var m := CylinderMesh.new()
	m.top_radius = 0.0
	m.bottom_radius = 1.0
	m.height = 1.0
	m.radial_segments = 10
	m.rings = 0
	return m

static func _block() -> Mesh:
	var m := BoxMesh.new()
	m.size = Vector3.ONE
	return m

static func _ring() -> Mesh:
	var m := TorusMesh.new()
	m.inner_radius = 0.9
	m.outer_radius = 1.0
	m.rings = 32
	m.ring_segments = 6
	return m
