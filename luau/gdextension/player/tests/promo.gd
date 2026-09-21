# Footage of the Player for a promo: one shot a run, written as a movie, with stills along the way.
#
#   godot --path . --write-movie <out>.avi --fixed-fps 60 --resolution 1920x1080 -s res://tests/promo.gd -- <shot> <stills dir>
#
# Movie mode steps the clock a fixed sixtieth per frame however long the frame took to draw, so
# the footage is smooth at full quality and needs no screen to record from; demo2/tests/promo.gd
# shoots the town the same way. tests/promo.sh runs every shot and cuts each movie down to the
# CLIP <first frame> <last frame> printed here: the frames that are the shot, the rest being the
# app starting and the chain answering.
extends SceneTree

var player: Node
var shot := "home"
var stills := ""
var frame := 0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	shot = args[0] if args.size() > 0 else "home"
	stills = args[1] if args.size() > 1 else "user://promo"
	DirAccess.make_dir_recursive_absolute(stills)

	# Without this the window opens at screen size and the movie's frames with it, whatever
	# --resolution said. demo2/tests/promo.gd sets the same four lines for it.
	root.mode = Window.MODE_WINDOWED
	root.borderless = true
	root.size = Vector2i(1920, 1080)
	root.position = Vector2i(0, 0)

	player = load("res://Player.tscn").instantiate()
	player.show_title = (shot == "title")
	# Its own profile: a take never touches a real player's recents or settings.
	player.profile_dir = "user://promo-profile"
	root.add_child(player)
	_run()

func _process(_delta: float) -> bool:
	frame += 1
	return false

func _wait(seconds: float) -> void:
	await create_timer(seconds).timeout

func _still(name: String) -> void:
	var file := stills.path_join("%s-%s.png" % [shot, name])
	root.get_texture().get_image().save_png(file)
	print("  STILL ", file)

## The page's ScrollContainer, or null. Every page is a MarginContainer holding one.
func _scroll() -> ScrollContainer:
	var page = player._page
	if page == null or not is_instance_valid(page):
		return null
	for c in page.get_children():
		if c is ScrollContainer:
			return c
	return null

## Eased scroll to the bottom or back to the top over `seconds`, so it reads as somebody looking
## down the page rather than a cut.
func _read_down(to_bottom: bool, seconds: float) -> void:
	var scroll := _scroll()
	if scroll == null:
		await _wait(seconds)
		return
	var bar := scroll.get_v_scroll_bar()
	var from := float(scroll.scroll_vertical)
	var to := float(bar.max_value - scroll.size.y) if to_bottom else 0.0
	if to <= from and to_bottom:
		await _wait(seconds)
		return
	var t := 0.0
	while t < seconds:
		t += 1.0 / 60.0
		var a: float = clamp(t / seconds, 0.0, 1.0)
		scroll.scroll_vertical = int(lerp(from, to, a * a * (3.0 - 2.0 * a)))   # smoothstep
		await process_frame

func _finish(first: int) -> void:
	print("CLIP %d %d" % [first, frame])
	quit(0)

func _run() -> void:
	await _wait(1.0)

	if shot == "title":
		await _wait(3.0)
		_still("card")
		await _wait(2.0)
		_finish(60)
		return

	# Every other shot needs the home screen up and the chain to have answered.
	player._refresh_home()
	await _wait(4.0)
	var first := frame

	if shot == "home":
		_still("list")
		await _wait(5.0)
		_finish(first)
		return

	var uri := ""
	for e in player.listing():
		if String(e.get("name", "")).begins_with("pBlockz"):
			uri = String(e.uri)
			break
	if uri == "":
		print("PROMO no published experience to open")
		_finish(first)
		return

	await player.open(uri)
	await _wait(3.0)
	first = frame

	if shot == "preview":
		_still("top")
		await _wait(2.0)
		await _read_down(true, 6.0)
		_still("bottom")
		await _wait(1.5)
		_finish(first)
		return

	if shot == "contracts":
		await _read_down(true, 2.5)
		# 12 s: every contract row is waiting on the explorer for its "verified", and that
		# answer is the shot.
		await _wait(12.0)
		_still("contracts")
		await _wait(4.0)
		_finish(first)
		return

	print("PROMO no such shot: %s" % shot)
	_finish(first)
