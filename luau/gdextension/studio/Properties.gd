# The Properties panel: an instance's declared properties in Roblox's form (a
# Color3 as three 0..255 numbers). A write crosses to the runtime's thread and comes
# back through the mirror a frame or two on: a cell changes only when refresh() reads it.
class_name Properties
extends Tree

var world: PulseBlockzWorld
var history: History      # every write goes through it, so it can be undone
var id := 0               # the one whose properties are laid out: the last selected
var ids: Array = []       # everything selected; a write goes to all of them

var _rows := {}          # property name -> TreeItem
var _shape := ""         # the class whose rows are laid out, "" for none
var _editing := ""       # the row the user is typing in: leave its text alone
var filter := ""         # filters on the property NAME: a value changes under you
var _pop: PopupPanel     # the colour picker, built once
var _picker: ColorPicker
var _picking := ""       # the Color3 row the picker is open on
var _axis_rows := {}     # Vector3 property name -> [X row, Y row, Z row]
var _extras_shape := ""      # which attributes and tags the panel was laid out for
var _attr_head: TreeItem
var _tag_head: TreeItem
var _want_ref := ""      # a Ref property waiting to be pointed at something

signal ref_wanted(name: String)   # the Studio puts the pointer in pick-an-instance mode

const MIXED := "--"        # the selection does not agree on this one

var palette: Palette = Palette.of("Dark")
var DIM: Color:
	get: return palette.text_faint
var LIVE: Color:
	get: return palette.text
var BAD: Color:
	get: return palette.error

func _ready():
	columns = 2
	set_column_title(0, "Property")
	set_column_title(1, "Value")
	column_titles_visible = true
	hide_root = true
	item_edited.connect(_on_edited)
	custom_popup_edited.connect(_on_swatch)
	button_clicked.connect(_on_button)

	_picker = ColorPicker.new()
	_picker.color_changed.connect(_on_colour)
	_pop = PopupPanel.new()
	_pop.add_child(_picker)
	add_child(_pop)

func set_filter(text: String) -> void:
	filter = text.strip_edges().to_lower()
	_apply_filter()

func _apply_filter() -> void:
	for name in _rows:
		_rows[name].visible = filter == "" or String(name).to_lower().contains(filter)

func show_instance(instance_id: int) -> void:
	show_instances([instance_id] if instance_id != 0 else [])

func show_instances(selection: Array) -> void:
	ids = selection.duplicate()
	id = ids.back() if not ids.is_empty() else 0
	_shape = ""
	_editing = ""
	# Disarm, or the next pick in the Explorer writes a Ref for a row now gone
	_want_ref = ""
	refresh()

func refresh() -> void:
	if world == null:
		return
	var info := world.get_instance(id) if id != 0 else {}
	var cls: String = info.get("class_name", "")
	if cls == "" or id == 0:
		clear()
		_rows.clear()
		_shape = ""
		return
	var props := world.get_properties(id)
	# Attributes and tags vary per instance, so the layout turns on them too
	var extras := str(world.get_attributes(id).keys()) + str(world.get_tags(id))
	if cls != _shape or extras != _extras_shape:
		_extras_shape = extras
		_lay_out(cls, props)
	for p in props:
		var row: TreeItem = _rows.get(p.name)
		if row == null or p.name == _editing:
			continue
		if _axis_rows.has(p.name) and p.value is Vector3:
			var three: Array = _axis_rows[p.name]
			for a in 3:
				three[a].set_range(1, [p.value.x, p.value.y, p.value.z][a])
		var agreed := _agreed(p)
		_set_mode(row, p, not agreed)
		if agreed:
			_write(row, p)
		else:
			row.set_text(1, MIXED)
			row.set_custom_color(1, DIM)

# Rows in the class's declaration order, Instance's own first, as Studio shows them
func _lay_out(cls: String, props: Array) -> void:
	clear()
	_rows.clear()
	_shape = cls
	var root := create_item()
	_axis_rows.clear()
	var groups := {}
	for p in props:
		var under := _group_of(p.name)
		if not groups.has(under):
			var head := create_item(root)
			head.set_text(0, under)
			head.set_selectable(0, false)
			head.set_selectable(1, false)
			head.set_custom_color(0, palette.text_faint)
			groups[under] = head
		var row := create_item(groups[under])
		row.set_text(0, p.name)
		row.set_selectable(0, false)
		row.set_tooltip_text(0, "%s  (%s)" % [p.name, p.type])
		row.set_selectable(1, not p.read_only and _writable(p.type))
		_set_mode(row, p, false)
		_rows[p.name] = row
		# A Tree cell holds one value, so each axis gets its own child row to drag
		if p.type == "Vector3" and not p.read_only:
			var three := []
			for axis in ["X", "Y", "Z"]:
				var sub := create_item(row)
				sub.set_text(0, axis)
				sub.set_selectable(0, false)
				sub.set_custom_color(0, palette.text_faint)
				sub.set_cell_mode(1, TreeItem.CELL_MODE_RANGE)
				sub.set_range_config(1, -100000, 100000, 0.001)
				sub.set_editable(1, true)
				sub.set_metadata(0, p.name)
				three.append(sub)
			_axis_rows[p.name] = three
			row.collapsed = true
		# A Ref is pointed at, not typed
		if p.type == "Instance" and not p.read_only:
			row.add_button(1, Icons.icon("select", 14), 0, false, "Point at an instance")
			row.set_metadata(0, p.name)
		_write(row, p)
	_lay_out_extras()
	_apply_filter()

# Attributes and tags belong to the instance, not the class, and sit under the
# declared properties as in Studio. The metadata marks are the change log's.
const ADD_ROW := "  + add"

func _lay_out_extras() -> void:
	var root := get_root()
	if root == null or id == 0:
		return
	_attr_head = create_item(root)
	_attr_head.set_text(0, "Attributes")
	_attr_head.set_selectable(0, false)
	_attr_head.set_selectable(1, false)
	_attr_head.set_custom_color(0, palette.text_faint)
	for key in world.get_attributes(id):
		_extra_row(_attr_head, key, world.get_attributes(id)[key], true)
	_extra_adder(_attr_head, true)

	_tag_head = create_item(root)
	_tag_head.set_text(0, "Tags")
	_tag_head.set_selectable(0, false)
	_tag_head.set_selectable(1, false)
	_tag_head.set_custom_color(0, palette.text_faint)
	for tag in world.get_tags(id):
		var row := create_item(_tag_head)
		row.set_text(0, tag)
		row.set_selectable(0, false)
		row.set_cell_mode(1, TreeItem.CELL_MODE_CHECK)
		row.set_checked(1, true)
		row.set_editable(1, true)
		row.set_text(1, "on")
		row.set_metadata(0, "#" + tag)
	_extra_adder(_tag_head, false)

func _extra_row(head: TreeItem, key: String, value, editable: bool) -> TreeItem:
	var row := create_item(head)
	row.set_text(0, key)
	row.set_selectable(0, false)
	row.set_cell_mode(1, TreeItem.CELL_MODE_STRING)
	row.set_editable(1, editable)
	row.set_text(1, _as_text(value))
	row.set_tooltip_text(0, "%s  (attribute)" % key)
	row.set_metadata(0, "@" + key)
	return row

func _extra_adder(head: TreeItem, attribute: bool) -> void:
	var row := create_item(head)
	row.set_text(0, ADD_ROW)
	row.set_custom_color(0, palette.text_faint)
	row.set_selectable(0, false)
	row.set_cell_mode(1, TreeItem.CELL_MODE_STRING)
	row.set_editable(1, true)
	row.set_text(1, "")
	row.set_tooltip_text(1, "Type a name to add one" if attribute else "Type a tag to add it")
	row.set_metadata(0, "+@" if attribute else "+#")

static func _as_text(v) -> String:
	if v is bool:
		return "true" if v else "false"
	if v is Vector3:
		return "%s, %s, %s" % [v.x, v.y, v.z]
	if v is Color:
		return "%d, %d, %d" % [roundi(v.r * 255), roundi(v.g * 255), roundi(v.b * 255)]
	return str(v)

# `like` is the value being replaced, null for a new one
static func _as_value(text: String, like):
	var t := text.strip_edges()
	if like is bool or t == "true" or t == "false":
		return t == "true"
	if like is Vector3 or (like == null and t.count(",") == 2):
		var bits := t.split(",")
		if bits.size() == 3 and bits[0].strip_edges().is_valid_float():
			return Vector3(float(bits[0]), float(bits[1]), float(bits[2]))
	if like is float or like is int or (like == null and t.is_valid_float()):
		return float(t)
	return t

# The runtime reports one flat list, so Roblox's grouping lives here, by name
const GROUPS := {
	"Appearance": ["Color", "Material", "Transparency", "Reflectance", "CastShadow", "BrickColor", "TextColor3",
				   "BackgroundColor3", "Image", "Texture", "Brightness", "Range", "Enabled"],
	"Data": ["Name", "ClassName", "Position", "Orientation", "Size", "CFrame", "Value", "Source", "Text"],
	"Behaviour": ["Anchored", "CanCollide", "CanTouch", "CanQuery", "Locked", "Massless", "Archivable",
				  "Disabled", "Sit", "Health", "MaxHealth", "WalkSpeed", "JumpPower"],
}

static func _group_of(name: String) -> String:
	for group in GROUPS:
		if GROUPS[group].has(name):
			return group
	return "Part"

# Mixed falls back to a text cell, and leaves boolean and EnumItem read-only: a
# half-ticked box reads as "off" and a dropdown cannot sit between two items.
func _set_mode(row: TreeItem, p: Dictionary, mixed: bool) -> void:
	var editable: bool = not p.read_only and _writable(p.type)
	if mixed:
		row.set_cell_mode(1, TreeItem.CELL_MODE_STRING)
		row.set_editable(1, editable and p.type != "boolean" and p.type != "EnumItem")
		return
	if p.type == "Color3" and editable:
		row.set_cell_mode(1, TreeItem.CELL_MODE_CUSTOM)
		row.set_selectable(1, false)
		row.set_text(1, "")            # the Tree paints the text over the swatch
		row.set_custom_draw_callback(1, _draw_colour)
	elif p.type == "boolean":
		row.set_cell_mode(1, TreeItem.CELL_MODE_CHECK)
	elif p.type == "EnumItem" and editable:
		row.set_cell_mode(1, TreeItem.CELL_MODE_RANGE)
		row.set_text(1, ",".join(p.enum_items))
	else:
		row.set_cell_mode(1, TreeItem.CELL_MODE_STRING)
	row.set_editable(1, editable)

func _write(row: TreeItem, p: Dictionary) -> void:
	if p.type == "Color3" and row.get_cell_mode(1) == TreeItem.CELL_MODE_CUSTOM:
		row.set_metadata(1, p.value)               # what the callback paints
		row.set_tooltip_text(1, _format(p))
	elif p.type == "boolean":
		row.set_checked(1, bool(p.value))
	elif p.type == "EnumItem" and row.get_cell_mode(1) == TreeItem.CELL_MODE_RANGE:
		var at: int = p.enum_items.find(str(p.value))
		row.set_range(1, max(at, 0))
	else:
		row.set_text(1, _format(p))
	row.set_custom_color(1, DIM if p.is_default else LIVE)

# Runs from inside the Tree's own _draw, so draw_rect lands on this node
func _draw_colour(item: TreeItem, rect: Rect2) -> void:
	var c = item.get_metadata(1)
	if not (c is Color):
		return
	var swatch := Rect2(rect.position + Vector2(2, 3), Vector2(maxf(rect.size.x - 18, 8), maxf(rect.size.y - 7, 4)))
	draw_rect(swatch, c)
	draw_rect(swatch, palette.border, false, 1.0)

func _on_swatch() -> void:
	var row := get_edited()
	if row == null:
		return
	_picking = row.get_text(0)
	var value = row.get_metadata(1)
	if value is Color:
		_picker.color = value
	_pop.popup(get_custom_popup_rect())

func _on_colour(c: Color) -> void:
	if _picking != "":
		write(_picking, c)

func _on_button(item: TreeItem, _column: int, _id: int, _mouse: int) -> void:
	var name = item.get_metadata(0)
	if name is String and name != "":
		_want_ref = name
		ref_wanted.emit(name)

# False when no Ref row is armed, so the Studio can treat the pick as a selection
func pointed_at(id: int) -> bool:
	if _want_ref == "":
		return false
	var name := _want_ref
	_want_ref = ""
	write(name, id)
	return true

func cancel_ref() -> void:
	_want_ref = ""

# In an adder row ("+@", "+#") the typed text is the name, not the value
func _write_extra(row: TreeItem, mark: String) -> void:
	var typed := row.get_text(1).strip_edges()
	if mark == "+@":
		row.set_text(1, "")
		if typed == "":
			return
		for one in ids:
			history.set_attribute(one, typed, "")
	elif mark == "+#":
		row.set_text(1, "")
		if typed == "":
			return
		for one in ids:
			history.set_tag(one, typed, true)
	elif mark.begins_with("@"):
		var key := mark.substr(1)
		var was = world.get_attributes(id).get(key, null)
		for one in ids:
			history.set_attribute(one, key, _as_value(typed, was))
	else:
		var tag := mark.substr(1)
		var on := row.is_checked(1)
		for one in ids:
			history.set_tag(one, tag, on)
	_shape = ""          # the attributes or tags may have changed: lay out again
	_extras_shape = ""
	refresh()

# `_rows` holds declared properties only, so these are found by their mark
func _rows_for_attr(key: String) -> TreeItem:
	return _row_marked(_attr_head, "@" + key)

func _rows_for_tag(tag: String) -> TreeItem:
	return _row_marked(_tag_head, "#" + tag)

func _row_marked(head: TreeItem, mark: String) -> TreeItem:
	if head == null:
		return null
	for i in head.get_child_count():
		var row := head.get_child(i)
		if row.get_metadata(0) == mark:
			return row
	return null

func waiting_for_ref() -> bool:
	return _want_ref != ""

func _on_edited() -> void:
	_on_edited_for(get_edited())

func _on_edited_for(row: TreeItem) -> void:
	if row == null or world == null:
		return
	var name := row.get_text(0)
	# An axis child: the property is written as a whole vector, from all three rows
	var owner = row.get_metadata(0)
	if owner is String and _axis_rows.has(owner):
		var three: Array = _axis_rows[owner]
		var made := Vector3(three[0].get_range(1), three[1].get_range(1), three[2].get_range(1))
		_editing = owner
		write(owner, made)
		_editing = ""
		return
	# An attribute or a tag: no class declares it, so the row carries its own mark
	var mark = row.get_metadata(0)
	if mark is String and (mark.begins_with("@") or mark.begins_with("#") or mark.begins_with("+")):
		_write_extra(row, mark)
		return
	var p := _prop(name)
	if p.is_empty():
		return
	_editing = name
	var value = _parse(p, row)
	var ok: bool = value != null and write(name, value)
	row.set_custom_color(1, LIVE if ok else BAD)
	if not ok:
		_write(row, p)          # put the old value back
	_editing = ""

# One property onto everything selected that has it, as a single undo step
func write(name: String, value) -> bool:
	var ok := false
	history.begin("%s of %d" % [name, ids.size()] if ids.size() > 1 else name)
	for one in ids:
		if not _prop_of(one, name).is_empty():
			ok = history.set_property(one, name, value) or ok
	history.end()
	return ok

func _agreed(p: Dictionary) -> bool:
	for one in ids:
		if one == id:
			continue
		var theirs := _prop_of(one, p.name)
		if theirs.is_empty() or theirs.value != p.value:
			return false
	return true

func _prop_of(of_id: int, name: String) -> Dictionary:
	for p in world.get_properties(of_id):
		if p.name == name:
			return p
	return {}

func _prop(name: String) -> Dictionary:
	for p in world.get_properties(id):
		if p.name == name:
			return p
	return {}

# ---- values, as Roblox writes them ------------------------------------------------
static func _writable(type: String) -> bool:
	return type in ["boolean", "number", "string", "Vector3", "Vector2", "Color3", "EnumItem", "UDim", "UDim2", "NumberRange", "Content"]

func _format(p: Dictionary) -> String:
	var v = p.value
	if v == null:
		return ""
	match p.type:
		"number": return _num(v)
		"string": return str(v)
		"Vector3": return "%s, %s, %s" % [_num(v.x), _num(v.y), _num(v.z)]
		"Vector2", "UDim", "NumberRange": return "%s, %s" % [_num(v.x), _num(v.y)]
		"UDim2": return "%s, %s, %s, %s" % [_num(v.x), _num(v.y), _num(v.z), _num(v.w)]
		"Color3": return "%d, %d, %d" % [roundi(v.r * 255), roundi(v.g * 255), roundi(v.b * 255)]
		"EnumItem": return str(v)
		"Instance":
			if int(v) == 0:
				return ""
			var to := world.get_instance(int(v))
			return to.get("name", "(gone)")
		"NumberSequence", "ColorSequence", "PhysicalProperties":
			var numbers := PackedStringArray()
			for f in v:
				numbers.append(_num(f))
			return ", ".join(numbers)
		"Font":
			return "%s %d%s" % [v.get("family", ""), int(v.get("weight", 400)), " italic" if v.get("italic", false) else ""]
		"Content":
			# Content source 2 is an Object; anything else carries a uri
			if int(v.get("source", 0)) == 2:
				var obj := world.get_instance(int(v.get("object", 0)))
				return "(%s)" % obj.get("class_name", "object")
			return str(v.get("uri", ""))
	return str(v)

# No trailing zeros, no exponent: GDScript's % has no %g
static func _num(v: float) -> String:
	var s := String.num(v, 4)
	if s.contains("."):
		s = s.rstrip("0").rstrip(".")
	return "0" if s == "" or s == "-" else s

# The cell text as a value of the property's type, null if it does not parse
func _parse(p: Dictionary, row: TreeItem):
	if p.type == "boolean":
		return row.is_checked(1)
	if p.type == "EnumItem":
		var items: PackedStringArray = p.enum_items
		var at := int(row.get_range(1))
		return items[at] if at >= 0 and at < items.size() else null
	var text := row.get_text(1)
	if p.type == "string" or p.type == "Content":
		return text
	var n := _numbers(text)
	match p.type:
		"number": return n[0] if n.size() == 1 else null
		"Vector3": return Vector3(n[0], n[1], n[2]) if n.size() == 3 else null
		"Vector2", "UDim", "NumberRange": return Vector2(n[0], n[1]) if n.size() == 2 else null
		"UDim2": return Vector4(n[0], n[1], n[2], n[3]) if n.size() == 4 else null
		"Color3": return Color(n[0] / 255.0, n[1] / 255.0, n[2] / 255.0) if n.size() == 3 else null
	return null

static func _numbers(text: String) -> Array:
	var out := []
	for piece in text.split(",", false):
		var t := piece.strip_edges()
		if not t.is_valid_float():
			return []
		out.append(t.to_float())
	return out
