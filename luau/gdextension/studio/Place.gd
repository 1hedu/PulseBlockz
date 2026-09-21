# A place directory read into a world's Instance tree by Rojo's layout rules: a file's path
# under the DataModel decides what it becomes, default.project.json says which directory is
# which service, and each instance keeps its disk path for Save. ScriptSync.gd mirrors these rules.
class_name Place
extends RefCounted

var dir := ""
var files := {}      # instance path plus the file's suffix ("Workspace/Map/Build.server.luau")
                     # -> disk path, or a project node's meta JSON, told apart by a leading "{"
var mounts := {}     # bare instance path ("Workspace/Map") -> the directory mounted there
var changed := {}    # files the last Save rewrote; one already current is not in here

# Read the place into the world in one batch: no script runs until every file is in the tree,
# so a require() at startup resolves without WaitForChild.
func open(world: PulseBlockzWorld, place_dir: String) -> int:
	dir = place_dir
	files = _collect(place_dir)
	var names := files.keys()
	names.sort()
	for name in names:
		var from: String = files[name]
		if from.begins_with("{"):                  # a project node's meta, not a file
			world.load_file(name, from)
		elif name.ends_with(".rbxm"):              # binary: the source is its base64
			world.load_file(name, Marshalls.raw_to_base64(FileAccess.get_file_as_bytes(from)))
		else:
			var f := FileAccess.open(from, FileAccess.READ)
			if f != null:
				world.load_file(name, f.get_as_text())
	return names.size()

# Disk path an instance came from; "" when a project node, not a file, made it.
func source_of(instance_path: String) -> String:
	var from: String = files.get(instance_path, "")
	return "" if from.begins_with("{") else from

# The file an instance saves to, under the deepest mount that covers it. ""
# means no mount does, and the place has nowhere to put it.
func file_for(instance_path: String) -> String:
	var deepest := ""
	for mounted in mounts:
		if (instance_path == mounted or instance_path.begins_with(mounted + "/")) and mounted.length() > deepest.length():
			deepest = mounted
	if deepest == "":
		return ""
	var rest := instance_path.substr(deepest.length()).trim_prefix("/")
	if rest == "":
		return ""
	return mounts[deepest].path_join(rest + ".model.json")

# Give a service somewhere to save to: make a directory and write its $path into
# default.project.json, the one file the Studio touches outside the place's own.
# The $path goes on the service, the level Rojo mounts at and children inherit.
# Returns the directory now mounted, or "" if the project could not be written.
func make_mount(instance_path: String) -> String:
	var service := instance_path.get_slice("/", 0)
	if dir == "" or service == "" or service == instance_path:
		return ""            # Save writes no file for a service itself
	if mounts.has(service):
		return mounts[service]
	var project := dir.path_join("default.project.json")
	var tree = JSON.parse_string(FileAccess.get_file_as_string(project)) if FileAccess.file_exists(project) else null
	if typeof(tree) != TYPE_DICTIONARY:
		tree = {"name": dir.get_file(), "tree": {"$className": "DataModel"}}
	if typeof(tree.get("tree")) != TYPE_DICTIONARY:
		return ""
	# An empty `mounts` does not mean the project is silent: a $path naming a
	# single file, and Rojo's {"optional": ...} form, both leave no mount.
	# Repointing an existing $path would orphan whatever it names.
	var node = tree.tree.get(service, {})
	if typeof(node) != TYPE_DICTIONARY or node.has("$path"):
		return ""
	var taken := {}
	for mounted in mounts:
		taken[mounts[mounted]] = true
	var stem := "src/" + service.to_snake_case()
	var rel := stem
	var n := 1
	# A directory with anything in it belongs to something else: adopting it
	# would pull its files into the tree on the next open.
	while taken.has(dir.path_join(rel)) or not _empty_dir(dir.path_join(rel)):
		n += 1
		rel = "%s%d" % [stem, n]
	var disk := dir.path_join(rel)
	if DirAccess.make_dir_recursive_absolute(disk) != OK:
		return ""
	# $path first, then the node's other keys in the order they were read: a
	# project file is edited by hand.
	var put := {"$path": rel}
	for k in node:
		if k != "$path":
			put[k] = node[k]
	tree.tree[service] = put
	var f := FileAccess.open(project, FileAccess.WRITE)
	if f == null:
		return ""
	f.store_string(JSON.stringify(tree, "  ", false) + "
")
	mounts[service] = disk
	return disk

# True when the directory is missing or holds nothing; false when it will not open.
static func _empty_dir(path: String) -> bool:
	if not DirAccess.dir_exists_absolute(path):
		return true
	var d := DirAccess.open(path)
	if d == null:
		return false
	d.list_dir_begin()
	var empty := d.get_next() == ""
	d.list_dir_end()
	return empty

func _collect(place_dir: String) -> Dictionary:
	mounts.clear()
	var out := {}
	var project := place_dir.path_join("default.project.json")
	if FileAccess.file_exists(project):
		var tree = JSON.parse_string(FileAccess.get_file_as_string(project))
		if typeof(tree) == TYPE_DICTIONARY and tree.has("tree"):
			_mount(place_dir, tree.tree, "", out)
		else:
			push_error("Place: %s has no tree" % project)
	else:
		for rel in _walk(place_dir, ""):
			out[rel] = place_dir.path_join(rel)
	return out

# Extensions Save writes, and so may also remove. A *.meta.json is not among
# them: it belongs to the project file, not to an instance.
const OURS := [".model.json", ".server.luau", ".client.luau", ".luau", ".server.lua", ".client.lua", ".lua"]

# Remove files under the mounts that this Save did not write, so an instance
# deleted in the Studio does not come back on the next open. The test is what
# Save wrote, not what instance is gone: a folder now kept as a directory also
# leaves a stale model behind, and no missing instance points at it.
func sweep(written: Dictionary) -> Array:
	var gone := []
	for mounted in mounts:
		var root: String = mounts[mounted]
		for path in _ours_in(root, []):
			if written.has(path) or String(path).get_file().begins_with("init."):
				continue
			if DirAccess.remove_absolute(path) == OK:
				gone.append(path)
	for path in gone:
		for named in files.keys():
			if files[named] == path:
				files.erase(named)
	return gone

func _ours_in(base: String, found: Array) -> Array:
	var d := DirAccess.open(base)
	if d == null:
		return found
	d.list_dir_begin()
	var n := d.get_next()
	while n != "":
		if d.current_is_dir():
			if not n.begins_with("."):
				_ours_in(base.path_join(n), found)
		elif not n.ends_with(".meta.json") and _owned_suffix(n) != "":
			found.append(base.path_join(n))
		n = d.get_next()
	return found

static func _owned_suffix(file: String) -> String:
	for one in OURS:
		if file.ends_with(one):
			return one
	return ""

# The suffix of the file an instance came from (".server.luau", ".rbxmx",
# ".model.json"), or "" when nothing on disk made it.
const INIT := ["/init.server.luau", "/init.client.luau", "/init.luau",
			   "/init.server.lua", "/init.client.lua", "/init.lua"]

func source_suffix(instance_path: String) -> String:
	for named in files:
		if named.begins_with(instance_path + "."):
			return named.substr(instance_path.length())
	# A script with children is a directory, and its own source is the init.*
	# inside it -- Rojo's spelling, and the runtime's.
	for one in INIT:
		if files.has(instance_path + one):
			return one
	return ""

# True when the place keeps files under this instance: it is a directory on
# disk, and writing it as one model would orphan every script inside.
func holds_files(instance_path: String) -> bool:
	for named in files:
		if named.begins_with(instance_path + "/"):
			return true
	return false

# The extension Rojo gives a new script of each class.
const SCRIPT_FILE := {"Script": ".server.luau", "LocalScript": ".client.luau", "ModuleScript": ".luau"}

# Characters an instance name may hold but a file name may not: each writes a
# path nobody meant -- on Windows a colon opens an alternate data stream -- so
# a save that would use one is refused.
const ILLEGAL := [":", "*", "?", "\"", "<", ">", "|", "/", "\\"]

static func nameable(file: String) -> bool:
	for bad in ILLEGAL:
		if file.get_file().contains(bad):
			return false
	return true

func save_script(world: PulseBlockzWorld, id: int, instance_path: String) -> String:
	var suffix := source_suffix(instance_path)
	var to: String = files.get(instance_path + suffix, "")
	if to == "":
		var kind: String = world.get_instance(id).get("class_name", "")
		if not SCRIPT_FILE.has(kind):
			return ""
		suffix = SCRIPT_FILE[kind]
		var where := file_for(instance_path)     # gives ".model.json"; swap the tail
		if where == "":
			return ""
		to = where.substr(0, where.length() - ".model.json".length()) + suffix
	if not nameable(to):
		return ""
	var text := ""
	for prop in world.get_properties(id, true):   # true: include hidden, where Source is
		if prop.name == "Source":
			text = str(prop.value)
	if not _put(to, text):
		return ""
	files[instance_path + suffix] = to
	return to

# A model authored as an *.rbxmx is written back as one. The XML is built in
# C++, where the class table says what shape each property takes in that file.
func save_rbxmx(world: PulseBlockzWorld, id: int, instance_path: String) -> String:
	var to: String = files.get(instance_path + ".rbxmx", "")
	if to == "" or not nameable(to):
		return ""
	var xml: String = world.to_rbxmx(Model.of(world, id), instance_path.get_file())
	if xml == "":
		return ""
	return to if _put(to, xml) else ""

# The services a Roblox place file carries. The rest (RunService, HttpService)
# hold nothing to save.
const PLACE_SERVICES := ["Workspace", "Players", "Lighting", "MaterialService", "ReplicatedFirst", "ReplicatedStorage",
	"ServerScriptService", "ServerStorage", "StarterGui", "StarterPack", "StarterPlayer", "Teams", "SoundService",
	"Chat", "TextChatService", "LocalizationService", "TestService"]

# The place as the .rbxlx Roblox Studio opens: each PLACE_SERVICES service in
# the tree with everything under it, Refs resolved across the file (a Model's
# PrimaryPart, the Workspace's CurrentCamera). "" when no service is in the tree.
static func rbxlx_of(world: PulseBlockzWorld) -> String:
	var services := []
	for id in world.get_child_ids(0):
		var info := world.get_instance(id)
		if not PLACE_SERVICES.has(info.get("class_name", "")):
			continue
		var one := Model.node(world, id, 0)
		one["name"] = info.get("name", info.get("class_name", ""))
		# The ground rides along as Roblox's own SmoothGrid, under a Terrain child.
		if info.get("class_name", "") == "Workspace" and int(world.get_terrain_info().get("resolution", 0)) > 0:
			var grid: String = world.terrain_smoothgrid()
			if grid != "":
				if not one.has("children"):
					one["children"] = []
				one["children"].append({"className": "Terrain", "name": "Terrain", "properties": {"SmoothGrid": grid, "MaterialColors": world.terrain_material_colors()}})
		# An edit world sets no CurrentCamera, that being a client's; without one
		# Studio adds a second Camera beside the one already in the tree.
		if info.get("class_name", "") == "Workspace" and not one.get("properties", {}).has("CurrentCamera"):
			for kid in world.get_child_ids(id):
				var kinfo := world.get_instance(kid)
				if kinfo.get("class_name", "") == "Camera":
					if not one.has("properties"):
						one["properties"] = {}
					one["properties"]["CurrentCamera"] = "Workspace/" + str(kinfo.get("name", "Camera"))
					break
		services.append(one)
	if services.is_empty():
		return ""
	return world.to_rbxlx(JSON.stringify({"className": "DataModel", "children": services}))

func save_rbxlx(world: PulseBlockzWorld, to: String) -> bool:
	var xml := rbxlx_of(world)
	if xml == "":
		return false
	var f := FileAccess.open(to, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string(xml)
	return true

# Write an instance and its subtree as the *.model.json the loader reads back.
# Returns the file written, or "" when the place has nowhere for it.
func save(world: PulseBlockzWorld, id: int, instance_path: String) -> String:
	var to := file_for(instance_path)
	if to == "" or not nameable(to):
		return ""
	if not _put(to, Model.of(world, id) + "
"):
		return ""
	files[instance_path + ".model.json"] = to
	return to

# A write of identical text moves only the timestamp, which is what a sync
# watcher, a build system and a version control diff go by. Compare first.
func _put(to: String, text: String) -> bool:
	if FileAccess.file_exists(to) and FileAccess.get_file_as_string(to) == text:
		return true
	DirAccess.make_dir_recursive_absolute(to.get_base_dir())
	var f := FileAccess.open(to, FileAccess.WRITE)
	if f == null:
		return false
	f.store_string(text)
	changed[to] = true
	return true

# A Rojo project tree node: $path mounts a directory, or a single file, at the
# node's place in the DataModel; $className / $properties / $attributes stand in
# as the instance's meta.json, and an init.meta.json found on disk wins.
func _mount(place_dir: String, node, inst: String, out: Dictionary) -> void:
	if typeof(node) != TYPE_DICTIONARY:
		return
	var is_file := false
	if node.has("$path") and typeof(node["$path"]) == TYPE_STRING:
		var disk: String = place_dir.path_join(node["$path"])
		if FileAccess.file_exists(disk):
			out[inst + _suffix(disk)] = disk
			is_file = true
		else:
			if inst != "":
				mounts[inst] = disk
			for rel in _walk(disk, ""):
				out[inst.path_join(rel) if inst != "" else rel] = disk.path_join(rel)
	if inst != "":
		var meta := _node_meta(node)
		if meta != "":
			var name := inst + ".meta.json" if is_file else inst.path_join("init.meta.json")
			if not out.has(name):
				out[name] = meta
	for k in node.keys():
		if not str(k).begins_with("$"):
			_mount(place_dir, node[k], inst.path_join(k) if inst != "" else k, out)

static func _node_meta(node: Dictionary) -> String:
	var meta := {}
	if node.has("$className"): meta["className"] = node["$className"]
	if node.has("$properties"): meta["properties"] = node["$properties"]
	if node.has("$attributes"): meta["attributes"] = node["$attributes"]
	return JSON.stringify(meta) if not meta.is_empty() else ""

static func _suffix(file: String) -> String:
	for s in [".server.luau", ".server.lua", ".client.luau", ".client.lua", ".luau", ".lua", ".model.json", ".meta.json", ".rbxmx", ".rbxm"]:
		if file.ends_with(s):
			return s
	return ""

func _walk(base: String, rel: String) -> Array:
	var out := []
	var d := DirAccess.open(base.path_join(rel) if rel != "" else base)
	if d == null:
		return out
	d.list_dir_begin()
	var n := d.get_next()
	while n != "":
		var r := rel.path_join(n) if rel != "" else n
		if d.current_is_dir():
			if not n.begins_with("."):
				out.append_array(_walk(base, r))
		elif n.ends_with(".luau") or n.ends_with(".lua") or n.ends_with(".model.json") or n.ends_with(".meta.json") or n.ends_with(".rbxmx") or n.ends_with(".rbxm"):
			out.append(r)
		n = d.get_next()
	return out
