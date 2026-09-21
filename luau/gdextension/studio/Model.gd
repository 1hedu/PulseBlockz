# A subtree of the world as Rojo `*.model.json` text: non-default, writable
# properties only. Save writes it to a file, Play hands it to the world that
# runs the place (`buildModel` in rbx_host.cpp); what it drops is absent there.
class_name Model
extends RefCounted

# Name and Parent are the tree's; the velocities are runtime state.
const NOT_SAVED := ["Name", "Parent", "AssemblyLinearVelocity", "AssemblyAngularVelocity"]
# Rounding step for the numbers `_n` snaps: a float32 0.4 reads back as
# 0.400000005960464, noise in a file people read and diff. 1e-4 cuts nothing
# real, since place geometry is not authored finer.
const PLACES := 0.0001

static func of(world: PulseBlockzWorld, id: int) -> String:
	return JSON.stringify(node(world, id, id), "  ")

static func node(world: PulseBlockzWorld, id: int, root: int) -> Dictionary:
	var info := world.get_instance(id)
	var out := {"className": info.get("class_name", "Folder")}
	var props := {}
	for p in world.get_properties(id, true):   # true: hidden ones too, such as a Script's Source
		if p.is_default or p.read_only or p.name in NOT_SAVED:
			continue
		# A twin or alias is a second spelling of one value (Image for ImageContent): write one.
		if str(p.get("twin", "")) != "" or str(p.get("alias", "")) != "":
			continue
		var written = value(world, p, root)
		if written != null:
			props[p.name] = written
	if not props.is_empty():
		out["properties"] = props
	var attrs := world.get_attributes(id)
	if not attrs.is_empty():
		var written := {}
		for key in attrs:
			var one = attribute(attrs[key])
			if one != null:
				written[key] = one
		if not written.is_empty():
			out["attributes"] = written
	var tags := world.get_tags(id)
	if not tags.is_empty():
		var tag_list := Array(tags)
		tag_list.sort()
		out["tags"] = tag_list
	var kids := []
	var order := 0
	for child in world.get_child_ids(id):
		var one := node(world, child, root)
		one["name"] = world.get_instance(child).get("name", "")
		one["_order"] = order      # scratch tiebreak for the sort, erased below
		order += 1
		kids.append(one)
	if not kids.is_empty():
		# Name order makes the file deterministic; same-named siblings stay in
		# tree order, which is what path_from's ordinals count and the order
		# buildModel re-parents them in, so "Part[2]" reloads as the same instance.
		kids.sort_custom(func(a, b): return str(a.name) < str(b.name) or (str(a.name) == str(b.name) and a._order < b._order))
		for one in kids:
			one.erase("_order")
		out["children"] = kids
	return out

# One property in the spelling the engine reads back (jsonToValue). null keeps
# it out of the file.
static func value(world: PulseBlockzWorld, p: Dictionary, root: int):
	var v = p.value
	if v == null:
		return null
	match p.type:
		"boolean", "string":
			return v
		"number":
			return _num(v)
		"Vector3":
			return [_n(v.x), _n(v.y), _n(v.z)]
		"Vector2", "UDim", "NumberRange":
			return [_n(v.x), _n(v.y)]
		"UDim2":
			return [[_n(v.x), _n(v.y)], [_n(v.z), _n(v.w)]]
		"Color3":
			return colour(v)
		"EnumItem":
			return str(v)
		"PhysicalProperties":
			return Array(v) if v is PackedFloat32Array else null
		"NumberSequence":
			var keys := []
			for i in range(0, v.size() - 2, 3):
				keys.append({"time": _n(v[i]), "value": _n(v[i + 1]), "envelope": _n(v[i + 2])})
			return {"keypoints": keys}
		"ColorSequence":
			var stops := []
			for i in range(0, v.size() - 3, 4):
				stops.append({"time": _n(v[i]), "color": colour(Color(v[i + 1], v[i + 2], v[i + 3]))})
			return {"keypoints": stops}
		"Font":
			return {"family": v.get("family", ""), "weight": int(v.get("weight", 400)),
					"style": "Italic" if v.get("italic", false) else "Normal"}
		"Content":
			# source 2 is an object (an EditableImage) a script made: no uri to write
			return str(v.get("uri", "")) if int(v.get("source", 0)) != 2 else null
		"Instance":
			return path_from(world, root, int(v))
	return null

# An attribute in the four shapes the loader reads back; anything else is dropped.
static func attribute(v):
	match typeof(v):
		TYPE_BOOL, TYPE_STRING: return v
		TYPE_INT, TYPE_FLOAT: return _num(v)
		TYPE_VECTOR3: return [_n(v.x), _n(v.y), _n(v.z)]
		TYPE_COLOR: return colour(v)
	return null

static func _n(v: float) -> float:
	return snappedf(v, PLACES)

# The instance's name, with "[k]" when a sibling shares it: k counts the
# same-named in tree order, from 1.
static func _segment(world: PulseBlockzWorld, id: int, info: Dictionary) -> String:
	var name: String = str(info.name)
	var k := 1
	var others := 0
	var found := false
	for sib in world.get_child_ids(int(info.parent)):
		if world.get_instance(sib).get("name", "") != name:
			continue
		if sib == id:
			found = true
		else:
			others += 1
			if not found:
				k += 1
	return name + "[%d]" % k if others > 0 else name

# JSON has no spelling for inf or nan and stringify writes them bare, which no
# reader takes; the loader's typed {"Float64": "inf"} form survives.
static func _num(v: float):
	if is_nan(v):
		return {"Float64": "nan"}
	if is_inf(v):
		return {"Float64": "inf" if v > 0 else "-inf"}
	return _n(v)

# Color3.fromRGB is exactly byte/255, and a script comparing against fromRGB
# expects it back to the last bit: those go as Roblox's Color3uint8.
static func colour(c: Color):
	var bytes := [c.r * 255.0, c.g * 255.0, c.b * 255.0]
	for b in bytes:
		if absf(b - roundf(b)) > 0.001 or b < 0 or b > 255:
			return [_n(c.r), _n(c.g), _n(c.b)]
	return {"Color3uint8": [roundi(bytes[0]), roundi(bytes[1]), roundi(bytes[2])]}

# Where `id` is, as a path down from `root` ("Map/Chassis"), or null when it is
# not under `root`. Siblings sharing a name get an ordinal ("Part[2]"): Roblox
# lets a door's three parts all be "Part", and the bare name finds the first.
static func path_from(world: PulseBlockzWorld, root: int, id: int):
	if id == 0 or id == root:
		return null
	var back := []
	var at := id
	for hop in 64:
		var info := world.get_instance(at)
		if info.is_empty():
			return null
		back.append(_segment(world, at, info))
		at = info.parent
		if at == root:
			back.reverse()
			return "/".join(back)
		if at == 0:
			return null
	return null
