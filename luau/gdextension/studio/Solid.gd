# Solid modelling: Union, Negate and Separate. As on Roblox, a union is one part whose
# shape is the boolean of its operands, baked into MeshData recentred on its bounds with
# Size as the bounds -- a MeshPart's layout, so Size scales the shape, not a box round it.
class_name Solid
extends RefCounted

# MeshData blob: "PBOP", u32 version, vertex count, index count, then
# position+normal floats per vertex and u32 indices; the C++ reader matches it.
const MAGIC := "PBOP"

static func _meshes_under(world: PulseBlockzWorld, handles: Handles, id: int) -> Array:
	var out := []
	for part in handles.bodies_of(id):
		# The part's own shape only: a Decal's quad, a Highlight's mesh copy and a
		# SurfaceGui's face share the body, and each of them changes the boolean.
		var shape = world.get_part_mesh(part)
		if shape is MeshInstance3D and shape.mesh != null:
			out.append([shape.mesh, shape.global_transform])
	return out

# `keep` minus `cut` as one world-space mesh, or null. A CSGShape only bakes once
# it is inside a tree and processed, so the combiner is parented hidden to the
# scene root -- outside the place, where nothing can select or save it.
static func _combine(keep: Array, cut: Array) -> ArrayMesh:
	if keep.is_empty():
		return null
	var tree := Engine.get_main_loop() as SceneTree
	if tree == null:
		return null
	var root := CSGCombiner3D.new()
	root.visible = false
	for pair in keep:
		root.add_child(_operand(pair, CSGShape3D.OPERATION_UNION))
	for pair in cut:
		root.add_child(_operand(pair, CSGShape3D.OPERATION_SUBTRACTION))
	tree.root.add_child(root)
	await tree.process_frame
	await tree.process_frame
	var made := root.get_meshes()
	var mesh: ArrayMesh = null
	if made.size() >= 2 and made[1] is ArrayMesh and made[1].get_surface_count() > 0:
		mesh = made[1]
	tree.root.remove_child(root)
	root.queue_free()
	return mesh

static func _operand(pair: Array, op: int) -> CSGMesh3D:
	var node := CSGMesh3D.new()
	node.mesh = pair[0]
	node.transform = pair[1]
	node.operation = op
	return node

# Returns [base64 blob, centre, size]; ["", ZERO, ZERO] when there is nothing to write.
static func encode(mesh: ArrayMesh) -> Array:
	if mesh == null or mesh.get_surface_count() == 0:
		return ["", Vector3.ZERO, Vector3.ZERO]
	var verts := PackedVector3Array()
	var norms := PackedVector3Array()
	var tris := PackedInt32Array()
	for s in mesh.get_surface_count():
		var arrays := mesh.surface_get_arrays(s)
		if arrays.size() <= Mesh.ARRAY_VERTEX:
			continue
		var v: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var n: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL] if arrays[Mesh.ARRAY_NORMAL] != null else PackedVector3Array()
		var idx: PackedInt32Array = arrays[Mesh.ARRAY_INDEX] if arrays[Mesh.ARRAY_INDEX] != null else PackedInt32Array()
		var base := verts.size()
		for i in v.size():
			verts.append(v[i])
			norms.append(n[i] if i < n.size() else Vector3.UP)
		if idx.is_empty():
			for i in v.size():
				tris.append(base + i)
		else:
			for i in idx.size():
				tris.append(base + idx[i])
	if verts.size() < 3 or tris.size() < 3:
		return ["", Vector3.ZERO, Vector3.ZERO]
	# Bounds become Size, their centre Position; 0.05 is the smallest a part may be.
	var low := verts[0]
	var high := verts[0]
	for v in verts:
		low = low.min(v)
		high = high.max(v)
	var middle: Vector3 = (low + high) * 0.5
	var span: Vector3 = (high - low).max(Vector3.ONE * 0.05)

	var raw := PackedByteArray()
	raw.resize(16 + verts.size() * 24 + tris.size() * 4)
	for i in 4:
		raw[i] = MAGIC.unicode_at(i)
	raw.encode_u32(4, 1)
	raw.encode_u32(8, verts.size())
	raw.encode_u32(12, tris.size())
	var at := 16
	for i in verts.size():
		var v: Vector3 = verts[i] - middle
		raw.encode_float(at, v.x); raw.encode_float(at + 4, v.y); raw.encode_float(at + 8, v.z)
		raw.encode_float(at + 12, norms[i].x); raw.encode_float(at + 16, norms[i].y); raw.encode_float(at + 20, norms[i].z)
		at += 24
	for i in tris.size():
		raw.encode_u32(at, tris[i])
		at += 4
	return [Marshalls.raw_to_base64(raw), middle, span]

# Unions the selection, subtracting any NegateOperation in it from the rest, as
# Roblox's single Union button does. Returns the new id or 0; one history batch,
# so one undo puts every operand back.
static func union(world: PulseBlockzWorld, handles: Handles, history: History,
				  ids: Array, parent: int) -> int:
	var keep := []
	var cut := []
	var real := []
	for id in ids:
		var cls: String = world.get_instance(id).get("class_name", "")
		if cls == "":
			continue
		real.append(id)
		var meshes := _meshes_under(world, handles, id)
		if cls == "NegateOperation":
			cut.append_array(meshes)
		else:
			keep.append_array(meshes)
	if keep.is_empty() or real.size() < 2:
		return 0
	var baked = await _combine(keep, cut)
	var blob := encode(baked)
	if blob[0] == "":
		return 0
	# A model, not Instance.new: UnionOperation is not creatable here or in
	# Roblox -- unioning is how one is made. One write, not eight: per-property
	# writes would draw the union growing into itself over several frames.
	var operands := []
	for id in real:
		operands.append({"name": world.get_instance(id).get("name", "Part"), "model": Model.of(world, id)})
	var node := {"className": "UnionOperation", "properties": _look_of(world, real[0])}
	# "origin" is where the union stood when made; Separate offsets from it.
	node.properties["Operands"] = JSON.stringify({"origin": [blob[1].x, blob[1].y, blob[1].z], "list": operands})
	node.properties["MeshData"] = blob[0]
	node.properties["Size"] = [blob[2].x, blob[2].y, blob[2].z]
	node.properties["Position"] = [blob[1].x, blob[1].y, blob[1].z]
	history.begin("union %d parts" % real.size())
	var made: int = await history.add(parent, "Union", JSON.stringify(node), "union %d parts" % real.size())
	if made == 0:
		history.end()
		return 0
	for id in real:
		history.destroy(id)
	history.end()
	return made

# Restores a union's operands and destroys it; a negation among them comes back
# a negation, as in Roblox.
static func separate(world: PulseBlockzWorld, handles: Handles, history: History, id: int) -> Array:
	var info := world.get_instance(id)
	if info.get("class_name", "") != "UnionOperation":
		return []
	var text := ""
	var now_pos := Vector3.ZERO
	var now_orient := Vector3.ZERO
	for p in world.get_properties(id, true):
		if p.name == "Operands":
			text = str(p.value)
		elif p.name == "Position" and p.value is Vector3:
			now_pos = p.value
		elif p.name == "Orientation" and p.value is Vector3:
			now_orient = p.value
	if text == "":
		return []
	var parsed = JSON.parse_string(text)
	var list = parsed
	var origin := now_pos          # a union stored without one counts as unmoved
	if parsed is Dictionary:
		list = parsed.get("list", [])
		var o = parsed.get("origin", null)
		if o is Array and o.size() == 3:
			origin = Vector3(float(o[0]), float(o[1]), float(o[2]))
	if not (list is Array) or list.is_empty():
		return []
	# The move since baking: a turn about the union's own centre, then a carry.
	var turn := Basis.from_euler(Vector3(deg_to_rad(now_orient.x), deg_to_rad(now_orient.y), deg_to_rad(now_orient.z)), EULER_ORDER_YXZ)
	var moved := not now_pos.is_equal_approx(origin) or not now_orient.is_equal_approx(Vector3.ZERO)
	var parent: int = info.get("parent", 0)
	history.begin("separate %d" % list.size())
	var made := []
	for one in list:
		var back: int = await history.add(parent, str(one.get("name", "Part")), str(one.get("model", "")), "separate")
		if back == 0:
			continue
		made.append(back)
		if not moved:
			continue
		for body in handles.bodies_of(back):
			var bpos := Vector3.ZERO
			var bori := Vector3.ZERO
			for p in world.get_properties(body):
				if p.name == "Position" and p.value is Vector3:
					bpos = p.value
				elif p.name == "Orientation" and p.value is Vector3:
					bori = p.value
			var basis := turn * Basis.from_euler(Vector3(deg_to_rad(bori.x), deg_to_rad(bori.y), deg_to_rad(bori.z)), EULER_ORDER_YXZ)
			var e := basis.get_euler(EULER_ORDER_YXZ)
			history.set_property(body, "Position", now_pos + turn * (bpos - origin))
			history.set_property(body, "Orientation", Vector3(rad_to_deg(e.x), rad_to_deg(e.y), rad_to_deg(e.z)))
	history.destroy(id)
	history.end()
	return made

# Replaces each selected part with a ghost that cuts its shape out when unioned.
static func negate(world: PulseBlockzWorld, handles: Handles, history: History, ids: Array) -> Array:
	var made := []
	history.begin("negate %d" % ids.size())
	for id in ids:
		var cls: String = world.get_instance(id).get("class_name", "")
		if cls == "" or cls == "NegateOperation":
			continue
		var parent: int = world.get_instance(id).get("parent", 0)
		var baked = await _combine(_meshes_under(world, handles, id), [])
		var blob := encode(baked)
		if blob[0] == "":
			continue
		var node := {"className": "NegateOperation", "properties": {
			"MeshData": blob[0],
			"Size": [blob[2].x, blob[2].y, blob[2].z],
			"Position": [blob[1].x, blob[1].y, blob[1].z],
			"Transparency": 0.7,
			"CanCollide": false,
			"Anchored": true,
			"Color": Model.colour(Color(0.85, 0.2, 0.2)),
		}}
		var neg: int = await history.add(parent, world.get_instance(id).get("name", "Part"),
										 JSON.stringify(node), "negate")
		if neg == 0:
			continue
		history.destroy(id)
		made.append(neg)
	history.end()
	return made

# Appearance carried from the first operand, spelled as a *.model.json spells it.
const LOOK := ["Color", "Material", "Transparency", "Reflectance", "Anchored", "CanCollide", "CastShadow"]

static func _look_of(world: PulseBlockzWorld, id: int) -> Dictionary:
	var out := {}
	for p in world.get_properties(id):
		if not LOOK.has(p.name) or p.is_default:
			continue
		match p.type:
			"Color3": out[p.name] = Model.colour(p.value)
			"EnumItem": out[p.name] = str(p.value)
			"number": out[p.name] = float(p.value)
			"boolean": out[p.name] = bool(p.value)
	return out
