# The script editor: tabs of open Scripts over a CodeEdit. commit() hands a buffer back as
# one history entry, not one per keystroke. The buffer is read from Source when a tab opens
# and never re-read: refreshing it from the change log would drag the caret of whoever types.
class_name ScriptPane
extends PanelContainer

signal dirtied                       # the unsaved state changed, either way
signal closed                        # the last tab went
signal breakpoints_changed(id: int, lines: Array)   # lines count from one
signal shown(id: int)                # opened, or its tab picked

const SCRIPT_CLASSES := ["Script", "LocalScript", "ModuleScript"]

var world: PulseBlockzWorld
var history: History
var palette: Palette = Palette.of("Dark")

var _tabs: TabBar
var _code: CodeEdit
var _open: Array = []                # [{id, text, dirty}], in tab order
var _at := -1                        # index in _open of the showing tab, -1 for none

# One bar for find and replace, the replace half hidden until Ctrl+H. The search
# is the CodeEdit's own, so it covers the showing tab only, not the other buffers.
var _find_bar: HBoxContainer
var _find_box: LineEdit
var _with_box: LineEdit
var _find_says: Label
var _find_case: Button
var _replace_bits: Array = []

static func is_script(of_world: PulseBlockzWorld, id: int) -> bool:
	return of_world != null and of_world.get_instance(id).get("class_name", "") in SCRIPT_CLASSES

func _ready():
	repaint()
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", 0)
	add_child(box)

	_tabs = TabBar.new()
	_tabs.clip_tabs = false
	_tabs.tab_close_display_policy = TabBar.CLOSE_BUTTON_SHOW_ALWAYS
	_tabs.tab_changed.connect(_on_tab)
	_tabs.tab_close_pressed.connect(close_at)
	box.add_child(_tabs)

	_code = CodeEdit.new()
	_code.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_code.gutters_draw_line_numbers = true
	_code.gutters_draw_breakpoints_gutter = true
	_code.gutters_draw_executing_lines = true
	_code.breakpoint_toggled.connect(_on_breakpoint)
	_code.draw_tabs = true
	_code.indent_use_spaces = false
	_code.auto_brace_completion_enabled = true

	# CodeEdit has no font property: a monospace face is a theme override.
	var face := SystemFont.new()
	face.font_names = PackedStringArray(["Consolas", "Cascadia Mono", "DejaVu Sans Mono", "Courier New", "monospace"])
	_code.add_theme_font_override("font", face)
	_code.add_theme_font_size_override("font_size", 13)
	_code.text_changed.connect(_on_typed)
	_code.code_completion_enabled = true
	_code.code_completion_prefixes = PackedStringArray([".", ":"])
	_code.code_completion_requested.connect(_offer_completions)
	_code.focus_exited.connect(func(): commit())
	# The box's own string and comment spans, separate from the highlighter's
	# colour regions: without them, indenting and word selection read the wrong text.
	_code.add_string_delimiter("\"", "\"", true)
	_code.add_string_delimiter("'", "'", true)
	_code.add_comment_delimiter("--[[", "]]", false)
	_code.add_comment_delimiter("--", "", true)
	_build_find(box)
	box.add_child(_code)
	box.move_child(_find_bar, 1)     # under the tabs, above the text
	visible = false
	repaint()

func _build_find(box: VBoxContainer) -> void:
	_find_bar = HBoxContainer.new()
	_find_bar.visible = false
	_find_box = LineEdit.new()
	_find_box.placeholder_text = "Find"
	_find_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_find_box.text_changed.connect(func(_t): _retally(); _step(0))
	_find_box.text_submitted.connect(func(_t): _step(1))
	_find_bar.add_child(_find_box)
	_find_says = Label.new()
	_find_says.custom_minimum_size = Vector2(78, 0)
	_find_says.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_find_bar.add_child(_find_says)
	_find_case = Button.new()
	_find_case.text = "Aa"
	_find_case.toggle_mode = true
	_find_case.tooltip_text = "Match case"
	_find_case.toggled.connect(func(_on): _retally(); _step(0))
	_find_bar.add_child(_find_case)
	for one in [["<", -1], [">", 1]]:
		var b := Button.new()
		b.text = one[0]
		b.tooltip_text = "Previous match  Shift+Enter" if one[1] < 0 else "Next match  Enter"
		b.pressed.connect(func(): _step(one[1]))
		_find_bar.add_child(b)
	_with_box = LineEdit.new()
	_with_box.placeholder_text = "Replace with"
	_with_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_with_box.text_submitted.connect(func(_t): replace_one())
	_find_bar.add_child(_with_box)
	_replace_bits.append(_with_box)
	for one in [["Replace", func(): replace_one()], ["All", func(): replace_all()]]:
		var b := Button.new()
		b.text = one[0]
		b.pressed.connect(one[1])
		_find_bar.add_child(b)
		_replace_bits.append(b)
	var shut := Button.new()
	shut.text = "x"
	shut.pressed.connect(close_find)
	_find_bar.add_child(shut)
	box.add_child(_find_bar)

# ---- completion ---------------------------------------------------------------------
# What the box offers after a dot or a colon. Only a literal path from game,
# workspace or script resolves; following a local that holds an instance would
# need a parser, so nothing is offered rather than a guess.
const GLOBALS := ["game", "workspace", "script", "print", "warn", "error", "task", "wait", "tick", "time",
	"Instance", "Vector3", "Vector2", "CFrame", "Color3", "UDim", "UDim2", "Enum", "BrickColor",
	"TweenInfo", "NumberRange", "NumberSequence", "ColorSequence", "Random", "Ray", "RaycastParams",
	"math", "string", "table", "os", "pairs", "ipairs", "next", "type", "typeof", "tostring", "tonumber",
	"pcall", "xpcall", "select", "unpack", "setmetatable", "getmetatable", "require", "assert", "coroutine"]
const KEYWORDS := ["and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if", "in",
	"local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while", "continue"]

# [text, kind] pairs for the text before the caret; pure, so a test can call it.
func completions_for(before: String) -> Array:
	var out := []
	var re := RegEx.create_from_string("([A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)*)([.:])([A-Za-z_][A-Za-z0-9_]*)?$")
	var m := re.search(before)
	if m == null:
		# A dot after a call's result or an index cannot be followed: offer
		# nothing, not the globals.
		if before.ends_with(".") or before.ends_with(":"):
			return out
		var word := RegEx.create_from_string("([A-Za-z_][A-Za-z0-9_]*)$").search(before)
		var part: String = word.get_string(1) if word != null else ""
		for g in GLOBALS:
			if g.to_lower().begins_with(part.to_lower()):
				out.append([g, CodeEdit.KIND_VARIABLE])
		for k in KEYWORDS:
			if part != "" and k.begins_with(part.to_lower()):
				out.append([k, CodeEdit.KIND_PLAIN_TEXT])
		return out
	var head: String = m.get_string(1)
	var sep: String = m.get_string(2)
	var part: String = m.get_string(3)
	var id := _resolve(head)
	if id < 0:
		return out
	var cls: String = "DataModel" if id == 0 else world.get_instance(id).get("class_name", "")
	var members: Dictionary = world.get_class_members(cls)
	var seen := {}
	if sep == ".":
		for c in world.get_child_ids(id):
			var n: String = world.get_instance(c).get("name", "")
			if n != "" and not seen.has(n) and n.to_lower().begins_with(part.to_lower()):
				seen[n] = true
				out.append([n, CodeEdit.KIND_CLASS])
		for p in members.get("properties", PackedStringArray()):
			if not seen.has(p) and p.to_lower().begins_with(part.to_lower()):
				seen[p] = true
				out.append([p, CodeEdit.KIND_MEMBER])
		for e in members.get("events", PackedStringArray()):
			if not seen.has(e) and e.to_lower().begins_with(part.to_lower()):
				seen[e] = true
				out.append([e, CodeEdit.KIND_SIGNAL])
	else:
		for f in members.get("methods", PackedStringArray()):
			if f.to_lower().begins_with(part.to_lower()):
				out.append([f, CodeEdit.KIND_FUNCTION])
	return out

# The instance a dotted path names; 0 is the DataModel, -1 nothing known.
func _resolve(path: String) -> int:
	if world == null:
		return -1
	var steps := path.split(".")
	var at := -1
	var first: String = steps[0]
	if first == "game":
		at = 0
	elif first == "workspace":
		at = _service("Workspace")
	elif first == "script":
		at = showing()
		if at == 0:
			return -1
	else:
		return -1
	for i in range(1, steps.size()):
		var step: String = steps[i]
		if step == "Parent":
			at = int(world.get_instance(at).get("parent", -1)) if at != 0 else -1
		else:
			var next := -1
			for c in world.get_child_ids(at):
				if world.get_instance(c).get("name", "") == step:
					next = c
					break
			at = next
		if at < 0:
			return -1
	return at

func _service(cls: String) -> int:
	for c in world.get_child_ids(0):
		if world.get_instance(c).get("class_name", "") == cls:
			return c
	return -1

func _offer_completions() -> void:
	if _at < 0:
		return
	var line: String = _code.get_line(_code.get_caret_line())
	var before: String = line.substr(0, _code.get_caret_column())
	var got := completions_for(before)
	if got.is_empty():
		return
	for one in got:
		_code.add_code_completion_option(one[1], one[0], one[0])
	_code.update_code_completion_options(true)

# ---- find and replace ---------------------------------------------------------------
func open_find(with_replace := false) -> void:
	if _at < 0:
		return
	var picked := _code.get_selected_text()
	if picked != "" and not picked.contains("\n"):
		_find_box.text = picked
	_find_bar.visible = true
	for one in _replace_bits:
		one.visible = with_replace
	_retally()
	_find_box.grab_focus()
	_find_box.select_all()

func close_find() -> void:
	_find_bar.visible = false
	_code.grab_focus()

func finding() -> bool:
	return _find_bar != null and _find_bar.visible

func set_query(text: String) -> void:
	_find_box.text = text
	_retally()

func set_with(text: String) -> void:
	_with_box.text = text

func set_case(on: bool) -> void:
	_find_case.button_pressed = on
	_retally()

func at_column() -> int:
	return _code.get_caret_column()

func text_now() -> String:
	return _code.text

func match_count() -> int:
	return _all_matches().size()

# Every match as [line, column], top to bottom. CodeEdit.search takes a starting
# point and wraps, so it is walked rather than asked to enumerate.
func _all_matches() -> Array:
	var out := []
	var q := _find_box.text if _find_box != null else ""
	if q == "" or _at < 0:
		return out
	var flags := TextEdit.SEARCH_MATCH_CASE if _find_case.button_pressed else 0
	var line := 0
	var col := 0
	for guard in 20000:
		# search() answers Vector2i(column, line): x is the column.
		var hit: Vector2i = _code.search(q, flags, line, col)
		if hit.x < 0:
			break
		var at := [hit.y, hit.x]
		if not out.is_empty() and out[0] == at:
			break                      # wrapped round to the first hit
		out.append(at)
		line = hit.y
		col = hit.x + 1
		if col > _code.get_line(line).length():
			line += 1
			col = 0
			if line >= _code.get_line_count():
				break
	return out

func _retally() -> void:
	if _find_says == null:
		return
	var all := _all_matches()
	if _find_box.text == "":
		_find_says.text = ""
	elif all.is_empty():
		_find_says.text = "none"
	else:
		_find_says.text = "%d of %d" % [_which(all) + 1, all.size()]

# The match at or after the caret, wrapping to the first past the last one.
func _which(all: Array) -> int:
	var line := _code.get_caret_line()
	var col := _code.get_caret_column()
	for i in all.size():
		if all[i][0] > line or (all[i][0] == line and all[i][1] >= col):
			return i
	return 0

# way: -1 back, +1 on, 0 to the nearest without moving past it.
func _step(way: int) -> void:
	var all := _all_matches()
	if all.is_empty():
		_retally()
		return
	var i := _which(all)
	if way != 0:
		# _which is already the next match; only a caret sitting on one steps past it.
		i = (i + all.size() - 1) % all.size() if way < 0 else i
		if way > 0 and _on_match(all[i]):
			i = (i + 1) % all.size()
	_show_match(all[i])
	_retally()

func _on_match(at: Array) -> bool:
	return _code.get_caret_line() == at[0] and _code.get_caret_column() == at[1]

func _show_match(at: Array) -> void:
	var n := _find_box.text.length()
	_code.select(at[0], at[1], at[0], at[1] + n)
	_code.set_caret_line(at[0])
	_code.set_caret_column(at[1])
	_code.center_viewport_to_caret()

func replace_one() -> void:
	var all := _all_matches()
	if all.is_empty() or _at < 0:
		return
	var i := _which(all)
	if not _on_match(all[i]):
		_show_match(all[i])
		return                        # the first press only selects the match
	var at: Array = all[i]
	_code.begin_complex_operation()
	_code.remove_text(at[0], at[1], at[0], at[1] + _find_box.text.length())
	_insert_at(at, _with_box.text)
	_code.end_complex_operation()
	_on_typed()
	_step(1)

# Bottom up: replacing a match first would shift the positions of the ones after it.
func replace_all() -> void:
	var all := _all_matches()
	if all.is_empty() or _at < 0:
		return
	var n := _find_box.text.length()
	_code.begin_complex_operation()
	for i in range(all.size() - 1, -1, -1):
		var at: Array = all[i]
		_code.remove_text(at[0], at[1], at[0], at[1] + n)
		_insert_at(at, _with_box.text)
	_code.end_complex_operation()
	_on_typed()
	_retally()

func _insert_at(at: Array, text: String) -> void:
	if text == "":
		return
	_code.set_caret_line(at[0])
	_code.set_caret_column(at[1])
	_code.insert_text_at_caret(text)

# The one panel that stays opaque whatever the theme says: over a moving 3D view,
# fixed text reads as though the text itself were moving.
func repaint() -> void:
	var ground := StyleBoxFlat.new()
	ground.bg_color = palette.code_fill()
	ground.border_color = palette.border
	ground.set_border_width_all(1)
	add_theme_stylebox_override("panel", ground)
	if _code != null:
		_code.syntax_highlighter = _luau()

# Keyword and literal colours chosen to match how a script reads in Roblox Studio.
func _luau() -> CodeHighlighter:
	var lit := CodeHighlighter.new()
	for word in ["and", "break", "do", "else", "elseif", "end", "false", "for", "function", "if",
				 "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until",
				 "while", "continue", "export", "type"]:
		lit.add_keyword_color(word, palette.code_keyword)
	for word in ["self", "script", "game", "workspace", "task", "wait", "print", "warn", "error",
				 "pairs", "ipairs", "require", "tostring", "tonumber", "typeof", "pcall", "next"]:
		lit.add_member_keyword_color(word, palette.code_builtin)
	lit.number_color = palette.code_number
	lit.symbol_color = palette.code_symbol
	lit.function_color = palette.code_function
	lit.member_variable_color = palette.code_member
	# A CodeHighlighter has no string_color or comment_color: both are colour regions.
	lit.add_color_region("\"", "\"", palette.code_string)
	lit.add_color_region("'", "'", palette.code_string)
	lit.add_color_region("[[", "]]", palette.code_string)
	lit.add_color_region("--[[", "]]", palette.code_comment)
	lit.add_color_region("--", "", palette.code_comment, true)
	return lit

# ---- opening and closing ------------------------------------------------------------
func open(id: int) -> void:
	if not is_script(world, id):
		return
	for i in _open.size():
		if _open[i].id == id:
			_show(i)
			return
	commit()
	_open.append({"id": id, "text": source_of(id), "dirty": false})
	_tabs.add_tab(world.get_instance(id).get("name", "Script"))
	_show(_open.size() - 1)

# Open a script with the caret on a line, counting from one as an error names it.
func go_to(id: int, line: int) -> void:
	open(id)
	if showing() != id:
		return
	var want := clampi(line - 1, 0, maxi(_code.get_line_count() - 1, 0))
	_code.set_caret_line(want)
	_code.set_caret_column(0)
	_code.center_viewport_to_caret()
	_code.grab_focus()

func close_at(i: int) -> void:
	if i < 0 or i >= _open.size():
		return
	commit()
	_open.remove_at(i)
	_tabs.remove_tab(i)
	if _open.is_empty():
		_at = -1
		visible = false
		closed.emit()
	else:
		_show(mini(i, _open.size() - 1))
	dirtied.emit()

func close_all() -> void:
	commit()
	_open.clear()
	_tabs.clear_tabs()
	_at = -1
	visible = false
	closed.emit()
	dirtied.emit()

func showing() -> int:
	return _open[_at].id if _at >= 0 and _at < _open.size() else 0

# The caret's line, counting from one as an error does.
func at_line() -> int:
	return _code.get_caret_line() + 1

func dirty() -> bool:
	for one in _open:
		if one.dirty:
			return true
	return false

func _show(i: int) -> void:
	if i < 0 or i >= _open.size():
		return
	if _at != i:
		commit()
	_at = i
	if _tabs.current_tab != i:
		_tabs.current_tab = i
	_applying = true
	_code.text = _open[i].text
	_applying = false
	visible = true
	shown.emit(_open[i].id)

func _on_tab(i: int) -> void:
	_show(i)

# ---- the debugger's gutters ---------------------------------------------------------
var _applying := false               # text or breakpoints set from code, not a user click

func _on_breakpoint(_line: int) -> void:
	if _applying or _at < 0:
		return
	breakpoints_changed.emit(_open[_at].id, breakpoints())

# The showing script's breakpoints, lines counting from one.
func breakpoints() -> Array:
	var out := []
	for l in _code.get_breakpointed_lines():
		out.append(int(l) + 1)
	return out

# Restore a script's gutter breakpoints: replacing the text on a tab switch clears them.
func set_breakpoints(lines: Array) -> void:
	_applying = true
	_code.clear_breakpointed_lines()
	for l in lines:
		if int(l) >= 1 and int(l) <= _code.get_line_count():
			_code.set_line_as_breakpoint(int(l) - 1, true)
	_applying = false

# The arrow on the line a stopped script stands at; 0 clears it.
func mark_executing(line: int) -> void:
	_code.clear_executing_lines()
	if line >= 1 and line <= _code.get_line_count():
		_code.set_line_as_executing(line - 1, true)
		_code.set_caret_line(line - 1)
		_code.center_viewport_to_caret()

func clear_executing() -> void:
	_code.clear_executing_lines()

func type(text: String) -> void:
	if _at < 0:
		return
	_code.text = text
	_on_typed()

func _on_typed() -> void:
	if _at < 0:
		return
	var was: bool = _open[_at].dirty
	_open[_at].text = _code.text
	_open[_at].dirty = true
	_mark(_at)
	if not was:
		dirtied.emit()

func _mark(i: int) -> void:
	if i < 0 or i >= _open.size():
		return
	var name: String = world.get_instance(_open[i].id).get("name", "Script")
	_tabs.set_tab_title(i, name + ("*" if _open[i].dirty else ""))

# ---- handing the text over ----------------------------------------------------------
# Every dirty tab, not only the one on screen. While a place plays, history.world
# is the copy, so nothing is handed over until it stops. Nothing is told to reload: an edit
# world has no script running, and Play runs a fresh copy of the tree that carries the edited
# text because Source serializes with the tree.
func commit() -> void:
	if history == null or history.world != world:
		return
	for i in _open.size():
		_commit_one(i)

func _commit_one(i: int) -> void:
	var one: Dictionary = _open[i]
	if not one.dirty:
		return
	if world.get_instance(one.id).is_empty():   # deleted while the tab was open
		one.dirty = false
		_mark(i)
		return
	if not history.set_property(one.id, "Source", one.text):
		return                                   # not taken: it stays unsaved
	_sent = {"id": one.id, "text": one.text}     # settled() waits on this one, queued last
	one.dirty = false
	_mark(i)
	dirtied.emit()

# Await the handed-over text appearing in the mirror: a write is a queued job, so
# Play copying the tree or Save writing it out would take the text from before it.
func settled() -> void:
	if _sent.is_empty():
		return
	var want: String = _sent.text
	var id: int = _sent.id
	_sent = {}
	for wait in 30:
		if world == null or world.get_instance(id).is_empty():
			return
		if source_of(id) == want:
			return
		await Engine.get_main_loop().process_frame

var _sent := {}          # the last text handed over, until it lands

func source_of(id: int) -> String:
	for p in world.get_properties(id, true):    # Source is a hidden property
		if p.name == "Source":
			return str(p.value)
	return ""

# After a tree change: drop tabs whose script is gone, and follow renames.
func follow() -> void:
	# The showing tab by id, not index: removing a tab shifts every index after it,
	# and a stale _at would commit this text onto a different script.
	var was := showing()
	var i := _open.size() - 1
	while i >= 0:
		if world.get_instance(_open[i].id).is_empty():
			_open.remove_at(i)
			_tabs.remove_tab(i)
		else:
			_mark(i)
		i -= 1
	if _open.is_empty():
		if visible:
			_at = -1
			visible = false
			closed.emit()
		return
	var now := _open.size() - 1
	for j in _open.size():
		if _open[j].id == was:
			now = j
	if now != _at:
		_at = -1            # forces _show to reload the box
		_show(now)
