# Undo and redo for every Studio edit: the old value is read from the world
# mirror before the write. Entries are property, attribute, tag, parent, exists
# (subtree as *.model.json) and batch, each runnable in either direction.
class_name History
extends RefCounted

signal changed          # an entry was recorded, undone or redone

const DEPTH := 200

var world: PulseBlockzWorld
var recording := true   # off during play: that world is a copy

var _done: Array = []   # oldest first; undo takes the back
var _undone: Array = []
var _batch: Array = []
var _batch_label := ""
var _busy := false      # an apply's own writes are not recorded

func can_undo() -> bool: return not _busy and not _done.is_empty()
func can_redo() -> bool: return not _busy and not _undone.is_empty()
func undo_label() -> String: return _done.back().label if not _done.is_empty() else ""
func redo_label() -> String: return _undone.back().label if not _undone.is_empty() else ""

func clear() -> void:
	_done.clear()
	_undone.clear()
	_batch.clear()
	changed.emit()

# ---- the edits ---------------------------------------------------------------------
func set_property(id: int, name: String, value) -> bool:
	var before = _value_of(id, name)
	if not world.set_property(id, name, value):
		return false
	_record({"kind": "property", "id": id, "name": name, "before": before, "after": value,
			 "label": "%s of %s" % [name, _name_of(id)]})
	return true

# No class declares attributes or tags: the world keeps them per instance, by name,
# beside the properties. Neither can be folded into set_property/_value_of.
func set_attribute(id: int, name: String, value) -> bool:
	var before = world.get_attributes(id).get(name, null)
	if not world.set_attribute(id, name, value):
		return false
	_record({"kind": "attribute", "id": id, "name": name, "before": before, "after": value,
			 "label": "@%s of %s" % [name, _name_of(id)]})
	return true

func set_tag(id: int, tag: String, on: bool) -> bool:
	var before: bool = world.get_tags(id).has(tag)
	if before == on or not world.set_tag(id, tag, on):
		return false
	_record({"kind": "tag", "id": id, "name": tag, "before": before, "after": on,
			 "label": "#%s of %s" % [tag, _name_of(id)]})
	return true

func set_parent(id: int, parent: int) -> bool:
	var before: int = world.get_instance(id).get("parent", 0)
	if not world.set_parent(id, parent):
		return false
	_record({"kind": "parent", "id": id, "before": before, "after": parent,
			 "label": "move %s" % _name_of(id)})
	return true

func insert(class_name_: String, parent: int) -> int:
	var was := _children(parent)
	if not world.create_instance(class_name_, parent):
		return 0
	var id := await _appeared(parent, was)
	if id == 0:
		return 0
	_record({"kind": "exists", "present": true, "parent": parent, "parent_path": _path_of(parent),
			 "name": world.get_instance(id).get("name", ""),
			 "id": id, "model": "", "label": "insert %s" % class_name_})
	return id

# `model` is *.model.json text, as Model.of writes it: paste and an undone delete.
func add(parent: int, name: String, model: String, label := "") -> int:
	var entry := {"kind": "exists", "present": true, "parent": parent, "parent_path": _path_of(parent),
				  "name": name,
				  "id": 0, "model": model, "label": label if label != "" else "add %s" % name}
	await _restore(entry)
	if entry.id == 0:
		return 0
	_record(entry)
	return entry.id

# Records an instance the caller already created -- an import, where the loader
# makes it -- without creating one here.
func record_added(id: int, parent: int, name: String) -> void:
	if id == 0:
		return
	_record({"kind": "exists", "present": true, "parent": parent, "parent_path": _path_of(parent),
			 "name": name, "id": id, "model": "", "label": "insert %s" % name})

func destroy(id: int) -> bool:
	if id == 0 or world.get_instance(id).is_empty():
		return false
	var entry := {"kind": "exists", "present": false, "parent": world.get_instance(id).get("parent", 0),
				  "parent_path": _path_of(world.get_instance(id).get("parent", 0)),
				  "name": world.get_instance(id).get("name", ""), "id": id,
				  "model": Model.of(world, id), "label": "delete %s" % _name_of(id)}
	world.destroy_instance(id)
	_record(entry)
	return true

# ---- batching ---------------------------------------------------------------------
func begin(label: String) -> void:
	if _batch_label != "":
		return          # nested: the outer batch wins
	_batch_label = label
	_batch = []

func end() -> void:
	if _batch_label == "":
		return
	var items := _batch
	var label := _batch_label
	_batch = []
	_batch_label = ""
	if items.is_empty():
		return
	if items.size() == 1:
		_push(items[0])
	else:
		_push({"kind": "batch", "items": items, "label": label})

# ---- undo and redo ------------------------------------------------------------------
func undo() -> void:
	if not can_undo():
		return
	var entry = _done.pop_back()
	await _apply(entry, false)
	_undone.push_back(entry)
	changed.emit()

func redo() -> void:
	if not can_redo():
		return
	var entry = _undone.pop_back()
	await _apply(entry, true)
	_done.push_back(entry)
	changed.emit()

func _apply(entry: Dictionary, forward: bool) -> void:
	_busy = true
	await _run(entry, forward)
	_busy = false

func _run(entry: Dictionary, forward: bool) -> void:
	match entry.kind:
		"batch":
			var items: Array = entry.items
			for i in items.size():
				await _run(items[i if forward else items.size() - 1 - i], forward)
		"property":
			world.set_property(entry.id, entry.name, entry.after if forward else entry.before)
		"attribute":
			var want = entry.after if forward else entry.before
			# The mirror cannot remove an attribute: one that was absent comes
			# back as "" rather than vanishing.
			world.set_attribute(entry.id, entry.name, want if want != null else "")
		"tag":
			world.set_tag(entry.id, entry.name, entry.after if forward else entry.before)
		"parent":
			var to: int = entry.after if forward else entry.before
			world.set_parent(entry.id, to)
			# A reparent is queued: the next item in the batch would otherwise
			# read the tree before the move lands.
			await _settled(entry.id, to)
		"exists":
			if entry.present == forward:
				await _restore(entry)
			else:
				# Id 0 is the DataModel: Model.of would take the whole place and
				# destroy does nothing. A skipped restore leaves id 0 too.
				if entry.id == 0 or world.get_instance(entry.id).is_empty():
					entry.id = 0
					return
				entry.model = Model.of(world, entry.id)   # the opposite direction restores from it
				world.destroy_instance(entry.id)
				await _settled(entry.id, -1)
				entry.id = 0

# Waits up to 30 frames for a queued write to show in the mirror; one that never
# lands must not wedge the undo. Parent -1 waits for the instance to go instead.
func _settled(id: int, parent: int) -> void:
	for wait in 30:
		await Engine.get_main_loop().process_frame
		var info := world.get_instance(id)
		if parent < 0:
			if info.is_empty():
				return
		elif info.is_empty() or info.get("parent", -1) == parent:
			return

func _restore(entry: Dictionary) -> void:
	# Ids are never reused: a parent deleted and put back is a new id at the same
	# path, so a dead one is re-found by path. Nothing at that path means the
	# parent's own model already carries this subtree -- restoring would duplicate
	# it, at the root, since a dead id has no path.
	var parent: int = entry.parent
	if parent != 0 and world.get_instance(parent).is_empty():
		parent = _id_at(entry.get("parent_path", ""))
		if parent == 0:
			entry.id = 0
			return
		entry.parent = parent
	var was := _children(parent)
	world.add_model(_path_of(parent), entry.name, entry.model)
	entry.id = await _appeared(parent, was)

# The id a name path wears now; 0 if no instance is at it.
func _id_at(path: String) -> int:
	if path == "":
		return 0
	var at := 0
	for step in path.split("/"):
		var found := 0
		for c in world.get_child_ids(at):
			if world.get_instance(c).get("name", "") == step:
				found = c
				break
		if found == 0:
			return 0
		at = found
	return at

# ---- odds and ends ------------------------------------------------------------------
func _record(entry: Dictionary) -> void:
	if _busy or not recording:
		return
	if _batch_label != "":
		# A drag writes the same property over and over: fold it into the entry
		# already held, which keeps the first "before".
		if entry.kind == "property":
			for held in _batch:
				if held.kind == "property" and held.id == entry.id and held.name == entry.name:
					held.after = entry.after
					return
		_batch.append(entry)
		return
	_push(entry)

func _push(entry: Dictionary) -> void:
	_done.push_back(entry)
	if _done.size() > DEPTH:
		_done.pop_front()
	_undone.clear()          # a new edit drops the redo stack
	changed.emit()

func _value_of(id: int, name: String):
	for p in world.get_properties(id, true):
		if p.name == name:
			return p.value
	return null

func _name_of(id: int) -> String:
	return world.get_instance(id).get("name", "?")

func _children(parent: int) -> Dictionary:
	var out := {}
	for c in world.get_child_ids(parent):
		out[c] = true
	return out

# The new child of `parent`: the runtime lands a create a frame or two later.
func _appeared(parent: int, before: Dictionary) -> int:
	for wait in 30:
		await Engine.get_main_loop().process_frame
		for c in world.get_child_ids(parent):
			if not before.has(c):
				return c
	return 0

# Path as add_model wants it: "Workspace/Map", "" for the root.
func _path_of(id: int) -> String:
	if id == 0:
		return ""
	var back := []
	var at := id
	for hop in 32:
		var info := world.get_instance(at)
		if info.is_empty():
			break
		back.append(info.name)
		at = info.parent
		if at == 0:
			break
	back.reverse()
	return "/".join(back)
