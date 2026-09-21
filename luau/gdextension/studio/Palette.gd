# The Studio's colours, named by role, and the Theme built from them. Dark and
# Light are authored; Clear is derived -- Dark with panel fills let down to
# `panel_alpha`. Only a panel's fill is ever translucent: a bright sky sits close
# enough in luminance to a light grey that translucent text over one disappears.
class_name Palette
extends RefCounted

const NAMES := ["Dark", "Light", "Clear"]

# X red, Y green, Z blue: world-space gizmo axes, the same in every variant.
const AXIS := [Color(0.94, 0.31, 0.36), Color(0.45, 0.83, 0.36), Color(0.30, 0.55, 0.95)]

var name := "Dark"
var panel_alpha := 1.0

var ground: Color
var surface: Color
var surface_alt: Color
var border: Color
var text: Color
var text_dim: Color
var text_faint: Color
var accent: Color
var selection: Color
var warning: Color
var error: Color
var success: Color
var outline: Color          # the selection box drawn in the 3D view
var playing: Color          # the frame round the view while a place is running

var code_bg: Color
var code_text: Color
var code_keyword: Color
var code_builtin: Color
var code_number: Color
var code_symbol: Color
var code_function: Color
var code_member: Color
var code_string: Color
var code_comment: Color
var code_gutter: Color

static func of(which: String) -> Palette:
	var p := Palette.new()
	p.name = which
	match which:
		"Light": p._light()
		"Clear": p._clear()
		_: p._dark()
	return p

func _dark() -> void:
	ground = Color("14161b"); surface = Color("1b1e25"); surface_alt = Color("232830"); border = Color("2f353f")
	text = Color("e4e7ec"); text_dim = Color("98a0b0"); text_faint = Color("6e7787")
	accent = Color("ff9436"); selection = Color("2f4a6b")
	warning = Color("f5c26b"); error = Color("f26d6d"); success = Color("7fd18f")
	outline = Color("ff8c1a"); playing = Color("3d8ce8")
	code_bg = Color("16181d"); code_text = Color("d6dbe3"); code_gutter = Color("5a6472")
	code_keyword = Color("c586c0"); code_builtin = Color("4ec9b0"); code_number = Color("b5cea8")
	code_symbol = Color("cfd3da"); code_function = Color("dcdcaa"); code_member = Color("9cdcfe")
	code_string = Color("ce9178"); code_comment = Color("6a9955")

func _light() -> void:
	ground = Color("e9ecf0"); surface = Color("ffffff"); surface_alt = Color("f3f5f8"); border = Color("c6ccd6")
	text = Color("1a1d23"); text_dim = Color("545c6a"); text_faint = Color("7a8493")
	accent = Color("c2560a"); selection = Color("cfe0f5")
	warning = Color("a2660b"); error = Color("b3261e"); success = Color("2e7d3a")
	outline = Color("d2600c"); playing = Color("1668cc")
	code_bg = Color("fbfcfd"); code_text = Color("24292f"); code_gutter = Color("9aa0a6")
	code_keyword = Color("a626a4"); code_builtin = Color("0184bc"); code_number = Color("986801")
	code_symbol = Color("383a42"); code_function = Color("4078f2"); code_member = Color("c1601c")
	code_string = Color("3f8b3f"); code_comment = Color("9296a1")

# Dark seen through: lifted text and harder borders, because a see-through panel
# needs a firmer edge to read as a panel at all.
func _clear() -> void:
	_dark()
	panel_alpha = 0.72
	ground = Color("0d0f13")
	border = Color("59616f")
	text = Color("f6f8fa"); text_dim = Color("d3d9e2"); text_faint = Color("aeb7c4")
	code_text = Color("e8ecf2")

# A panel's fill, let down by the variant. Never on text.
func fill(role: Color) -> Color:
	return Color(role.r, role.g, role.b, role.a * panel_alpha)

# The code editor's ground stays near-opaque in every variant: a moving view
# behind fixed text reads as the text itself moving.
func code_fill() -> Color:
	return Color(code_bg.r, code_bg.g, code_bg.b, maxf(panel_alpha, 0.95))

# ---- the Theme ---------------------------------------------------------------------
# Setters take (item name, theme type, value) -- the name first. One Theme on the
# root Window reaches popups and embedded windows too, since Window carries the
# same `theme` property a Control does. An item name that is not real fails
# silently, so these are Godot 4.3's names.
func theme() -> Theme:
	var t := Theme.new()

	t.set_stylebox("panel", "PanelContainer", _box(surface, border))
	t.set_stylebox("panel", "Panel", _box(surface, border))
	t.set_stylebox("panel", "PopupPanel", _box(surface, border))

	for state in ["normal", "hover", "pressed", "disabled", "focus"]:
		var face := surface_alt
		if state == "hover": face = surface_alt.lerp(text, 0.10)
		elif state == "pressed": face = accent.lerp(surface_alt, 0.55)
		elif state == "disabled": face = surface_alt.lerp(surface, 0.6)
		var edge := border if state != "focus" else accent
		t.set_stylebox(state, "Button", _box(face, edge, 3))
	t.set_color("font_color", "Button", text)
	t.set_color("font_hover_color", "Button", text)
	t.set_color("font_pressed_color", "Button", text)
	t.set_color("font_focus_color", "Button", text)
	t.set_color("font_disabled_color", "Button", Color(text_faint.r, text_faint.g, text_faint.b, 0.6))
	t.set_color("icon_normal_color", "Button", text_dim)
	t.set_color("icon_hover_color", "Button", text)
	t.set_color("icon_pressed_color", "Button", text)
	t.set_color("icon_focus_color", "Button", text)
	t.set_color("icon_disabled_color", "Button", Color(text_faint.r, text_faint.g, text_faint.b, 0.45))
	t.set_constant("h_separation", "Button", 5)

	t.set_stylebox("panel", "Tree", _box(surface, border))
	t.set_stylebox("selected", "Tree", _box(selection, selection, 2))
	t.set_stylebox("selected_focus", "Tree", _box(selection.lerp(accent, 0.18), accent, 2))
	t.set_stylebox("cursor", "Tree", _box(Color(0, 0, 0, 0), accent, 2))
	for state in ["title_button_normal", "title_button_hover", "title_button_pressed"]:
		# Opaque in every variant: a Tree paints its column titles after its rows,
		# so a see-through header shows the row scrolled under it.
		var head := _box(surface_alt, border, 0)
		head.bg_color = Color(surface_alt.r, surface_alt.g, surface_alt.b, 1.0)
		t.set_stylebox(state, "Tree", head)
	t.set_color("font_color", "Tree", text)
	t.set_color("font_selected_color", "Tree", text)
	t.set_color("title_button_color", "Tree", text_dim)
	t.set_color("guide_color", "Tree", Color(border.r, border.g, border.b, 0.5))
	t.set_constant("v_separation", "Tree", 5)

	t.set_stylebox("normal", "LineEdit", _box(surface_alt, border, 3))
	t.set_stylebox("focus", "LineEdit", _box(surface_alt, accent, 3))
	t.set_stylebox("read_only", "LineEdit", _box(surface, border, 3))
	t.set_color("font_color", "LineEdit", text)
	t.set_color("font_placeholder_color", "LineEdit", text_faint)
	t.set_color("caret_color", "LineEdit", accent)
	t.set_color("selection_color", "LineEdit", selection)

	t.set_stylebox("panel", "PopupMenu", _box(surface, border))
	t.set_stylebox("hover", "PopupMenu", _box(selection, selection, 2))
	t.set_color("font_color", "PopupMenu", text)
	t.set_color("font_hover_color", "PopupMenu", text)
	t.set_color("font_disabled_color", "PopupMenu", Color(text_faint.r, text_faint.g, text_faint.b, 0.55))
	t.set_color("font_separator_color", "PopupMenu", text_faint)

	t.set_stylebox("tab_selected", "TabBar", _box(surface_alt.lerp(accent, 0.10), accent, 0))
	t.set_stylebox("tab_unselected", "TabBar", _box(surface, border, 0))
	t.set_stylebox("tab_hovered", "TabBar", _box(surface_alt, border, 0))
	t.set_color("font_selected_color", "TabBar", text)
	t.set_color("font_unselected_color", "TabBar", text_dim)
	t.set_color("font_hovered_color", "TabBar", text)

	t.set_stylebox("normal", "CodeEdit", _box(code_fill(), border, 0))
	t.set_stylebox("focus", "CodeEdit", _box(Color(0, 0, 0, 0), Color(0, 0, 0, 0), 0))
	t.set_color("font_color", "CodeEdit", code_text)
	t.set_color("caret_color", "CodeEdit", accent)
	t.set_color("selection_color", "CodeEdit", selection)
	t.set_color("line_number_color", "CodeEdit", code_gutter)
	t.set_color("current_line_color", "CodeEdit", Color(accent.r, accent.g, accent.b, 0.06))
	t.set_color("background_color", "CodeEdit", Color(0, 0, 0, 0))

	t.set_stylebox("normal", "RichTextLabel", _box(surface, border, 0))
	t.set_color("default_color", "RichTextLabel", text_dim)
	t.set_color("font_color", "Label", text)

	t.set_stylebox("panel", "ItemList", _box(surface_alt, border, 3))
	t.set_stylebox("selected", "ItemList", _box(selection, selection, 2))
	t.set_stylebox("selected_focus", "ItemList", _box(selection.lerp(accent, 0.18), accent, 2))
	t.set_color("font_color", "ItemList", text)
	t.set_color("font_selected_color", "ItemList", text)

	t.set_stylebox("panel", "AcceptDialog", _box(surface, border))
	t.set_color("title_color", "Window", text)

	var rule := StyleBoxLine.new()
	rule.color = border
	rule.vertical = true
	rule.grow_begin = 2
	rule.grow_end = 2
	t.set_stylebox("separator", "VSeparator", rule)
	return t

func _box(face: Color, edge: Color, radius := 4) -> StyleBoxFlat:
	var b := StyleBoxFlat.new()
	b.bg_color = fill(face)
	b.border_color = edge
	b.set_border_width_all(1)
	b.set_corner_radius_all(radius)
	b.content_margin_left = 6
	b.content_margin_right = 6
	b.content_margin_top = 3
	b.content_margin_bottom = 3
	return b
