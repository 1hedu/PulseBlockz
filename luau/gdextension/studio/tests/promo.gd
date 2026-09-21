# Promo footage of the Studio: the editor with a place open in it.
#
#   godot --path . --write-movie <out>.avi --fixed-fps 60 --resolution 1920x1080 -s res://tests/promo.gd -- <shot> <stills dir>
#
# Movie mode steps the clock a fixed sixtieth per frame, so it records without a screen.
# CLIP <first frame> <last frame> marks the shot; before it the editor is still starting.
# demo2/tests/promo.gd and player/tests/promo.gd follow the same recipe -- change them together.
extends SceneTree

var shot := "studio"
var stills := ""
var frame := 0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	shot = args[0] if args.size() > 0 else "studio"
	stills = args[1] if args.size() > 1 else "user://promo"
	DirAccess.make_dir_recursive_absolute(stills)

	root.mode = Window.MODE_WINDOWED
	root.borderless = true
	root.size = Vector2i(1920, 1080)
	root.position = Vector2i(0, 0)

	root.add_child(load("res://Studio.tscn").instantiate())
	_run()

func _process(_delta: float) -> bool:
	frame += 1
	return false

func _wait(seconds: float) -> void:
	await create_timer(seconds).timeout

func _still(name: String) -> void:
	root.get_texture().get_image().save_png(stills.path_join("%s-%s.png" % [shot, name]))

func _run() -> void:
	# The editor builds its panes and then loads whatever place it opened last.
	await _wait(6.0)
	var first := frame
	_still("open")
	await _wait(6.0)
	print("CLIP %d %d" % [first, frame])
	quit(0)
