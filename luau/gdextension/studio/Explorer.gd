# The Explorer: the world's Instance tree, read from the world's own mirror
# (get_child_ids / get_instance) so it never touches a script thread. Only what is
# unfolded is built, and a change in get_tree_version() is what triggers a rebuild.
class_name Explorer
extends Tree

signal picked(ids: Array)
signal name_typed(id: int, to: String)
signal menu_wanted(at: Vector2)
signal opened(id: int)          # double-clicked
signal dropped(ids: Array, onto: int)   # dragged onto another row: reparent

var world: PulseBlockzWorld
var palette: Palette

var _version := -1
var _open := {0: true}     # unfolded instance ids; game (0) starts open
var _items := {}           # instance id -> TreeItem
var _selected: Array = []  # instance ids, in the order they were picked
var _quiet := false        # rebuilding: a select() from here is not the user's
var _renaming := 0         # the row being typed over
var filter := ""           # lower-cased, from the box over the tree

func _ready():
	columns = 2
	set_column_title(0, "Name")
	set_column_title(1, "Class")
	set_column_expand(1, false)
	set_column_custom_minimum_width(1, 96)
	column_titles_visible = true
	hide_root = false
	scroll_horizontal_enabled = false
	select_mode = Tree.SELECT_MULTI      # ctrl adds one, shift a run
	drop_mode_flags = Tree.DROP_MODE_ON_ITEM   # onto a row, never between: the runtime keeps no sibling order
	item_selected.connect(_on_selected)
	multi_selected.connect(func(_item, _column, _on): _on_selected())
	item_collapsed.connect(_on_collapsed)
	item_edited.connect(_on_edited)
	item_activated.connect(func():
		var item := get_selected()
		if item != null and item.get_metadata(0) != null:
			opened.emit(item.get_metadata(0)))
	item_mouse_selected.connect(func(at, button):
		if button == MOUSE_BUTTON_RIGHT:
			menu_wanted.emit(get_screen_position() + at))

func _process(_delta):
	if world == null:
		return
	if world.get_tree_version() != _version:
		_version = world.get_tree_version()
		rebuild()

# Select instances wherever they sit, unfolding their ancestors.
func reveal(selection: Array) -> void:
	_selected = selection.duplicate()
	for id in selection:
		_open_down_to(id)
	_open[0] = true
	rebuild()

func _open_down_to(id: int) -> void:
	var up: int = id
	for hop in 16:
		var info := world.get_instance(up)
		if info.is_empty() or info.parent == 0:
			break
		up = info.parent
		_open[up] = true

# F2 on a row: edit the name in place, as Studio does.
func rename(id: int) -> void:
	if not _items.has(id):
		return
	_renaming = id
	var row: TreeItem = _items[id]
	row.set_editable(0, true)
	row.set_text(0, world.get_instance(id).get("name", ""))
	set_selected(row, 0)
	edit_selected(true)

func _on_edited() -> void:
	var row := get_edited()
	if row == null or _renaming == 0:
		return
	var id := _renaming
	_renaming = 0
	row.set_editable(0, false)
	name_typed.emit(id, row.get_text(0))

# ---- dragging a row onto another --------------------------------------------------
func _get_drag_data(at: Vector2):
	var item := get_item_at_position(at)
	if item == null or item.get_metadata(0) == null or item.get_metadata(0) == 0:
		return null
	var moving := _selected.duplicate()
	var id = item.get_metadata(0)
	if not moving.has(id):
		moving = [id]
	var ghost := Label.new()
	ghost.text = item.get_text(0) if moving.size() == 1 else "%d instances" % moving.size()
	set_drag_preview(ghost)
	return {"studio_instances": moving}

func _can_drop_data(at: Vector2, data) -> bool:
	if typeof(data) != TYPE_DICTIONARY or not data.has("studio_instances"):
		return false
	var onto := get_item_at_position(at)
	if onto == null or onto.get_metadata(0) == null:
		return false
	var target = onto.get_metadata(0)
	for id in data.studio_instances:
		if id == target or _is_under(target, id):
			return false          # nothing may be put inside itself
	return true

func _drop_data(at: Vector2, data) -> void:
	var onto := get_item_at_position(at)
	if onto == null or onto.get_metadata(0) == null:
		return
	dropped.emit(data.studio_instances, onto.get_metadata(0))

# True when `maybe` sits under `top`. More than 64 parents up and it gives false.
func _is_under(maybe: int, top: int) -> bool:
	var at := maybe
	for hop in 64:
		var info := world.get_instance(at)
		if info.is_empty():
			return false
		at = info.parent
		if at == top:
			return true
		if at == 0:
			return false
	return false

func set_filter(text: String) -> void:
	var want := text.strip_edges().to_lower()
	# One letter matches most of a place, so filtering starts at two.
	filter = want if want.length() >= 2 else ""
	_version = -1
	rebuild()

# A row survives if it matches or anything under it does, so a hit brings its
# ancestors with it. Hiding a row hides its whole subtree, so every child is
# visited before `visible` is set here.
func _keep(item: TreeItem) -> bool:
	var hit: bool = item.get_text(0).to_lower().contains(filter) or item.get_text(1).to_lower().contains(filter)
	var kid := item.get_first_child()
	while kid != null:
		if _keep(kid):
			hit = true
		kid = kid.get_next()
	item.visible = hit
	return hit

func rebuild() -> void:
	_quiet = true
	clear()
	_items.clear()
	var root := create_item()
	root.set_text(0, "game")
	root.set_text(1, "DataModel")
	root.set_metadata(0, 0)
	root.collapsed = not _open.has(0)
	_items[0] = root
	_fill(root, 0)
	if filter != "":
		_keep(root)
		root.visible = true          # keep game as an anchor when nothing matches
	deselect_all()
	for id in _selected:
		if _items.has(id):
			_items[id].select(0)
	if not _selected.is_empty() and _items.has(_selected.back()):
		scroll_to_item(_items[_selected.back()])
	_quiet = false

func _fill(item: TreeItem, id: int) -> void:
	var kids := _sorted(id)
	for cid in kids:
		var info := world.get_instance(cid)
		if info.is_empty():
			continue
		var ti := create_item(item)
		ti.set_text(0, info.name)
		ti.set_text(1, info.class_name)
		if palette != null:
			ti.set_custom_color(1, palette.text_faint)
		# Shape is the primary read and the tint only secondary, as in Studio's Explorer
		var shape := Icons.for_class(info.class_name)
		var art := Icons.icon(shape, 16)
		if art != null:
			ti.set_icon(0, art)
			ti.set_icon_max_width(0, 16)
			ti.set_icon_modulate(0, Icons.tint_for(shape))
		ti.set_metadata(0, cid)
		_items[cid] = ti
		var open: bool = _open.has(cid) or filter != ""
		ti.collapsed = not open
		if open:
			_fill(ti, cid)
		elif world.get_child_ids(cid).size() > 0:
			create_item(ti).set_text(0, "...")   # stand-in, so the fold arrow appears

# Insertion order, the order Roblox's own Explorer shows -- never alphabetical.
func _sorted(id: int) -> Array:
	var kids := []
	for cid in world.get_child_ids(id):
		kids.append(cid)
	return kids

# The Tree owns which rows are lit. The whole set is read back in the order
# already held, so the last row picked stays last.
func _on_selected():
	if _quiet:
		return
	var now := []
	var item := get_next_selected(null)
	while item != null:
		var id = item.get_metadata(0)
		if id != null and id != 0:
			now.append(id)
		item = get_next_selected(item)
	var ordered := []
	for id in _selected:
		if now.has(id):
			ordered.append(id)
	for id in now:
		if not ordered.has(id):
			ordered.append(id)
	_selected = ordered
	picked.emit(_selected)

func _on_collapsed(item: TreeItem):
	if _quiet:
		return
	var id = item.get_metadata(0)
	if id == null:
		return
	if item.collapsed:
		_open.erase(id)
	else:
		_open[id] = true
		_version = -1        # rebuild next frame, with this branch filled in
