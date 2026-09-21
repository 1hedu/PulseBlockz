# The Debugger, shown under the view while a played place stands at a breakpoint:
# call stack left, stopped frame first, the picked frame's locals right. The button
# set and its keys are Roblox Studio's; the Studio wires them and opens the script.
class_name DebuggerPane
extends PanelContainer

signal continue_pressed
signal step_pressed(kind: int)          # 1 into, 2 over, 3 out
signal frame_picked(script: String, line: int)

var at_script := ""
var line := 0
var frames: Array = []
var _where: Label
var _stack: ItemList
var _locals: Tree
var _buttons: Array = []

func _init() -> void:
	name = "Debugger"
	custom_minimum_size = Vector2(0, 200)
	var box := VBoxContainer.new()
	add_child(box)
	var bar := HBoxContainer.new()
	bar.add_theme_constant_override("separation", 6)
	box.add_child(bar)
	var title := Label.new()
	title.text = "Debugger"
	bar.add_child(title)
	_buttons.append(_button(bar, "Continue  F5", func(): continue_pressed.emit()))
	_buttons.append(_button(bar, "Step Over  F10", func(): step_pressed.emit(2)))
	_buttons.append(_button(bar, "Step Into  F11", func(): step_pressed.emit(1)))
	_buttons.append(_button(bar, "Step Out  Shift+F11", func(): step_pressed.emit(3)))
	_where = Label.new()
	_where.text = "running"
	_where.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.add_child(_where)
	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(split)
	_stack = ItemList.new()
	_stack.custom_minimum_size = Vector2(320, 0)
	_stack.item_selected.connect(_on_frame)
	split.add_child(_stack)
	_locals = Tree.new()
	_locals.columns = 2
	_locals.set_column_titles_visible(true)
	_locals.set_column_title(0, "Name")
	_locals.set_column_title(1, "Value")
	_locals.hide_root = true
	_locals.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.add_child(_locals)
	visible = false
	_enable(false)

func _button(into: Control, text: String, action: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(action)
	into.add_child(b)
	return b

func _enable(on: bool) -> void:
	for b in _buttons:
		b.disabled = not on

func paused() -> bool:
	return line > 0

func show_pause(where: String, at_line: int, at_frames: Array) -> void:
	at_script = where
	line = at_line
	frames = at_frames.duplicate()
	_where.text = "paused at %s:%d" % [at_script, line]
	_stack.clear()
	for f in frames:
		_stack.add_item("%s  --  %s:%s" % [f.get("function", "?"), f.get("script", ""), f.get("line", 0)])
	_enable(true)
	visible = true
	if not frames.is_empty():
		_stack.select(0)
		_show_locals(0)

func clear() -> void:
	at_script = ""
	line = 0
	frames.clear()
	_where.text = "running"
	_stack.clear()
	_locals.clear()
	_enable(false)

func _on_frame(i: int) -> void:
	_show_locals(i)
	if i >= 0 and i < frames.size():
		frame_picked.emit(str(frames[i].get("script", "")), int(frames[i].get("line", 0)))

func _show_locals(i: int) -> void:
	_locals.clear()
	if i < 0 or i >= frames.size():
		return
	var root := _locals.create_item()
	var locals: Dictionary = frames[i].get("locals", {})
	var names := locals.keys()
	names.sort()
	for n in names:
		var row := _locals.create_item(root)
		row.set_text(0, str(n))
		row.set_text(1, str(locals[n]))

func local_value(name_: String, frame := 0) -> String:
	if frame < 0 or frame >= frames.size():
		return ""
	return str(frames[frame].get("locals", {}).get(name_, ""))
