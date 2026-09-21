# Joins a server in a real window and photographs what a player sees, second by second.
#
#   godot --path . -s res://tests/join_shot.gd -- host:port <out dir>
#
# Windowed on purpose: a headless run has no pixels, so it can prove what a join does and never
# what it looks like.
extends SceneTree

var player: Node
var t := 0.0
var shot := 0
var out_dir := "user://join-shots"

func _initialize() -> void:
	var where := "play.safewrap.xyz:8800"
	for a in OS.get_cmdline_user_args():
		if a.contains(":") and not a.begins_with("--"):
			where = a
		elif a.begins_with("--out="):
			out_dir = a.substr(6)
	DirAccess.make_dir_recursive_absolute(out_dir)
	get_root().set_content_scale_size(Vector2i(1280, 720))
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://join-shots-profile"
	get_root().add_child(player)
	_start.call_deferred(where)

func _start(where: String) -> void:
	# Added from _initialize, the Player's own _ready has not run yet: its screens do not exist
	# until a frame has passed, and a page built before them lands on nothing.
	await get_root().get_tree().process_frame
	var hp: Dictionary = player._split_server(where)
	# Through the screens a player uses, not straight into join(): the join page is what hides
	# home and sets the wallet's leash, and a probe that skips it is not watching the same thing.
	player.join_page(String(hp.host), int(hp.port))
	_press.call_deferred("Join")

## Presses the button of that name on whatever page is up.
func _press(label: String) -> void:
	await get_root().get_tree().process_frame
	for b in _buttons(player._page):
		if b.text == label:
			b.emit_signal("pressed")
			print("PRESSED %s" % label)
			return
	print("NO BUTTON %s" % label)

func _buttons(n: Node) -> Array:
	var out := []
	if n == null or not is_instance_valid(n):
		return out
	if n is Button:
		out.append(n)
	for c in n.get_children():
		out.append_array(_buttons(c))
	return out

func _process(delta: float) -> bool:
	t += delta
	if t < shot * 2.0 + 1.0:
		return false
	shot += 1
	var img := get_root().get_texture().get_image()
	img.save_png("%s/%02d.png" % [out_dir, shot])
	# What is on screen, named, so a picture can be read against what the code thinks it is doing.
	var page: String = "page" if (player._page != null and is_instance_valid(player._page)) else "-"
	var card: String = "card" if (player._wait != null and is_instance_valid(player._wait)) else "-"
	var names := PackedStringArray()
	for b in _buttons(player._page):
		names.append(b.text)
	print("SHOT %02d  t=%4.1f  %s %s  session=%s  buttons=[%s]"
		% [shot, t, page, card, player.session != null, ", ".join(names)])
	if names.has("Go in"):
		_press("Go in")
	if shot >= 16:
		quit(0)
	return false
