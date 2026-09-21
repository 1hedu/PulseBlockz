# Footage of the Publisher for a promo: a place being put on chain from its folder.
#
#   godot --path . --write-movie <out>.avi --fixed-fps 60 --resolution 1920x1080 -s res://tests/promo.gd -- <shot> <stills dir>
#
# The same shape as demo2/tests/promo.gd, player/tests/promo.gd and studio/tests/promo.gd: movie
# mode steps the clock a fixed sixtieth at a time however long a frame took to draw, so it needs
# no screen to record from.
#
# The shots:
#   publisher  the form as it opens: the folder, the pictures, what it will declare it uses
#
# It prints CLIP <first frame> <last frame>: the part of the movie that is the shot, the rest
# being the app starting.
extends SceneTree

var shot := "publisher"
var stills := ""
var frame := 0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	shot = args[0] if args.size() > 0 else "publisher"
	stills = args[1] if args.size() > 1 else "user://promo"
	DirAccess.make_dir_recursive_absolute(stills)

	root.mode = Window.MODE_WINDOWED
	root.borderless = true
	root.size = Vector2i(1920, 1080)
	root.position = Vector2i(0, 0)

	# Past the wallet door: the shot is the form, not the title card. The Player's own title
	# is the one that belongs in a film, and two of them is one too many.
	var app: Node = load("res://Publisher.tscn").instantiate()
	app.show_title = false
	root.add_child(app)
	_run()

func _process(_delta: float) -> bool:
	frame += 1
	return false

func _wait(seconds: float) -> void:
	await create_timer(seconds).timeout

func _still(name: String) -> void:
	root.get_texture().get_image().save_png(stills.path_join("%s-%s.png" % [shot, name]))

func _run() -> void:
	await _wait(5.0)
	var first := frame
	_still("form")
	await _wait(6.0)
	print("CLIP %d %d" % [first, frame])
	quit(0)
