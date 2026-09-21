# COPY -- do not edit. The original is luau/gdextension/host/Toast.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Transaction cards over the game: pending, then confirmed or reverted in the same card.
# `raise_pending` returns the card and `settle` finishes it; a pending card never times out.
# A click shows the transaction -- the place itself, or the block explorer.
class_name PulseBlockzToast
extends CanvasLayer

## Block explorer host, used when the place does not handle the click itself.
var explorer_site := "scan.v4.testnet.pulsechain.com"
## Set by the wallet; the route to the place's Chain node.
var world
## Makes each ShowScan distinct: an attribute set to the value it already holds signals
## nothing, so two clicks on one hash would show once.
var _asked := 0

## Seconds an answered card stays up.
const KEEP := 9.0
## Most cards on screen at once.
const MOST := 4
## Card width, sized to hold the shortened hash without wrapping.
const WIDE := 560.0

const GOOD := Color(0.36, 0.85, 0.52)
const BAD := Color(0.95, 0.42, 0.42)
const WAIT := Color(0.62, 0.70, 0.86)

var _column: VBoxContainer

func _ready() -> void:
	layer = 90          # over the game, under the wallet's prompt at 100
	var margin := MarginContainer.new()
	margin.set_anchors_preset(Control.PRESET_TOP_WIDE)
	margin.offset_top = 14
	margin.offset_bottom = 400
	margin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(margin)
	_column = VBoxContainer.new()
	_column.add_theme_constant_override("separation", 8)
	_column.alignment = BoxContainer.ALIGNMENT_BEGIN
	_column.mouse_filter = Control.MOUSE_FILTER_IGNORE
	margin.add_child(_column)

## Returns the card, to hand to `settle` once the answer is in.
func raise_pending(title: String, detail: String, hash: String = "") -> Control:
	return _card(title, detail, hash, WAIT, false)

## A card that starts as its own answer -- something that failed before it had a hash.
func raise_done(title: String, detail: String, ok: bool, hash: String = "") -> Control:
	return _card(title, detail, hash, GOOD if ok else BAD, true)

## Turns a pending card into its answer in place; no second card for the same act.
func settle(card: Control, title: String, detail: String, ok: bool, hash: String = "") -> void:
	if card == null or not is_instance_valid(card):
		return
	card.set_meta("hash", hash if hash != "" else String(card.get_meta("hash", "")))
	# owned = false: these nodes are built at runtime with no owner, and an owned-only search
	# would skip every one of them
	var stripe := card.find_child("Stripe", true, false) as ColorRect
	if stripe:
		stripe.color = GOOD if ok else BAD
	var head := card.find_child("Title", true, false) as Label
	if head:
		head.text = title
		head.add_theme_color_override("font_color", GOOD if ok else BAD)
	var sub := card.find_child("Detail", true, false) as Label
	if sub:
		sub.text = _with_hash(detail, String(card.get_meta("hash", "")))
	_fade_later(card)

func _with_hash(detail: String, hash: String) -> String:
	if hash == "":
		return detail
	var short := hash.substr(0, 10) + ".." + hash.substr(hash.length() - 6)
	return "%s\n%s   .   click to open it on the scan" % [detail, short]

func _card(title: String, detail: String, hash: String, tint: Color, finished: bool) -> Control:
	while _column.get_child_count() >= MOST:
		var oldest := _column.get_child(0)
		_column.remove_child(oldest)
		oldest.queue_free()

	var card := PanelContainer.new()
	card.mouse_filter = Control.MOUSE_FILTER_STOP
	card.custom_minimum_size = Vector2(WIDE, 0)
	card.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	card.set_meta("hash", hash)
	var box := StyleBoxFlat.new()
	box.bg_color = Color(0.06, 0.07, 0.11, 0.94)
	box.set_corner_radius_all(6)
	box.set_border_width_all(1)
	box.border_color = Color(1, 1, 1, 0.10)
	box.content_margin_left = 0
	box.content_margin_right = 16
	box.content_margin_top = 12
	box.content_margin_bottom = 12
	card.add_theme_stylebox_override("panel", box)

	var row := HBoxContainer.new()
	row.name = "Row"
	row.add_theme_constant_override("separation", 10)
	card.add_child(row)

	# A colour bar, not an icon: a place can replace the pictures the client ships with.
	var stripe := ColorRect.new()
	stripe.name = "Stripe"
	stripe.color = tint
	stripe.custom_minimum_size = Vector2(5, 0)
	stripe.size_flags_vertical = Control.SIZE_EXPAND_FILL
	row.add_child(stripe)

	var body := VBoxContainer.new()
	body.name = "Body"
	body.add_theme_constant_override("separation", 2)
	body.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(body)

	var head := Label.new()
	head.name = "Title"
	head.text = title
	head.add_theme_color_override("font_color", tint)
	head.add_theme_font_size_override("font_size", 22)
	body.add_child(head)

	var sub := Label.new()
	sub.name = "Detail"
	sub.text = _with_hash(detail, hash)
	sub.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	sub.add_theme_font_size_override("font_size", 16)
	sub.modulate = Color(0.80, 0.84, 0.92)
	body.add_child(sub)

	card.gui_input.connect(func(event):
		var mb := event as InputEventMouseButton
		if mb and mb.pressed and mb.button_index == MOUSE_BUTTON_LEFT:
			_open(String(card.get_meta("hash", ""))))
	_column.add_child(card)
	card.modulate = Color(1, 1, 1, 0)
	create_tween().tween_property(card, "modulate:a", 1.0, 0.18)
	if finished:
		_fade_later(card)
	return card

## Instance id of ReplicatedStorage/Chain, or 0 if the place has no Chain node.
func _chain_id() -> int:
	if world == null:
		return 0
	var id := 0
	for want in ["ReplicatedStorage", "Chain"]:
		var found := 0
		for cid in world.get_child_ids(id):
			if String((world.get_instance(cid) as Dictionary).get("name", "")) == want:
				found = cid
				break
		if found == 0:
			return 0
		id = found
	return id

## Shows the transaction: the place when Chain carries AskScanHandles, the browser otherwise.
## The attribute is read, never asked for: an answer to wait on would race the place's panel
## against a browser window opening for the same click.
func _open(hash: String) -> void:
	if hash == "":
		return
	var chain := _chain_id()
	var handled := false
	if chain != 0:
		handled = bool((world.get_attributes(chain) as Dictionary).get("AskScanHandles", false))
	if handled:
		_asked += 1
		world.run_client_chunk("toast_show", """
local rs = game:GetService("ReplicatedStorage")
local c = rs:FindFirstChild("Chain")
if c then c:SetAttribute("ShowScan", %s) end
""" % preload("res://host/Luau.gd").quote(JSON.stringify({"n": _asked, "view": "tx", "id": hash})))
		return
	OS.shell_open("https://%s/tx/%s" % [explorer_site, hash])

func _fade_later(card: Control) -> void:
	await get_tree().create_timer(KEEP).timeout
	if not is_instance_valid(card):
		return
	var out := create_tween()
	out.tween_property(card, "modulate:a", 0.0, 0.35)
	await out.finished
	if is_instance_valid(card):
		card.queue_free()
