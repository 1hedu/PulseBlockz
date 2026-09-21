# The terrain brush. The field is voxels held in the world node with the mesh and the
# collision shape, so a stroke under the surface leaves a cave with a roof. A drag only
# sculpts that working copy; mouse-up writes it back as one `Heights` entry, one undo.
class_name TerrainTool
extends RefCounted

enum Mode { ADD, SUBTRACT, SMOOTH, FLATTEN, PAINT }

var world: PulseBlockzWorld
var history: History

var mode: int = Mode.ADD
var radius := 16.0
var strength := 24.0        # studs per second at the middle of the brush
var material := 1280        # Enum.Material value: what Add lays down and Paint paints

var _stroking := false
var _level := 0.0           # what Flatten flattens to: where the stroke began
var _before := ""           # the field at mouse-down: a stroke matching it writes nothing
var _touched := false

func active() -> bool:
	return _stroking

# Where the pointer meets the ground, or {}: the cursor ring is drawn from this too.
func aim(from: Vector3, dir: Vector3) -> Dictionary:
	return {} if world == null else world.terrain_raycast(from, dir)

func begin(from: Vector3, dir: Vector3) -> bool:
	if world == null or world.get_terrain_id() == 0:
		return false
	var hit := aim(from, dir)
	if hit.is_empty():
		return false
	# The world's live field, not the tree's Heights, which is a stroke behind.
	_before = world.terrain_commit()
	_level = hit.position.y
	_stroking = true
	_touched = false
	_paint(hit.position, 0.016)
	return true

func drag(from: Vector3, dir: Vector3, delta: float) -> void:
	if not _stroking:
		return
	var hit := aim(from, dir)
	if hit.is_empty():
		return
	_paint(hit.position, delta)

func _paint(at: Vector3, delta: float) -> void:
	var amount: float = strength * clampf(delta, 0.001, 0.05)
	world.terrain_sculpt(at, radius, amount, mode, _level, material)
	_touched = true

# terrain_commit() only encodes the field; the history entry is what writes the tree.
func finish() -> void:
	if not _stroking:
		return
	_stroking = false
	if not _touched or history == null:
		return
	var id: int = world.get_terrain_id()
	if id == 0:
		return
	var now: String = world.terrain_commit()
	if now == _before:
		return
	history.set_property(id, "Heights", now)

func make_ground(resolution: int, cell: float, y: float) -> void:
	if world == null or history == null:
		return
	var id: int = world.get_terrain_id()
	if id == 0:
		return
	world.terrain_flatten(resolution, cell, y)
	history.set_property(id, "Heights", world.terrain_commit())
