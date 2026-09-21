# Handles for a GuiObject in the previewed UI: an outline and eight grips, drawn
# in the Studio's own layer over the view. The preview is laid out at one pixel
# per pixel, so a pixel of drag is one unit of UDim2 offset -- the scale halves
# are left alone and no anchor algebra is needed.
class_name GuiHandles
extends Control

const GRIP := 5.0            # half the size of a grip square, in pixels
const REACH := 8.0           # how near the pointer must be to take one

var world: PulseBlockzWorld
var history: History
var palette: Palette = Palette.of("Dark")

var id := 0                  # the GuiObject shown, 0 for none
var rect := Rect2()          # its rect on screen, kept up to date by the Studio

var _grip := -2              # -2 nothing, -1 the body, 0..7 a grip
var _from := {}              # what the drag started from
var _at := Vector2.ZERO

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_FULL_RECT)

func show_for(gui_id: int, on_screen: Rect2) -> void:
	if id != gui_id or rect != on_screen:
		id = gui_id
		rect = on_screen
		queue_redraw()
	visible = id != 0 and rect.size.x > 0.5

func dragging() -> bool:
	return _grip != -2

# The eight grips: corners then edge middles, as (x, y) in 0..1 of the rect.
const GRIPS := [Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1),
				Vector2(0.5, 0), Vector2(1, 0.5), Vector2(0.5, 1), Vector2(0, 0.5)]

func grip_at(point: Vector2) -> int:
	if id == 0:
		return -2
	for i in GRIPS.size():
		if point.distance_to(rect.position + rect.size * GRIPS[i]) <= REACH:
			return i
	return -1 if rect.has_point(point) else -2

func begin(point: Vector2) -> bool:
	var g := grip_at(point)
	if g == -2 or world == null or history == null:
		return false
	_grip = g
	_at = point
	_from = {"position": _udim2(id, "Position"), "size": _udim2(id, "Size")}
	history.begin(("move " if g == -1 else "resize ") + world.get_instance(id).get("name", "gui"))
	return true

func drag(point: Vector2) -> void:
	if not dragging():
		return
	var d := point - _at
	var pos: Vector4 = _from.position
	var size: Vector4 = _from.size
	if _grip == -1:
		history.set_property(id, "Position", Vector4(pos.x, pos.y + d.x, pos.z, pos.w + d.y))
		return
	var g: Vector2 = GRIPS[_grip]
	# Right and bottom grips grow Size; left and top grow it the other way and
	# move Position by the same amount, so the far edge stays put.
	var dx := d.x if g.x > 0.75 else (-d.x if g.x < 0.25 else 0.0)
	var dy := d.y if g.y > 0.75 else (-d.y if g.y < 0.25 else 0.0)
	var px := d.x if g.x < 0.25 else 0.0
	var py := d.y if g.y < 0.25 else 0.0
	history.set_property(id, "Size", Vector4(size.x, maxf(size.y + dx, 1.0), size.z, maxf(size.w + dy, 1.0)))
	if px != 0.0 or py != 0.0:
		history.set_property(id, "Position", Vector4(pos.x, pos.y + px, pos.z, pos.w + py))

func finish() -> void:
	if not dragging():
		return
	_grip = -2
	history.end()

func _udim2(of: int, name: String) -> Vector4:
	for p in world.get_properties(of):
		if p.name == name and p.value is Vector4:
			return p.value
	return Vector4.ZERO

func _draw() -> void:
	if id == 0:
		return
	draw_rect(rect, palette.accent, false, 1.5)
	for g in GRIPS:
		var c: Vector2 = rect.position + rect.size * g
		draw_rect(Rect2(c - Vector2.ONE * GRIP, Vector2.ONE * GRIP * 2), palette.accent, true)
		draw_rect(Rect2(c - Vector2.ONE * GRIP, Vector2.ONE * GRIP * 2), palette.ground, false, 1.0)
