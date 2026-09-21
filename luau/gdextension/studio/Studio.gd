# PulseBlockz Studio: the place editor. A PulseBlockzWorld in edit mode -- no local player, no
# character, no in-game camera -- with a free camera, an Explorer and a Properties grid.
#   godot --path studio -- --place=<a directory laid out like a Rojo project>
extends Node3D

const DOCK_WIDTH := 320
const OUTPUT_HEIGHT := 150
const DOCK_MIN := 140
const OUTPUT_MIN := 48

var world: PulseBlockzWorld        # the edit world: the tree, not running
var playing: PulseBlockzWorld      # the world Play made, or null
# Seconds a script may run without yielding: Roblox Studio's "Script Timeout Length" default.
var script_timeout := 10.0
var place := Place.new()
var history := History.new()   # every edit goes through it

var _explorer: Explorer
var _properties: Properties
var _output: RichTextLabel
var _status: Label
var _commands := {}         # action id -> the Button standing for it on the ribbon
var _tab_bar: TabBar
var _band: HBoxContainer    # the row of groups under the tabs
var _docks := {}            # panel name -> its PanelContainer, for the View tab
var _stack: VSplitContainer # what is above the Output | the Output
var _side: HSplitContainer  # Explorer | the rest
var _rest: HSplitContainer  # the viewport | Properties
var _settings := ConfigFile.new()   # the whole of user://studio.cfg, held in memory
var _shown_levels := {"print": true, "warn": true, "error": true, "studio": true}
var _bar: Label             # the status line along the bottom
var _command: LineEdit      # one line of Luau, run against the live world
var _said: Array = []       # what has been typed into it
var _said_at := 0
var _was_looking: Transform3D   # where the camera stood before Play took it
var _cycle_at := Vector2.ZERO   # the last place clicked, for cycling through overlaps
var _cycle_past: Array = []     # the bodies already picked there
var _grid: MeshInstance3D       # the stud grid on the ground
var _gui_preview := true        # StarterGui drawn over the view while you edit
var _ground_res := 64           # squares across a new field
var _test_players := 0          # extra players seated in a playtest, besides you
var _terrain: TerrainTool       # the ground brush, when it has the pointer
var _sculpting := false         # the Terrain tab's brush is the active tool
var _playmark: PanelContainer   # the frame drawn round the view while a place runs
var _playing_says: Label
var _gui_handles: GuiHandles     # round a selected GuiObject in the previewed UI
var _file_pop: PopupMenu        # the File menu, so the autosave tick can be ticked
var _recent_pop: PopupMenu      # the places opened before this one
var _autosave: Timer
var _autosave_on := false
var _collision_shown := false
var _cam: Camera3D
var _box: MeshInstance3D
var _handles: Handles
var _selection: Array = []      # instance ids, in the order they were picked
var _marquee: Panel             # the drag box, while one is being dragged
var _marquee_from := Vector2.ZERO
var _marquee_armed := false
var _marquee_on := false
var _clipboard: Array = []      # {name, model} for each thing copied
var _menu: PopupMenu
var _editor: ScriptPane
const AnimationEditorScript = preload("res://AnimationEditor.gd")   # by path: the class cache is per machine
var _anim: AnimationEditorScript   # the Animation Editor, under the view when a rig is being posed
const DebuggerPaneScript = preload("res://DebuggerPane.gd")
const RigBuilderScript = preload("res://RigBuilder.gd")
var _debugger: DebuggerPaneScript  # the call stack and locals while a played script stands at a breakpoint
var _breakpoints := {}             # script full name -> Array of lines (from one); the gutter is the source of truth
var _debug_paused := false
var _ask: ConfirmationDialog
var _dirty := false         # the place has edits that are not on disk
var _opened := false        # the place has finished arriving: only then may Save sweep
var _gap: Control           # the hole the 3D shows through
# Play gets a viewport of its own filling the hole, so a ScreenGui lays out to the view rather
# than the window, as in Roblox. Run keeps the Studio's view.
var _play_view: SubViewportContainer = null
var _play_vp: SubViewport = null
var _tree_seen := -1
var palette := Palette.of("Dark")
var _lines: Array = []      # {from, text, role} -- the Output is re-rendered when the theme changes
var _ui: Control            # the root of the UI: a CanvasLayer owns no theme, a Control does
var _insert: Window
var _right_at := Vector2.ZERO   # where the right button went down, to tell a click from a look

var _looking := false
var _panning := false            # the middle button: slide the view rather than turn it
var _yaw := 0.0
var _pitch := 0.0
var _speed := 24.0

func _ready():
	# Menus and the insert window sit inside the Studio's window, not as OS windows.
	get_window().gui_embed_subwindows = true
	get_tree().auto_accept_quit = false      # the close guard gets a say first
	_settings.load(LOOK_FILE)                # whatever was remembered last time
	_cam = $Camera
	_cam.current = true
	_yaw = _cam.rotation.y
	_pitch = _cam.rotation.x
	_build_ui()
	_build_box()
	_build_grid()
	_handles = Handles.new()
	_handles.camera = _cam
	_handles.history = history
	_handles.changed.connect(func(): _properties.refresh())
	add_child(_handles)
	_properties.history = history
	_terrain = TerrainTool.new()
	_terrain.history = history
	history.changed.connect(_history_changed)
	open_place(_place_argument())
	_tool(Handles.Mode.MOVE)
	set_look(_remembered_look())
	var tick := Timer.new()          # the panel follows the world, a few times a second
	tick.wait_time = 0.2
	tick.timeout.connect(func():
		_properties.refresh()
		if _team_label != null and is_instance_valid(_team_label):
			_team_label.text = _team_text())
	add_child(tick)
	tick.start()
	_test_players = int(_setting("studio", "test_players", 0))
	_gui_preview = bool(_setting("studio", "gui_preview", true))
	set_gui_preview(_gui_preview)
	_autosave = Timer.new()
	_autosave.timeout.connect(_autosave_tick)
	add_child(_autosave)
	set_autosave(bool(_setting("studio", "autosave", false)))

func _place_argument() -> String:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--place="):
			return a.get_slice("=", 1)
	# From a checkout, the Luau demo beside this project (the tests open it).
	var demo := ProjectSettings.globalize_path("res://").path_join("../demo/scripts").simplify_path()
	if not OS.has_feature("template") and FileAccess.file_exists(demo.path_join("default.project.json")):
		return demo
	# An installed Studio has no demo beside it: the last place still there, or a new one.
	for one in _setting("places", "recent", []):
		if FileAccess.file_exists(String(one).path_join("default.project.json")):
			return String(one)
	var first := ProjectSettings.globalize_path("user://places").path_join("My First Place")
	if not FileAccess.file_exists(first.path_join("default.project.json")):
		_scaffold(first, true)
	return first

# Where the files a place names (meshes/rock.obj, sounds/creak.wav) are rooted. A Rojo project
# sits one directory up from its src/, so that is the guess when --assets does not say.
func _assets_argument(place_dir: String) -> String:
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--assets="):
			return a.get_slice("=", 1)
	var up := place_dir.path_join("..").simplify_path()
	for d in ["meshes", "sounds", "images"]:
		if DirAccess.dir_exists_absolute(up.path_join(d)):
			return up
	return place_dir

# ---- the world --------------------------------------------------------------------
# Opening a place makes a new world: an Instance tree cannot be unbuilt, so the old one goes
# with its node.
func open_place(dir: String) -> void:
	stop()
	if _anim != null and _anim.visible:
		_anim.close()
	_opened = false
	if _editor != null:
		_editor.close_all()
		if _gap != null:
			_gap.visible = true
	_new_world(_assets_argument(dir))
	var n := place.open(world, dir)
	_settle_then_frame()
	_say("studio", "%s -- %d files" % [dir, n], "studio")
	_remember_place(dir)
	_dirty = false
	_refresh_title()
	_load_plugins()

# A fresh edit world: the Studio's own for a place on disk, or -- given a host -- a Client
# world joined to another Studio's place (Team Create), where edits go to the host and come
# back replicated.
func _new_world(assets: String, address := "", port := 0) -> void:
	if world != null:
		remove_child(world)
		world.queue_free()
	_select([])
	world = PulseBlockzWorld.new()
	world.name = "World"
	_give_roblox_cookie(world)
	world.max_millis_per_call = script_timeout * 1000.0
	world.edit_mode = true           # no script starts, nothing falls: a tree to work on
	if address != "":
		world.mode = PulseBlockzWorld.MODE_CLIENT
		world.server_address = address
		world.server_port = port
		world.player_name = _user_name()
		world.user_id = int(_setting("studio", "user_id", randi_range(1000, 999999)))
		_set_setting("studio", "user_id", world.user_id)
		world.auto_join = true       # join the host's session
	else:
		world.auto_join = false      # nobody is playing: no local player, no character
	world.default_controls = false
	world.default_camera = false     # the Studio flies its own
	world.track_properties = true    # the mirror keeps every property, for the panel
	world.gui_preview = _gui_preview  # StarterGui drawn as it would look in game
	if _terrain != null:
		_terrain.world = world
	if _gui_handles != null:
		_gui_handles.world = world
	world.asset_root = assets
	world.script_print.connect(func(n, t): _say(n, t))
	world.script_warn.connect(func(n, t): _say(n, t, "warn"))
	world.script_error.connect(func(n, t): _say(n, t, "error"))
	world.script_killed.connect(_on_killed)
	world.plugin_buttons_changed.connect(_on_plugin_buttons)
	world.plugin_selection_requested.connect(func(ids): _select(Array(ids), true))
	world.plugin_setting_changed.connect(_on_plugin_setting)
	world.plugin_activated.connect(_on_plugin_active)
	world.plugin_open_script.connect(func(id, line): _open_script(id, line))
	world.plugin_edit.connect(_on_plugin_edit)
	world.plugin_waypoint.connect(_on_plugin_waypoint)
	world.server_connected.connect(func(_id): _say("studio", "joined %s:%d -- the host's place is yours to edit with them" % [address, port], "studio"))
	world.server_disconnected.connect(func(): _say("studio", "the host went away; what you see is the last of its place", "warn"))
	_kills.clear()
	add_child(world)
	_explorer.world = world
	_properties.world = world
	_handles.world = world
	history.clear()
	_look_at(world)

# ---- Team Create -------------------------------------------------------------------
# Host lets other Studios join this edit world; Join makes this Studio a guest of another's.
const TEAM_PORT := 8802
var _team_label: Label = null
var _team_joined := ""

func _user_name() -> String:
	var n := str(_setting("studio", "user_name", ""))
	if n == "":
		n = OS.get_environment("USERNAME")
	if n == "":
		n = OS.get_environment("USER")
	return n if n != "" else "Studio"

## Where a Roblox session cookie is read from, in order: the env var, then a file under
## user:// (AppData -- outside the repo, so it cannot be committed). Empty when there is none.
##
## A place's own meshes and textures are private assets: Roblox serves an uploaded mesh only
## to a session with rights to it, so an anonymous fetch answers 401 and a place imports as
## untextured blocks. Signing in as the owner is the only way to read them.
const ROBLOX_COOKIE_ENV := "PBLOCKZ_ROBLOX_COOKIE"
const ROBLOX_COOKIE_FILE := "user://roblox_cookie.txt"

func _roblox_cookie() -> String:
	if OS.has_environment(ROBLOX_COOKIE_ENV):
		var v := OS.get_environment(ROBLOX_COOKIE_ENV).strip_edges()
		if v != "":
			return v
	if FileAccess.file_exists(ROBLOX_COOKIE_FILE):
		var f := FileAccess.open(ROBLOX_COOKIE_FILE, FileAccess.READ)
		if f != null:
			# A Windows editor writes a UTF-8 BOM, and strip_edges() leaves it: U+FEFF is not
			# whitespace. Sent as part of the header value it makes Roblox refuse the cookie.
			return f.get_as_text().lstrip("﻿").strip_edges()
	return ""

## Hands the cookie to a world, and says whether it has one -- never what it is.
func _give_roblox_cookie(w: PulseBlockzWorld) -> void:
	var c := _roblox_cookie()
	if c == "":
		return
	w.set_cloud_cookie(c)
	if not _said_cookie:
		_said_cookie = true
		_say("studio", "signed in to Roblox asset delivery; a place's own meshes can be fetched", "studio")

var _said_cookie := false

func _collaborate_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	var host := Button.new()
	host.text = "Host"
	host.toggle_mode = true
	host.button_pressed = world != null and world.is_hosting()
	host.disabled = playing != null or (world != null and world.collaborating())
	host.tooltip_text = "Team Create: let other Studios join this place over the network, on port %d.\nTheir edits land here and go out to everyone; you save." % TEAM_PORT
	host.toggled.connect(func(on): _host_session(on, host))
	row.add_child(host)
	var addr := LineEdit.new()
	addr.placeholder_text = "host:port"
	addr.text = str(_setting("studio", "team_address", "127.0.0.1:%d" % TEAM_PORT))
	addr.custom_minimum_size.x = 150
	addr.tooltip_text = "The hosting Studio's address"
	row.add_child(addr)
	var join := Button.new()
	join.text = "Join"
	join.disabled = playing != null
	join.tooltip_text = "Join a Team Create session: the host's place appears here, live, and what you change goes to the host"
	join.pressed.connect(func(): _join_session(addr.text))
	row.add_child(join)
	var who := Label.new()
	who.text = _team_text()
	who.add_theme_color_override("font_color", palette.text_dim)
	row.add_child(who)
	_team_label = who
	return row

func _team_text() -> String:
	if world == null:
		return ""
	if world.is_hosting():
		var n: int = world.get_client_count()
		return "hosting, %d here" % n if n != 1 else "hosting, 1 here"
	if world.collaborating():
		return ("in %s" % _team_joined) if world.is_server_connected() else ("joining %s" % _team_joined)
	return ""

func _host_session(on: bool, button: Button) -> void:
	if world == null or world.collaborating():
		return
	if on:
		world.listen(TEAM_PORT, 32)
		_say("studio", "hosting Team Create on port %d: others join with this machine's address" % TEAM_PORT, "studio")
	else:
		# Hosting only ends when the place is reopened, so the toggle goes back on.
		_say("studio", "reopen the place to stop hosting; the guests are let go then", "studio")
		button.set_pressed_no_signal(true)
	if _team_label != null:
		_team_label.text = _team_text()

func _join_session(text: String) -> void:
	var parts := text.strip_edges().rsplit(":", true, 1)
	var address := parts[0] if parts.size() > 0 else ""
	var port := int(parts[1]) if parts.size() > 1 else TEAM_PORT
	if address == "":
		_say("studio", "Join needs the host's address, as host:port", "warn")
		return
	_set_setting("studio", "team_address", text.strip_edges())
	stop()
	if _anim != null and _anim.visible:
		_anim.close()
	if _editor != null:
		_editor.close_all()
	_team_joined = "%s:%d" % [address, port]
	_new_world(_assets_argument(""), address, port)
	_settle_then_frame()
	_say("studio", "joining %s ..." % _team_joined, "studio")
	_dirty = false
	_refresh_title()
	_load_plugins()

# Recent places, newest first, in the settings file. One that has gone is dropped when reached
# for rather than swept at startup: an unplugged drive is not a deletion.
const RECENT_MAX := 8

func _remember_place(dir: String) -> void:
	var list: Array = _setting("places", "recent", [])
	list = list.filter(func(one): return String(one) != dir)
	list.push_front(dir)
	if list.size() > RECENT_MAX:
		list.resize(RECENT_MAX)
	_set_setting("places", "recent", list)
	_fill_recent()

func _forget_place(dir: String) -> void:
	var list: Array = _setting("places", "recent", [])
	_set_setting("places", "recent", list.filter(func(one): return String(one) != dir))
	_fill_recent()

func _fill_recent() -> void:
	if _recent_pop == null:
		return
	_recent_pop.clear()
	var list: Array = _setting("places", "recent", [])
	for i in list.size():
		var one := String(list[i])
		_recent_pop.add_item(one.get_file() if one.get_file() != "" else one, i)
		_recent_pop.set_item_tooltip(i, one)
		_recent_pop.set_item_disabled(i, one == place.dir)
	if list.is_empty():
		_recent_pop.add_item("nothing yet", 0)
		_recent_pop.set_item_disabled(0, true)

func _open_recent(i: int) -> void:
	var list: Array = _setting("places", "recent", [])
	if i < 0 or i >= list.size():
		return
	var dir := String(list[i])
	if dir == place.dir:
		return
	if not DirAccess.dir_exists_absolute(dir):
		_say("studio", "%s is not there any more" % dir, "warn")
		_forget_place(dir)
		return
	_confirm("Opening another place throws away everything not saved. Go on?", func(): open_place(dir))

const AUTOSAVE_SECONDS := 300.0

func set_autosave(on: bool) -> void:
	_autosave_on = on
	_set_setting("studio", "autosave", on)
	if _autosave != null:
		if on:
			_autosave.start(AUTOSAVE_SECONDS)
		else:
			_autosave.stop()
	_refresh_file_menu()

func _autosave_tick() -> void:
	if not _autosave_on or playing != null or not _opened or not unsaved():
		return
	_say("studio", "autosaving", "studio")
	await save_place()

func _refresh_file_menu() -> void:
	if _file_pop == null:
		return
	var at := _file_pop.get_item_index(4)
	if at >= 0:
		_file_pop.set_item_checked(at, _autosave_on)

func live() -> PulseBlockzWorld:
	return playing if playing != null else world

func _process(delta):
	# A ScreenGui is scaled and anchored to the hole the docks leave, not the window, so the
	# rect is re-sent whenever a splitter moves.
	if world != null and _gap != null and playing == null:
		var hole := _gap.get_global_rect()
		if hole.size.x > 1.0 and hole.size.y > 1.0:
			world.gui_preview_rect = hole
	if _gui_handles != null:
		var gid := 0
		if playing == null and _gui_preview and world != null and _selection.size() == 1 and _is_gui(_selection[0]):
			gid = _selection[0]
		_gui_handles.show_for(gid, world.gui_rect(gid) if gid != 0 else Rect2())
	if live() != null:
		var s: Dictionary = live().get_stats()
		_status.text = "%d fps   %d instances   %d parts" % [
			Engine.get_frames_per_second(), s.get("instances", 0), s.get("parts", 0)]
	if _bar != null:
		var what := "nothing selected"
		if _selection.size() == 1:
			what = "%s  (%s)" % [live().get_instance(primary()).get("name", "?"), live().get_instance(primary()).get("class_name", "")]
		elif _selection.size() > 1:
			what = "%d selected" % _selection.size()
		_bar.text = "  %s      %s      snap %s studs / %s deg      fly %d" % [
			"playing" if playing != null else "editing", what,
			("off" if _handles.snap == 0 else str(_handles.snap)),
			("off" if _handles.rotate_snap == 0 else str(int(_handles.rotate_snap))), int(_speed)]
	if live() != null and _editor != null:
		var version: int = live().get_tree_version()
		if version != _tree_seen:
			_tree_seen = version
			_editor.follow()      # a tab whose script has gone should go with it
	_fly(delta)
	_draw_box()

# ---- selection --------------------------------------------------------------------
func primary() -> int:
	return _selection.back() if not _selection.is_empty() else 0

func _select(ids: Array, reveal := false) -> void:
	_selection = ids.duplicate()
	if playing == null and world != null:
		world.plugin_selection(PackedInt64Array(_selection))   # Selection:Get() / SelectionChanged for the plugins
	_properties.show_instances(_selection)
	if _handles != null:
		_handles.target(_selection)
	if reveal:
		_explorer.reveal(_selection)
	_refresh_commands()

func _select_one(id: int, reveal := false) -> void:
	_select([] if id == 0 else [id], reveal)

# Shift adds to what is selected, ctrl takes one out; neither replaces it.
func _picked(hits: Array, add: bool, toggle: bool, reveal := false) -> void:
	var next := _selection.duplicate() if add or toggle else []
	for id in hits:
		if toggle and next.has(id):
			next.erase(id)
		elif not next.has(id):
			next.append(id)
	_select(next, reveal)

# Unshaded lines a hair above y = 0, so they do not z-fight the baseplate.
func _build_grid() -> void:
	_grid = MeshInstance3D.new()
	_grid.mesh = ImmediateMesh.new()
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.vertex_color_use_as_albedo = true
	mat.no_depth_test = false
	_grid.material_override = mat
	_grid.position.y = 0.02
	_grid.visible = false
	add_child(_grid)
	_draw_grid()

func _draw_grid() -> void:
	var mesh: ImmediateMesh = _grid.mesh
	mesh.clear_surfaces()
	mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	var reach := 128
	for i in range(-reach, reach + 1, 4):
		var edge: bool = i % 16 == 0
		var tone: Color = palette.text_dim if edge else palette.text_faint
		tone.a = 0.5 if edge else 0.18
		mesh.surface_set_color(tone)
		mesh.surface_add_vertex(Vector3(i, 0, -reach))
		mesh.surface_set_color(tone)
		mesh.surface_add_vertex(Vector3(i, 0, reach))
		mesh.surface_set_color(tone)
		mesh.surface_add_vertex(Vector3(-reach, 0, i))
		mesh.surface_set_color(tone)
		mesh.surface_add_vertex(Vector3(reach, 0, i))
	mesh.surface_end()

# What the physics collides with, which for a MeshPart is a different shape from what is drawn.
func _show_collision(on: bool) -> void:
	_collision_shown = on
	var w := live()
	if w == null:
		return
	for id in _descendants(_workspace(), []):
		var body := w.get_part_node(id)
		if body == null:
			continue
		for c in body.get_children():
			if c is CollisionShape3D:
				var seen := c.get_node_or_null("StudioOutline") as MeshInstance3D
				if not on:
					if seen != null: seen.queue_free()
					continue
				if seen != null:
					continue
				var shape: Shape3D = c.shape
				if shape == null:
					continue
				var art := MeshInstance3D.new()
				art.name = "StudioOutline"
				art.mesh = shape.get_debug_mesh()
				var mat := StandardMaterial3D.new()
				mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
				mat.albedo_color = Color(0.35, 0.9, 1.0)
				mat.no_depth_test = true
				art.material_override = mat
				c.add_child(art)

func _build_box():
	_box = MeshInstance3D.new()
	_box.mesh = ImmediateMesh.new()
	_box.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.no_depth_test = true
	mat.albedo_color = Color(1, 0.55, 0.1)
	_box.material_override = mat
	_box.visible = false
	add_child(_box)

# Every body under the selection, so picking a Model boxes the parts it holds.
func _outlined() -> Array:
	if _handles == null or live() != world:
		return _selection
	return _handles.bodies()

# The twelve edges of a box, as pairs of AABB.get_endpoint indices.
const EDGES := [[0, 1], [1, 3], [3, 2], [2, 0], [4, 5], [5, 7], [7, 6], [6, 4], [0, 4], [1, 5], [2, 6], [3, 7]]

func _draw_box():
	var mesh: ImmediateMesh = _box.mesh
	mesh.clear_surfaces()
	_box.visible = false
	if live() == null or _selection.is_empty():
		return
	var boxes := []
	for id in _outlined():
		var body := live().get_part_node(id)
		if body == null:
			continue
		var drawn := _drawn_mesh(body, live().get_instance(id).get("name", ""))
		if drawn != null:
			boxes.append([drawn.global_transform, drawn.get_aabb()])
	if boxes.is_empty():
		return
	# Drawn in world space, so one mesh outlines the whole selection.
	_box.global_transform = Transform3D.IDENTITY
	mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	for one in boxes:
		var at: Transform3D = one[0]
		var bounds: AABB = one[1]
		for edge in EDGES:
			mesh.surface_add_vertex(at * bounds.get_endpoint(edge[0]))
			mesh.surface_add_vertex(at * bounds.get_endpoint(edge[1]))
	mesh.surface_end()
	_box.visible = true

func _drawn_mesh(body: Node3D, part_name: String) -> MeshInstance3D:
	var first: MeshInstance3D = null
	for c in body.get_children():
		if c is MeshInstance3D:
			if c.name == part_name:
				return c
			if first == null:
				first = c
	return first

# ---- the camera and the pointer ---------------------------------------------------
func _unhandled_input(e):
	# A plugin that called Activate gets the pointer as its PluginMouse; an exclusive one keeps
	# the left click away from the Studio's own picking.
	if _plugin_active != "" and playing == null and world != null and (e is InputEventMouseButton or e is InputEventMouseMotion):
		world.plugin_input(e, _cam.project_ray_origin(e.position), _cam.project_ray_normal(e.position))
		if _plugin_exclusive and e is InputEventMouseButton and e.button_index == MOUSE_BUTTON_LEFT:
			return
	if playing != null:
		return          # the game has the viewport: its own camera, its own controls
	if e is InputEventMouseButton:
		match e.button_index:
			MOUSE_BUTTON_RIGHT:
				# Held it turns the camera; released within 3 px of where it went down, the menu.
				_looking = e.pressed
				Input.mouse_mode = Input.MOUSE_MODE_CAPTURED if _looking else Input.MOUSE_MODE_VISIBLE
				if e.pressed:
					_right_at = e.position
				elif e.position.distance_to(_right_at) < 3.0:
					_open_menu(e.position)
			MOUSE_BUTTON_WHEEL_UP:
				if e.pressed:
					if e.shift_pressed: _speed = min(_speed * 1.2, 500.0)
					else: _zoom(1.0, e.position)
			MOUSE_BUTTON_WHEEL_DOWN:
				if e.pressed:
					if e.shift_pressed: _speed = max(_speed / 1.2, 1.0)
					else: _zoom(-1.0, e.position)
			MOUSE_BUTTON_MIDDLE:
				_panning = e.pressed
			MOUSE_BUTTON_LEFT:
				if _sculpting:
					# The brush owns the pointer while it is the tool, as Studio's do.
					if e.pressed:
						_terrain.begin(_cam.project_ray_origin(e.position), _cam.project_ray_normal(e.position))
					else:
						_terrain.finish()
					return
				# A GuiObject's handles sit over the view: they take the pointer first.
				if e.pressed and playing == null and _gui_handles != null and _gui_handles.begin(e.position):
					_marquee_armed = false
					return
				if e.pressed:
					var handle := _handles.pick(_cam.project_ray_origin(e.position), _cam.project_ray_normal(e.position))
					# Roblox drags an unselected brick in the same gesture: select it first.
					var grabbed := false
					if handle < 0 and _handles.mode == Handles.Mode.MOVE and _handles.surface \
							and not e.shift_pressed and not e.ctrl_pressed:
						var under := _outermost(_part_under(e.position), e.alt_pressed)
						if under != 0 and not _locked(under):
							if not _selection.has(under):
								_select_one(under, true)
							grabbed = _handles.begin_free(_cam.project_ray_origin(e.position), _cam.project_ray_normal(e.position))
					if grabbed or (handle >= 0 and _handles.begin(handle, _cam.project_ray_origin(e.position), _cam.project_ray_normal(e.position))):
						_marquee_armed = false
					else:
						# Not on a handle: a click, until the pointer moves and it becomes a marquee.
						_marquee_from = e.position
						_marquee_armed = true
						_marquee_on = false
				else:
					if _gui_handles != null and _gui_handles.dragging():
						_gui_handles.finish()
					if _handles.dragging():
						_handles.finish()
					elif _marquee_on:
						_picked(_parts_in(Rect2(_marquee_from, e.position - _marquee_from).abs()), e.shift_pressed, e.ctrl_pressed, true)
					elif _marquee_armed:
						_pick(e.position, e.shift_pressed, e.ctrl_pressed, e.alt_pressed)
					_marquee_armed = false
					_marquee_on = false
					_marquee.visible = false
	elif e is InputEventMouseMotion and _gui_handles != null and _gui_handles.dragging():
		_gui_handles.drag(e.position)
	elif e is InputEventMouseMotion and _sculpting and _terrain.active():
		_terrain.drag(_cam.project_ray_origin(e.position), _cam.project_ray_normal(e.position),
					  get_process_delta_time())
	elif e is InputEventMouseMotion and _handles.dragging():
		_handles.drag(_cam.project_ray_origin(e.position), _cam.project_ray_normal(e.position))
	elif e is InputEventMouseMotion and _marquee_armed:
		if not _marquee_on and e.position.distance_to(_marquee_from) > 4.0:
			_marquee_on = true
			_marquee.visible = true
		if _marquee_on:
			var box := Rect2(_marquee_from, e.position - _marquee_from).abs()
			_marquee.position = box.position
			_marquee.size = box.size
	elif e is InputEventMouseMotion and _panning:
		# The step scales with fly speed: panning a map and a brick move the same on screen.
		var step: float = _dolly() * 0.25
		_cam.global_position -= _cam.global_transform.basis.x * e.relative.x * step
		_cam.global_position += _cam.global_transform.basis.y * e.relative.y * step
	elif e is InputEventMouseMotion and _looking:
		_yaw -= e.relative.x * 0.005
		_pitch = clamp(_pitch - e.relative.y * 0.005, -1.5, 1.5)
		_cam.transform.basis = Basis.from_euler(Vector3(_pitch, _yaw, 0))
	elif e is InputEventKey and e.pressed and not e.echo:
		if e.keycode == KEY_DELETE and not _selection.is_empty():
			_delete()
		elif e.keycode == KEY_ESCAPE:
			_properties.cancel_ref()
			_select([])
		elif e.keycode == KEY_Z and e.ctrl_pressed: _redo() if e.shift_pressed else _undo()
		elif e.keycode == KEY_Y and e.ctrl_pressed: _redo()
		elif e.keycode == KEY_X and e.ctrl_pressed: _do("cut")
		elif e.keycode == KEY_C and e.ctrl_pressed: _do("copy")
		elif e.keycode == KEY_V and e.ctrl_pressed: _do("paste")
		elif e.keycode == KEY_D and e.ctrl_pressed: _do("duplicate")
		elif e.keycode == KEY_G and e.ctrl_pressed: _do("union") if e.shift_pressed else _do("group")
		elif e.keycode == KEY_U and e.ctrl_pressed: _do("separate") if e.shift_pressed else _do("ungroup")
		elif e.keycode == KEY_N and e.ctrl_pressed and e.shift_pressed: _do("negate")
		elif e.keycode == KEY_A and e.ctrl_pressed: _select_all()
		elif e.keycode == KEY_S and e.ctrl_pressed: save_place()
		elif e.keycode == KEY_F and e.ctrl_pressed: _editor.open_find(false)
		elif e.keycode == KEY_H and e.ctrl_pressed: _editor.open_find(true)
		elif e.keycode == KEY_F2: _do("rename")
		elif e.keycode == KEY_F5: _do("dbg_continue")
		elif e.keycode == KEY_F10: _do("dbg_over")
		elif e.keycode == KEY_F11: _do("dbg_out") if e.shift_pressed else _do("dbg_into")
		elif e.keycode == KEY_F: _do("focus")
		elif e.keycode == KEY_1: _tool(Handles.Mode.SELECT)
		elif e.keycode == KEY_2: _tool(Handles.Mode.MOVE)
		elif e.keycode == KEY_3: _tool(Handles.Mode.SCALE)
		elif e.keycode == KEY_4: _tool(Handles.Mode.ROTATE)

func _fly(delta: float):
	if not _looking:
		return
	var dir := Vector3.ZERO
	var b := _cam.global_transform.basis
	if Input.is_key_pressed(KEY_W): dir -= b.z
	if Input.is_key_pressed(KEY_S) and not Input.is_key_pressed(KEY_CTRL): dir += b.z
	if Input.is_key_pressed(KEY_A): dir -= b.x
	if Input.is_key_pressed(KEY_D): dir += b.x
	if Input.is_key_pressed(KEY_E): dir += Vector3.UP
	if Input.is_key_pressed(KEY_Q): dir -= Vector3.UP
	if dir == Vector3.ZERO:
		return
	var hurry := 3.0 if Input.is_key_pressed(KEY_SHIFT) else 1.0
	_cam.global_position += dir.normalized() * _speed * hurry * delta

func _pick(at: Vector2, add := false, toggle := false, alt := false):
	if live() == null:
		return
	var from := _cam.project_ray_origin(at)
	var query := PhysicsRayQueryParameters3D.create(from, from + _cam.project_ray_normal(at) * 4000.0, _space_of(live()))
	# Clicking within 4 px of the last click takes the next thing along the ray, so a part
	# inside another is reachable.
	if at.distance_to(_cycle_at) > 4.0:
		_cycle_past.clear()
	_cycle_at = at
	query.exclude = _cycle_past
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	if hit.is_empty() and not _cycle_past.is_empty():
		_cycle_past.clear()                       # round again from the front
		query.exclude = []
		hit = get_world_3d().direct_space_state.intersect_ray(query)
	var id: int = 0 if hit.is_empty() else live().get_part_id(hit.collider)
	if not hit.is_empty():
		_cycle_past.append(hit.rid)
	if id != 0 and _locked(id):
		id = 0
	# A Ref property is waiting to be pointed at something: this click is its answer.
	if id != 0 and _properties.waiting_for_ref() and _properties.pointed_at(id):
		return
	id = _outermost(id, alt)
	# A previewed ScreenGui is drawn over the place, so it beats anything the ray reached.
	if playing == null and _gui_preview and world != null:
		var on_gui: int = world.gui_at(at)
		if on_gui != 0:
			id = on_gui
	_picked([] if id == 0 else [id], add, toggle, true)

# Roblox selects the outermost Model a part sits in; Alt reaches past it to the part itself.
func _outermost(part: int, alt: bool) -> int:
	if part == 0 or alt or live() == null:
		return part
	var best := part
	var at := part
	for hop in 16:
		var info := live().get_instance(at)
		if info.is_empty():
			break
		var up := int(info.get("parent", 0))
		if up == 0:
			break
		if live().get_instance(up).get("class_name", "") == "Model":
			best = up
		at = up
	return best

func _part_under(at: Vector2) -> int:
	if live() == null:
		return 0
	var from := _cam.project_ray_origin(at)
	var query := PhysicsRayQueryParameters3D.create(from, from + _cam.project_ray_normal(at) * 4000.0, _space_of(live()))
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	return 0 if hit.is_empty() else live().get_part_id(hit.collider)

# The collision mask for one world's bodies: the edit and played worlds share a space, and a
# ray must reach only the one being looked at.
static func _space_of(w: PulseBlockzWorld) -> int:
	return 1 << (w.physics_layer - 1) if w != null else 0xFFFFFFFF

# Roblox's Locked: a click cannot land on it, but the Explorer still reaches it.
func _locked(id: int) -> bool:
	for p in live().get_properties(id):
		if p.name == "Locked":
			return bool(p.value)
	return false

# Workspace parts whose eight projected corners give a screen rect meeting the box, so any
# part of one inside counts.
func _parts_in(box: Rect2) -> Array:
	var w := live()
	var found := []
	if w == null or box.size.x < 2 or box.size.y < 2:
		return found
	for id in _descendants(_workspace(), []):
		var body := w.get_part_node(id)
		if body == null:
			continue
		var drawn := _drawn_mesh(body, w.get_instance(id).get("name", ""))
		if drawn == null:
			continue
		var bounds := drawn.get_aabb()
		var on_screen := Rect2()
		var seen := false
		for corner in 8:
			var at: Vector3 = drawn.global_transform * bounds.get_endpoint(corner)
			if _cam.is_position_behind(at):
				continue
			var pixel := _cam.unproject_position(at)
			on_screen = Rect2(pixel, Vector2.ZERO) if not seen else on_screen.expand(pixel)
			seen = true
		if seen and box.intersects(on_screen) and not _locked(id):
			found.append(id)
	return found

func _descendants(id: int, out: Array, depth := 0) -> Array:
	if id == 0 or depth > 12:
		return out
	for c in live().get_child_ids(id):
		out.append(c)
		_descendants(c, out, depth + 1)
	return out

# ---- what the toolbar does --------------------------------------------------------
func _open_dialog():
	var dlg := FileDialog.new()
	dlg.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	dlg.access = FileDialog.ACCESS_FILESYSTEM
	dlg.use_native_dialog = true
	dlg.current_dir = place.dir
	dlg.dir_selected.connect(func(d): _confirm("This place has changes that are not saved. Open another anyway?", func(): open_place(d)))
	add_child(dlg)
	dlg.popup_centered_ratio(0.7)

# ---- one model, in and out ---------------------------------------------------------
# How a build travels between places: the loader reads *.rbxmx and *.rbxm, to_rbxmx writes XML.
func insert_from_file() -> void:
	var dlg := FileDialog.new()
	dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	dlg.access = FileDialog.ACCESS_FILESYSTEM
	dlg.use_native_dialog = true
	dlg.current_dir = place.dir
	dlg.filters = PackedStringArray(["*.rbxmx, *.rbxm, *.model.json ; Model files"])
	dlg.file_selected.connect(func(f): insert_file(f))
	add_child(dlg)
	dlg.popup_centered_ratio(0.7)

# Under the selection, or the Workspace. The loader takes text, so a binary .rbxm goes in as
# the base64 of its bytes.
func insert_file(from: String) -> void:
	if world == null or playing != null:
		return
	var parent := primary() if primary() != 0 else _workspace()
	if parent == 0:
		return
	var name := from.get_file().get_basename()
	if name.ends_with(".model"):
		name = name.get_basename()
	var text := ""
	if from.ends_with(".rbxm"):
		text = Marshalls.raw_to_base64(FileAccess.get_file_as_bytes(from))
	else:
		text = FileAccess.get_file_as_string(from)
	if text == "":
		_say("studio", "%s is empty, or could not be read" % from.get_file(), "error")
		return
	# The loader is fed by path, so the suffix is what says which reader runs.
	var suffix := ".rbxm" if from.ends_with(".rbxm") else ".rbxmx" if from.ends_with(".rbxmx") else ".model.json"
	var at := _path_of(parent)
	var was := {}
	for c in world.get_child_ids(parent):
		was[c] = true
	world.load_file("%s/%s%s" % [at, name, suffix] if at != "" else "%s%s" % [name, suffix], text)
	var made := await _appeared_under(parent, was)
	if made == 0:
		_say("studio", "nothing in %s the loader could read" % from.get_file(), "warn")
		return
	history.record_added(made, parent, name)
	_select_one(made)
	_say("studio", "inserted %s from %s" % [name, from.get_file()], "studio")

func save_selection_to_file() -> void:
	_prune()
	if _selection.size() != 1:
		_say("studio", "pick one thing to save out", "warn")
		return
	var dlg := FileDialog.new()
	dlg.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	dlg.access = FileDialog.ACCESS_FILESYSTEM
	dlg.use_native_dialog = true
	dlg.current_dir = place.dir
	dlg.current_file = "%s.rbxmx" % live().get_instance(primary()).get("name", "Model")
	dlg.filters = PackedStringArray(["*.rbxmx ; Roblox model", "*.model.json ; Rojo model"])
	dlg.file_selected.connect(func(f): write_selection(f))
	add_child(dlg)
	dlg.popup_centered_ratio(0.7)

func write_selection(to: String) -> void:
	if _selection.size() != 1:
		return
	var id: int = primary()
	var name: String = live().get_instance(id).get("name", "Model")
	var text := Model.of(live(), id)
	if not to.ends_with(".model.json"):
		if not to.ends_with(".rbxmx"):
			to += ".rbxmx"
		text = live().to_rbxmx(text, name)
		if text == "":
			_say("studio", "could not write %s" % to.get_file(), "error")
			return
	var f := FileAccess.open(to, FileAccess.WRITE)
	if f == null:
		_say("studio", "could not open %s to write" % to, "error")
		return
	f.store_string(text)
	_say("studio", "saved %s to %s" % [name, to.get_file()], "studio")

# The child of `parent` that was not there before, waiting up to 120 frames for the loader.
func _appeared_under(parent: int, before: Dictionary) -> int:
	for wait in 120:
		await get_tree().process_frame
		for c in world.get_child_ids(parent):
			if not before.has(c):
				return c
	return 0

func _path_of(id: int) -> String:
	if id == 0:
		return ""
	var back := []
	var at := id
	for hop in 32:
		var info := world.get_instance(at)
		if info.is_empty():
			break
		back.append(str(info.get("name", "")))
		at = int(info.get("parent", 0))
		if at == 0:
			break
	back.reverse()
	return "/".join(back)

# ---- a place of your own -------------------------------------------------------------
const NEW_PROJECT := '{
  "name": "%s",
  "tree": {
    "$className": "DataModel",
    "ReplicatedStorage": { "$path": "src/shared" },
    "ServerScriptService": { "$path": "src/server" },
    "StarterPlayer": { "StarterPlayerScripts": { "$path": "src/client" } },
    "Workspace": { "$path": "src/workspace" }
  }
}
'

func new_place_dialog() -> void:
	var dlg := FileDialog.new()
	dlg.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	dlg.access = FileDialog.ACCESS_FILESYSTEM
	dlg.use_native_dialog = true
	dlg.title = "An empty directory for the new place"
	dlg.dir_selected.connect(func(d): _confirm("This place has changes that are not saved. Start a new one anyway?", func(): new_place(d)))
	add_child(dlg)
	dlg.popup_centered_ratio(0.7)

func new_place(dir: String) -> void:
	if not _scaffold(dir, true):
		return
	open_place(dir)

# A Rojo project in an empty directory: the project file, the four directories it names, and
# -- when `starter` -- the baseplate and spawn every Roblox place begins with. An import
# brings its own.
func _scaffold(dir: String, starter: bool) -> bool:
	if FileAccess.file_exists(dir.path_join("default.project.json")):
		_say("studio", "%s already holds a place: open it instead" % dir, "warn")
		return false
	for one in ["src/shared", "src/server", "src/client", "src/workspace"]:
		DirAccess.make_dir_recursive_absolute(dir.path_join(one))
	var f := FileAccess.open(dir.path_join("default.project.json"), FileAccess.WRITE)
	if f == null:
		_say("studio", "could not write a project file into %s" % dir, "error")
		return false
	f.store_string(NEW_PROJECT % dir.get_file())
	f = null
	if not starter:
		return true
	var ground := FileAccess.open(dir.path_join("src/workspace/Baseplate.model.json"), FileAccess.WRITE)
	ground.store_string('{"className":"Part","properties":{"Anchored":true,"Size":[512,20,512],' \
		+ '"Position":[0,-10,0],"Color":{"Color3uint8":[91,93,105]},"Material":"Concrete","Locked":true}}\n')
	ground = null
	var spawn := FileAccess.open(dir.path_join("src/workspace/SpawnLocation.model.json"), FileAccess.WRITE)
	spawn.store_string('{"className":"SpawnLocation","properties":{"Anchored":true,"Size":[12,1,12],' \
		+ '"Position":[0,0.5,0],"Color":{"Color3uint8":[62,124,66]}}}\n')
	spawn = null
	return true

# The whole place as a file Roblox Studio opens. The Rojo project stays the place's home.
func save_rbxlx_dialog() -> void:
	var dlg := FileDialog.new()
	dlg.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	dlg.access = FileDialog.ACCESS_FILESYSTEM
	dlg.use_native_dialog = true
	dlg.current_dir = place.dir
	dlg.current_file = "%s.rbxlx" % place.dir.get_file()
	dlg.filters = PackedStringArray(["*.rbxlx ; Roblox place"])
	dlg.file_selected.connect(func(f): save_rbxlx(f))
	add_child(dlg)
	dlg.popup_centered_ratio(0.7)

func save_rbxlx(to: String) -> bool:
	if not to.ends_with(".rbxlx"):
		to += ".rbxlx"
	if _editor != null:
		_editor.commit()
	if place.save_rbxlx(world, to):
		var terrain_note := ""
		var terrain_info: Dictionary = world.get_terrain_info()
		if int(terrain_info.get("resolution", 0)) > 0:
			terrain_note = "; the terrain is not in it (Roblox's voxel format is its own)"
		_say("studio", "saved the place as %s: Roblox Studio's File > Open takes it%s" % [to.get_file(), terrain_note], "studio")
		return true
	_say("studio", "could not write %s" % to.get_file(), "error")
	return false

# ---- a place from Roblox -------------------------------------------------------------
# An empty Rojo project is laid down, the .rbxlx or .rbxl's services merged into it, and
# Ctrl+S writes the lot out as model.json and .luau. Terrain does not come across.
func import_place_dialog() -> void:
	var dlg := FileDialog.new()
	dlg.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	dlg.access = FileDialog.ACCESS_FILESYSTEM
	dlg.use_native_dialog = true
	dlg.filters = PackedStringArray(["*.rbxlx, *.rbxl ; Roblox place"])
	dlg.file_selected.connect(func(f):
		var where := FileDialog.new()
		where.file_mode = FileDialog.FILE_MODE_OPEN_DIR
		where.access = FileDialog.ACCESS_FILESYSTEM
		where.use_native_dialog = true
		where.title = "An empty directory to keep the place in"
		where.dir_selected.connect(func(d): _confirm("This place has changes that are not saved. Open another anyway?", func(): import_place(f, d)))
		add_child(where)
		where.popup_centered_ratio(0.7))
	add_child(dlg)
	dlg.popup_centered_ratio(0.7)

func import_place(from: String, dir: String) -> void:
	if not _scaffold(dir, false):
		return
	open_place(dir)
	for wait in 300:
		await get_tree().process_frame
		if _opened:
			break
	var bytes := FileAccess.get_file_as_bytes(from)
	if bytes.is_empty():
		_say("studio", "%s is empty, or could not be read" % from.get_file(), "error")
		return
	world.import_place(bytes)
	await _settle_frames(30)
	_settle_then_frame()
	_dirty = true
	_refresh_title()
	_say("studio", "opened %s: Ctrl+S writes it into %s as a Rojo project" % [from.get_file(), dir.get_file()], "studio")

# Anchored, 20 studs in front of the camera, as Studio's Part button leaves one.
func _insert_part():
	var w := live()
	if w == null:
		return
	var parent := primary() if primary() != 0 else _workspace()
	if parent == 0:
		return
	var made := await history.insert("Part", parent)
	if made == 0:
		return
	# Not a second undo entry: the insert's entry keeps the whole instance, these with it.
	var at := _cam.global_position - _cam.global_transform.basis.z * 20.0
	w.set_property(made, "Anchored", true)
	w.set_property(made, "Position", at.round())
	_select_one(made, true)

# Built by a Luau chunk in the edit world, as Roblox's Rig Builder is, then recorded as one
# insert for Ctrl+Z.
func _build_rig(r15: bool) -> void:
	var w := live()
	if w == null or playing != null:
		return
	var ws := _workspace()
	if ws == 0:
		return
	var at := (_cam.global_position - _cam.global_transform.basis.z * 20.0).round()
	at.y = maxf(at.y, 3.2 if r15 else 3.0)   # its feet no lower than the ground plane
	var was := {}
	for c in w.get_child_ids(ws):
		was[c] = true
	w.run_chunk("rigbuilder", RigBuilderScript.chunk(r15, at))
	var made := await _appeared_under(ws, was)
	if made == 0:
		_say("studio", "the rig did not appear", "warn")
		return
	if r15:   # read-only to scripts, as in Roblox; the Studio sets it
		for kid in w.get_child_ids(made):
			if w.get_instance(kid).get("class_name", "") == "Humanoid":
				w.set_property(kid, "RigType", "R15")
	history.record_added(made, ws, "Rig")
	_select_one(made, true)
	_say("studio", "built an %s block rig" % ("R15" if r15 else "R6"), "studio")

# ---- Play and Stop ----------------------------------------------------------------
# Play builds a second world, copies the tree into it as the *.model.json text Save writes,
# and joins it as a player; Stop throws that world away. The edit world is untouched either
# way, so a build in progress survives.
const NOT_COPIED := ["Players", "PluginDebugService", "CoreGui"]     # whoever is playing, and the Studio's plugins: not the place

# The runtime makes one of each for every world, so Save skips them -- unless the Terrain
# holds sculpted Heights, or the place already keeps a file for one.
const NOT_SAVED := ["Camera", "Terrain"]

func _authored(id: int, kind: String, path: String) -> bool:
	if not (kind in NOT_SAVED):
		return true
	if kind == "Terrain":
		for p in world.get_properties(id, true):
			if p.name == "Heights" and str(p.value) != "":
				return true
	return place.source_suffix(path) != ""

# Run is Play with nobody in it: the place runs and the Studio keeps its camera. Play Here
# starts the character where the camera is looking.
func run() -> void:
	play(false)

func play_here() -> void:
	play(true, true)

func play(joined := true, here := false) -> void:
	_no_focus(self)
	get_viewport().gui_release_focus()
	if _anim != null and _anim.visible:
		_anim.close()       # the rig goes back to how the place has it before the copy is taken
	if playing != null or world == null:
		return
	if _editor != null:
		_editor.commit()      # uncommitted edits go into the copy that runs
	playing = PulseBlockzWorld.new()
	playing.name = "Play"
	_give_roblox_cookie(playing)
	_hear_place_assets(playing)   # before it builds anything: its parts ask as they arrive
	playing.max_millis_per_call = script_timeout * 1000.0
	playing.player_name = OS.get_environment("USERNAME") if OS.has_environment("USERNAME") else "Player"
	playing.asset_root = world.asset_root
	# Its own physics layer: the edit world is only hidden while a place plays, and its
	# bodies would otherwise be ghosts the played character walks into.
	playing.physics_layer = 2 if world.physics_layer != 2 else 3
	playing.track_properties = true      # the panels follow the running place
	playing.auto_join = joined
	playing.default_camera = joined      # nobody playing: keep the Studio's camera
	playing.default_controls = joined
	playing.script_print.connect(func(n, t): _say(n, t))
	playing.script_warn.connect(func(n, t): _say(n, t, "warn"))
	playing.script_error.connect(func(n, t): _say(n, t, "error"))
	playing.script_killed.connect(_on_killed)
	_kills.clear()
	if joined and _gap != null:
		_play_view = SubViewportContainer.new()
		_play_view.name = "PlayView"
		_play_view.stretch = true
		_play_view.set_anchors_preset(Control.PRESET_FULL_RECT)
		_play_view.mouse_filter = Control.MOUSE_FILTER_STOP
		_play_view.focus_mode = Control.FOCUS_ALL
		_play_vp = SubViewport.new()
		_play_vp.name = "PlayViewport"
		_play_vp.own_world_3d = true                 # its own space: the edit world's bodies are not in it
		_play_vp.handle_input_locally = true
		_play_vp.physics_object_picking = false
		_play_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		_play_vp.audio_listener_enable_3d = true
		_play_view.add_child(_play_vp)
		_gap.add_child(_play_view)
		_play_vp.add_child(playing)
		_play_view.grab_focus()
	else:
		add_child(playing)
	if _editor != null:
		await _editor.settled()     # the commit reaches Source before the copy reads it
		if playing == null:
			return                  # Stop landed during the await
	_copy_tree(world, playing)
	_stage_place_assets(playing, place.dir)
	# By full name, not id: an id from the edit world names nothing in the copy.
	for full in _breakpoints:
		for line in _breakpoints[full]:
			playing.set_breakpoint(full, int(line), true)
	playing.script_paused.connect(_on_script_paused)
	playing.script_resumed.connect(_on_script_resumed)
	world.hide()
	world.gui_preview = false          # a CanvasLayer is not hidden with its Node3D parent
	world.set_process(false)
	world.set_physics_process(false)
	_look_at(playing)
	_was_looking = _cam.global_transform
	if here:
		_start_at_camera()
	if joined and _test_players > 0:
		_seat_test_players(_test_players)
	_say("studio", ("playing" if joined else "running") + " %s" % place.dir.get_file(), "studio")

# ---- the place's own assets -------------------------------------------------------
# A published place fetches its fonts, sounds and meshes from the chain with the client's
# wallet; the Studio has none. Instead a place maps asset name -> path in place-assets.json
# beside its default.project.json, and this stages each file where the engine can open it and
# builds the ReplicatedStorage.PlaceAssets folder of StringValues the wallet would have.
const PLACE_ASSETS := "place-assets.json"
const PLACE_ASSET_DIR := "user://place-assets"
# Staged files by content hash: a model names its mesh by a pblockz:// uri, which is a
# content hash, and the engine asks its host for each one it draws (asset_wanted).
var _place_hashes := {}
var _place_asks: Array[String] = []     # asked before the manifest was staged; answered after
var _place_assets_ready := false

func _on_place_asset_wanted(uri: String, _kind: String) -> void:
	if not _place_assets_ready:
		_place_asks.append(uri)
		return
	_answer_place_asset(uri)

func _answer_place_asset(uri: String) -> void:
	if playing == null or not is_instance_valid(playing):
		return
	var p: Dictionary = PulseBlockzChain.parse_asset_uri(uri)
	var hash := String(p.get("content_hash", "")).trim_prefix("0x").to_lower() if p.get("ok", false) else ""
	var path := String(_place_hashes.get(hash, ""))
	if path == "":
		_say("studio", "the place asks for %s, which %s does not have" % [uri, PLACE_ASSETS], "warn")
	playing.asset_arrived(uri, path)

## Connected before the world builds anything: its parts ask for their assets in the first
## frames, and an ask with nothing listening is lost.
func _hear_place_assets(w: PulseBlockzWorld) -> void:
	_place_hashes = {}
	_place_asks = []
	_place_assets_ready = false
	if w != null and not w.asset_wanted.is_connected(_on_place_asset_wanted):
		w.asset_wanted.connect(_on_place_asset_wanted)

func _stage_place_assets(w: PulseBlockzWorld, place_dir: String) -> int:
	var staged := await _stage_place_files(w, place_dir)
	_place_assets_ready = true
	var asks := _place_asks
	_place_asks = []
	for uri in asks:
		_answer_place_asset(uri)
	return staged

func _stage_place_files(w: PulseBlockzWorld, place_dir: String) -> int:
	if w == null or place_dir == "":
		return 0
	var manifest_path := place_dir.path_join(PLACE_ASSETS)
	if not FileAccess.file_exists(manifest_path):
		return 0
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(manifest_path))
	if typeof(parsed) != TYPE_DICTIONARY:
		_say("studio", "%s is not an object" % PLACE_ASSETS, "warn")
		return 0
	DirAccess.make_dir_recursive_absolute(PLACE_ASSET_DIR)
	# A chunk handed to a world with no tree yet has no ReplicatedStorage and goes nowhere.
	for wait in 600:
		if w == null or not is_instance_valid(w):
			return 0
		if not w.get_child_ids(0).is_empty():
			break
		await get_tree().process_frame
	if w == null or not is_instance_valid(w) or w != playing:
		return 0
	w.run_chunk("placeassets", """
local rs = game:GetService("ReplicatedStorage")
if not rs:FindFirstChild("PlaceAssets") then
	local f = Instance.new("Folder")
	f.Name = "PlaceAssets"
	f.Parent = rs
end
""")
	var staged := 0
	var missing: Array[String] = []
	for name in parsed:
		# Keys starting "_" are notes to whoever reads the file, as the wallet's are.
		if String(name).begins_with("_"):
			continue
		var src := place_dir.path_join(String(parsed[name])).simplify_path()
		if not FileAccess.file_exists(src):
			missing.append(String(name))
			continue
		# Copied under user:// because the engine prefixes its asset root onto any path that
		# is not res:// or user://. Named for the asset, not the file, so two assets sharing
		# a filename cannot overwrite each other.
		var dst := "%s/%s.%s" % [PLACE_ASSET_DIR, name, src.get_extension()]
		var w_file := FileAccess.open(dst, FileAccess.WRITE)
		if w_file == null:
			missing.append(String(name))
			continue
		var bytes := FileAccess.get_file_as_bytes(src)
		w_file.store_buffer(bytes)
		w_file.close()
		_place_hashes[String(PulseBlockzChain.content_hash_of(bytes)).trim_prefix("0x").to_lower()] = dst
		w.run_chunk("placeasset_%s" % name, """
local rs = game:GetService("ReplicatedStorage")
local folder = rs:WaitForChild("PlaceAssets")
local v = folder:FindFirstChild("%s")
if not v then
	v = Instance.new("StringValue")
	v.Name = "%s"
	v.Parent = folder
end
v.Value = "%s"
""" % [name, name, dst])
		staged += 1
	if staged > 0:
		_say("studio", "%d place asset%s off disk" % [staged, "" if staged == 1 else "s"], "studio")
	if not missing.is_empty():
		_say("studio", "%s names %d asset%s that are not on disk: %s"
			% [PLACE_ASSETS, missing.size(), "" if missing.size() == 1 else "s",
			   ", ".join(missing)], "warn")
	return staged

# The server half of Roblox's "Clients and Servers": real Players with characters, firing
# PlayerAdded, joining Teams and receiving RemoteEvents, but nobody driving them and no
# LocalScripts. Seated after the local join lands -- the first player in a Play Solo world
# takes the local seat.
func _seat_test_players(count: int) -> void:
	for wait in 600:
		await get_tree().process_frame
		if playing == null:
			return
		if playing.get_local_player_id() != 0:
			break
	if playing == null:
		return
	for i in count:
		playing.add_player("Player%d" % (i + 2), 0)
	_say("studio", "seated %d test player%s beside you" % [count, "" if count == 1 else "s"], "studio")

# Put the character where the camera is, once the place has made one.
func _start_at_camera() -> void:
	var at := _cam.global_position - _cam.global_transform.basis.z * 6.0
	for wait in 240:
		await get_tree().process_frame
		if playing == null:
			return
		var who := playing.get_local_character_id()
		if who == 0:
			continue
		for part in playing.get_child_ids(who):
			if playing.get_instance(part).get("name", "") == "HumanoidRootPart":
				playing.set_property(part, "Position", at)
				return
		return

func stop() -> void:
	if _debugger != null:
		_debug_paused = false
		_debugger.clear()
		_debugger.visible = false
		if _editor != null:
			_editor.clear_executing()
	if playing == null:
		return
	if _play_view != null:
		if playing.get_parent() == _play_vp:
			_play_vp.remove_child(playing)
		_play_view.queue_free()
		_play_view = null
		_play_vp = null
	elif playing.get_parent() == self:
		remove_child(playing)
	playing.queue_free()
	playing = null
	world.show()
	world.gui_preview = _gui_preview
	world.set_process(true)
	world.set_physics_process(true)
	_cam.current = true
	if _was_looking != Transform3D():
		_cam.global_transform = _was_looking      # back where the camera was before Play
		_yaw = _cam.rotation.y
		_pitch = _cam.rotation.x
	_look_at(world)
	# A place that captured the pointer is gone; nothing else would ever release it.
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_looking = false
	_say("studio", "stopped: the place is as you left it", "studio")

# Every service's children and its own non-default properties, into a fresh world. A Ref
# pointing outside the subtree it is written with cannot be named in the file, and is left
# for the place's scripts to make again.
func _copy_tree(from: PulseBlockzWorld, to: PulseBlockzWorld) -> void:
	for sid in from.get_child_ids(0):
		var service := from.get_instance(sid)
		if service.class_name in NOT_COPIED:
			continue
		for p in from.get_properties(sid):
			if not p.is_default and not p.read_only and p.name != "Name":
				to.set_property(_service_of(to, service.class_name), p.name, p.value)
		for child in from.get_child_ids(sid):
			var kind: String = from.get_instance(child).get("class_name", "")
			if kind == "Terrain":
				# Every world makes its own Terrain: only the sculpted ground crosses.
				for p in from.get_properties(child, true):
					if (p.name == "Heights" or p.name == "MaterialColors") and str(p.value) != "":
						to.set_property(to.get_terrain_id(), p.name, p.value)
				continue
			to.add_model(service.name, from.get_instance(child).get("name", ""), Model.of(from, child))

func _service_of(of_world: PulseBlockzWorld, class_name_: String) -> int:
	for id in of_world.get_child_ids(0):
		if of_world.get_instance(id).class_name == class_name_:
			return id
	return 0

func _look_at(which: PulseBlockzWorld) -> void:
	_select([])
	_explorer.world = which
	_explorer._version = -1
	_properties.world = which
	_handles.world = which
	history.world = which
	history.recording = which == world     # what a played copy does is not an edit
	# Not _history_changed(): looking elsewhere is not an edit, and must not mark it dirty.
	_refresh_title()
	_refresh_commands()

func _history_changed() -> void:
	_dirty = true
	_refresh_title()
	_refresh_commands()

func _undo() -> void:
	if playing != null:
		return
	await history.undo()
	_after_history()

func _redo() -> void:
	if playing != null:
		return
	await history.redo()
	_after_history()

# What was selected may have just been undone out of existence.
func _after_history() -> void:
	var alive := []
	for id in _selection:
		if not world.get_instance(id).is_empty():
			alive.append(id)
	if alive.size() != _selection.size():
		_select(alive)
	else:
		_handles.target(_selection)   # what is under a Model may have changed
	_properties.refresh()
	if _editor != null:
		_editor.follow()

# ---- Save -------------------------------------------------------------------------
# The tree back to disk as *.model.json. What already has a file of another kind -- a
# Script's .luau, a model saved as .rbxmx -- is written in that form rather than overwritten.
# What no mounted directory covers is named in the Output instead.
## Publishing is the Publisher's: it holds the wallet and the Studio holds none. The place is
## saved first, and the Publisher opens on its folder. From a checkout, the publisher project
## beside this one run by this same Godot; from an export, the PulseBlockz Publisher
## executable beside this one. Returns the pid, or 0.
func open_publisher() -> int:
	if place.dir == "":
		return 0
	if unsaved():
		save_place()
	var dir := ProjectSettings.globalize_path(place.dir)
	var args := PackedStringArray()
	var exe := OS.get_executable_path()
	var project := ProjectSettings.globalize_path("res://").path_join("../publisher").simplify_path()
	if not OS.has_feature("template") and FileAccess.file_exists(project.path_join("project.godot")):
		args.append_array(["--path", project])
	else:
		var beside := exe.get_base_dir().path_join("PulseBlockz Publisher.exe" if OS.get_name() == "Windows" else "PulseBlockz Publisher")
		if not FileAccess.file_exists(beside):
			_say("studio", "No Publisher found beside the Studio (%s)" % beside, "warn")
			return 0
		exe = beside
	args.append_array(["--", "--dir", dir])
	var pid := OS.create_process(exe, args)
	if pid <= 0:
		_say("studio", "The Publisher did not start", "warn")
		return 0
	_say("studio", "opened the Publisher on %s" % dir, "studio")
	return pid

func save_place() -> void:
	if _editor != null:
		_editor.commit()
		await _editor.settled()     # the same, before the files are written
	if not _opened:
		# Half a tree looks like a place mostly deleted: the sweep below would take its files.
		_say("studio", "the place is still opening: nothing was saved", "warn")
		return
	if playing != null:
		_say("studio", "stop before saving: what is playing is a copy, not the place", "warn")
		return
	place.changed.clear()
	var note := {"written": {}, "seen": {}, "failed": [], "homeless": [], "kept": [], "made": []}
	var written: Dictionary = note.written
	var failed: Array = note.failed
	var homeless: Array = note.homeless
	var kept: Array = note.kept
	for sid in world.get_child_ids(0):
		var service := world.get_instance(sid)
		if service.class_name in NOT_COPIED:
			continue
		for child in world.get_child_ids(sid):
			var kid := world.get_instance(child)
			var kid_path := "%s/%s" % [service.name, kid.get("name", "")]
			# Skipping one the place keeps a file for would leave it to the sweep.
			if not _authored(child, kid.get("class_name", ""), kid_path):
				continue
			_save_subtree(child, kid_path, note)
	# A file under a mount that no instance answers for was deleted -- but a Save that could
	# not write everything cannot tell those from the ones it failed to write, so it sweeps
	# nothing at all.
	var swept := place.sweep(written) if failed.is_empty() else []
	if failed.is_empty():
		_dirty = false          # what Save tried and could not write is still unsaved
	_refresh_title()
	_say("studio", "saved %d into %s%s" % [written.size(), place.dir.get_file(),
		"" if place.changed.size() == written.size()
		else " (%d changed; the rest already said it)" % place.changed.size()], "studio")
	if not swept.is_empty():
		var names := []
		for path in swept:
			names.append(str(path).get_file())
		_say("studio", "removed %d file%s for what is no longer there: %s"
			% [swept.size(), "" if swept.size() == 1 else "s", ", ".join(names)], "print")
	if not kept.is_empty():
		_say("studio", "left as they are (authored in another form): %s" % ", ".join(kept), "print")
	if not note.made.is_empty():
		_say("studio", "the project had nowhere to keep %s, so default.project.json now points %s"
			% ["these" if note.made.size() > 1 else "one of these", ", ".join(note.made)], "print")
	if not homeless.is_empty():
		_say("studio", "no directory in the project maps to these, so they were not saved: %s"
			% ", ".join(homeless), "warn")
	if not failed.is_empty():
		_say("studio", "these could not be written, and the place is still unsaved: %s"
			% ", ".join(failed), "error")

func _save_subtree(id: int, path: String, note: Dictionary) -> void:
	var written: Dictionary = note.written
	var seen: Dictionary = note.seen
	var failed: Array = note.failed
	var homeless: Array = note.homeless
	var kept: Array = note.kept
	# Roblox lets siblings share a name; a directory does not, and the second would land on
	# the first one's file. Refused rather than lost on the next open.
	if seen.has(path):
		failed.append(path + " (a sibling of that name already has the file)")
		return
	seen[path] = true
	# A mount, or any instance with files inside: its children are the files. One model for
	# the lot would swallow the scripts under it and orphan their .luau.
	if place.mounts.has(path) or place.holds_files(path):
		var own := place.source_suffix(path)
		if own.ends_with(".meta.json"):
			kept.append(path)     # its own properties live in that meta.json, which Save does not write
		elif own.begins_with("/init."):
			# A script with children: the directory is the instance, the init.*
			# inside it its text. Written before descending, or the Source is lost.
			var wrote := place.save_script(world, id, path)
			if wrote != "":
				written[wrote] = true
		for child in world.get_child_ids(id):
			_save_subtree(child, "%s/%s" % [path, world.get_instance(child).get("name", "")], note)
		return
	var suffix := place.source_suffix(path)
	var kind: String = world.get_instance(id).get("class_name", "")
	if Place.SCRIPT_FILE.has(kind) and (suffix == "" or Place._owned_suffix(suffix) != ""):
		# A script belongs in the .luau it came from -- but one authored as an .rbxmx or
		# .rbxm is a model file, and raw Lua written over it would destroy it.
		var luau := place.save_script(world, id, path)
		if luau == "" and _mount_for(path, note):
			luau = place.save_script(world, id, path)
		if luau == "":
			# Nowhere to put it even now, or its name is not one a file can hold.
			(homeless if place.file_for(path) == "" else failed).append(path)
		else:
			written[luau] = true
		return
	if suffix == ".rbxmx":
		# Back as the same kind of file: a model.json beside it would leave two files
		# claiming the same instance.
		var xml := place.save_rbxmx(world, id, path)
		if xml == "":
			kept.append(path)     # could not be written: the file is left as it is
		else:
			written[xml] = true
		return
	if suffix != "" and suffix != ".model.json":
		kept.append(path)
		return
	var file := place.save(world, id, path)
	if file == "" and _mount_for(path, note):
		file = place.save(world, id, path)
	if file == "":
		(homeless if place.file_for(path) == "" else failed).append(path)
	else:
		written[file] = true

# Give the service a directory and add it to the project file. Only when there is no mount at
# all: a write that failed for any other reason must not rearrange the project.
func _mount_for(path: String, note: Dictionary) -> bool:
	if place.file_for(path) != "":
		return false
	var made := place.make_mount(path)
	if made == "":
		return false
	note.made.append("%s at %s" % [path.get_slice("/", 0), made.get_file()])
	return true

# ---- the verbs ---------------------------------------------------------------------
# One table of what each verb is, whether it can run now and what key does it, so the
# toolbar, the menus and the keyboard cannot drift apart. Roblox's shortcuts throughout.
func actions() -> Array:
	var some := not _selection.is_empty() and playing == null
	var models := false
	for id in _selection:
		if live() != null and live().get_instance(id).get("class_name", "") == "Model":
			models = true
	return [
		{"id": "save", "text": "Save", "key": "Ctrl+S", "menu": false,
		 "on": playing == null and _opened and unsaved(),
		 "tip": "Save the place  Ctrl+S" if unsaved() else "Nothing to save",
		 "run": func(): save_place()},
		{"id": "publish", "text": "Publish", "tabs": ["Home"], "group": "Publish",
		 "on": playing == null and _opened and place.dir != "",
		 "tip": "Put this place on chain with the Publisher: it shows every file and what it costs before anything is sent",
		 "run": func(): open_publisher()},
		{"id": "cut", "text": "Cut", "key": "Ctrl+X", "on": some, "tabs": ["Home"], "group": "Clipboard", "run": func(): _cut()},
		{"id": "copy", "text": "Copy", "key": "Ctrl+C", "on": some, "tabs": ["Home"], "group": "Clipboard", "run": func(): _copy()},
		{"id": "paste", "text": "Paste", "key": "Ctrl+V", "on": not _clipboard.is_empty() and playing == null, "tabs": ["Home"], "group": "Clipboard", "run": func(): _paste()},
		{"id": "duplicate", "text": "Duplicate", "key": "Ctrl+D", "on": some, "tabs": ["Home"], "group": "Clipboard", "run": func(): _duplicate()},
		{"id": "delete", "text": "Delete", "key": "Del", "on": not _selection.is_empty(), "run": func(): _delete()},
		{"id": "-1", "text": "", "key": "", "on": false, "run": func(): pass},
		{"id": "rename", "text": "Rename", "key": "F2", "on": _selection.size() == 1 and playing == null, "run": func(): _explorer.rename(primary())},
		{"id": "group", "text": "Group", "key": "Ctrl+G", "on": some, "tabs": ["Home"], "group": "Edit", "run": func(): _group()},
		{"id": "ungroup", "text": "Ungroup", "key": "Ctrl+U", "on": models and playing == null, "tabs": ["Home"], "group": "Edit", "run": func(): _ungroup()},
		{"id": "union", "text": "Union", "key": "Ctrl+Shift+G", "on": _selection.size() > 1 and playing == null,
		 "tabs": ["Model"], "group": "Solid", "tip": "One part shaped like all of them; a negation in the selection is cut out",
		 "run": func(): _union()},
		{"id": "separate", "text": "Separate", "key": "Ctrl+Shift+U",
		 "on": _selection.size() == 1 and playing == null and live() != null
			and live().get_instance(_selection[0]).get("class_name", "") == "UnionOperation",
		 "tabs": ["Model"], "group": "Solid", "tip": "Put the parts a union was made of back, and take the union away",
		 "run": func(): _separate()},
		{"id": "dbg_continue", "text": "Continue", "key": "F5", "on": _debug_paused and playing != null, "tabs": ["Test"], "group": "Debug",
		 "tip": "Let the stopped script go on until the next breakpoint", "run": func(): _debug_go(0)},
		{"id": "dbg_over", "text": "Step Over", "key": "F10", "on": _debug_paused and playing != null, "tabs": ["Test"], "group": "Debug",
		 "tip": "Run the line and stop on the next one of this function", "run": func(): _debug_go(2)},
		{"id": "dbg_into", "text": "Step Into", "key": "F11", "on": _debug_paused and playing != null, "tabs": ["Test"], "group": "Debug",
		 "tip": "Stop on the next line, inside a call if there is one", "run": func(): _debug_go(1)},
		{"id": "dbg_out", "text": "Step Out", "key": "Shift+F11", "on": _debug_paused and playing != null, "tabs": ["Test"], "group": "Debug",
		 "tip": "Run to the end of this function and stop in its caller", "run": func(): _debug_go(3)},
		{"id": "rig_r6", "text": "R6 Rig", "key": "", "on": playing == null and live() != null, "tabs": ["Avatar"], "group": "Rig",
		 "tip": "Build Roblox's six-limb block rig in front of the camera: the R6 parts, Motor6D joints and attachments, and a Humanoid",
		 "run": func(): _build_rig(false)},
		{"id": "rig_r15", "text": "R15 Rig", "key": "", "on": playing == null and live() != null, "tabs": ["Avatar"], "group": "Rig",
		 "tip": "Build the fifteen-limb block rig, each joint in its limb, as this engine lays R15 characters out",
		 "run": func(): _build_rig(true)},
		{"id": "animate", "text": "Animation Editor", "key": "", "on": playing == null and live() != null,
		 "tabs": ["Model", "Avatar"], "group": "Animation",
		 "tip": "Pose the selected rig's Motor6D joints over a timeline; Save writes a KeyframeSequence under its AnimSaves, which an Animation plays by that path",
		 "run": func(): _toggle_animation()},
		{"id": "negate", "text": "Negate", "key": "Ctrl+Shift+N", "on": some,
		 "tabs": ["Model"], "group": "Solid", "tip": "Turn it into a hole, to union with something else",
		 "run": func(): _negate()},
		{"id": "-2", "text": "", "key": "", "on": false, "run": func(): pass},
		{"id": "insert", "text": "Insert Object...", "key": "", "on": playing == null, "run": func(): _open_insert()},
		{"id": "focus", "text": "Zoom to", "key": "F", "on": not _selection.is_empty(), "run": func(): _focus()},
	] + _shell_actions() + _plugin_actions()

# The rest of the shell, in the same shape. `tabs` names the ribbon tabs an action appears
# on, `group` the band it sits in, `checked` marks a toggle. Without `tabs` it is menu and
# keyboard only, as Delete, Rename, Select All and Zoom to are in Studio.
func _shell_actions() -> Array:
	var editing := playing == null
	var some := not _selection.is_empty() and editing
	return [
		{"id": "undo", "text": "Undo", "key": "Ctrl+Z", "on": editing and history.can_undo(), "menu": false,
		 "tip": "Undo %s" % history.undo_label() if history.can_undo() else "Nothing to undo", "run": func(): _undo()},
		{"id": "redo", "text": "Redo", "key": "Ctrl+Shift+Z", "on": editing and history.can_redo(), "menu": false,
		 "tip": "Redo %s" % history.redo_label() if history.can_redo() else "Nothing to redo", "run": func(): _redo()},
		{"id": "select", "text": "Select", "key": "1", "tabs": ["Home", "Model"], "group": "Tools", "on": editing,
		 "checked": _handles != null and _handles.mode == Handles.Mode.SELECT, "menu": false, "run": func(): _tool(Handles.Mode.SELECT)},
		{"id": "move", "text": "Move", "key": "2", "tabs": ["Home", "Model"], "group": "Tools", "on": editing,
		 "checked": _handles != null and _handles.mode == Handles.Mode.MOVE, "menu": false, "run": func(): _tool(Handles.Mode.MOVE)},
		{"id": "scale", "text": "Scale", "key": "3", "tabs": ["Home", "Model"], "group": "Tools", "on": editing,
		 "checked": _handles != null and _handles.mode == Handles.Mode.SCALE, "menu": false, "run": func(): _tool(Handles.Mode.SCALE)},
		{"id": "rotate", "text": "Rotate", "key": "4", "tabs": ["Home", "Model"], "group": "Tools", "on": editing,
		 "checked": _handles != null and _handles.mode == Handles.Mode.ROTATE, "menu": false, "run": func(): _tool(Handles.Mode.ROTATE)},
		{"id": "part", "text": "Part", "tabs": ["Home"], "group": "Insert", "on": editing, "menu": false, "run": func(): _insert_part()},
		{"id": "anchor", "text": "Anchor", "tabs": ["Home", "Model"], "group": "Edit", "on": some,
		 "menu": false, "run": func(): _toggle_on_selection("Anchored")},
		{"id": "lock", "text": "Lock", "tabs": ["Home", "Model"], "group": "Edit", "on": some,
		 "menu": false, "run": func(): _toggle_on_selection("Locked")},
		{"id": "here", "text": "Play Here", "menu": false, "tabs": ["Test"], "group": "Test", "on": playing == null,
		 "run": func(): play_here()},
		{"id": "run", "text": "Run", "menu": false, "tabs": ["Test"], "group": "Test", "on": playing == null,
		 "run": func(): run()},
		{"id": "pause", "text": "Resume" if (playing != null and not playing.auto_step) else "Pause", "menu": false,
		 "tabs": ["Test"], "group": "Test", "on": playing != null,
		 "run": func(): playing.auto_step = not playing.auto_step; _refresh_commands()},
		{"id": "step", "text": "Step", "menu": false, "tabs": ["Test"], "group": "Test",
		 "on": playing != null and not playing.auto_step, "run": func(): playing.flush()},
		{"id": "surface", "text": "Surface" if _handles != null and _handles.surface else "Free move",
		 "menu": false, "tabs": ["Model"], "group": "Tools", "on": playing == null,
		 "tip": "Dragging a brick lands it flush on what is under it",
		 "run": func(): _handles.surface = not _handles.surface; _refresh_commands()},
		{"id": "axes", "text": "World axes" if _handles != null and _handles.world_axes else "Part axes",
		 "menu": false, "tabs": ["Model"], "group": "Tools", "on": playing == null,
		 "run": func(): _handles.world_axes = not _handles.world_axes; _refresh_commands()},
		{"id": "play", "text": "Stop" if playing != null else "Play", "tabs": ["Home", "Test"], "group": "Test",
		 "on": true, "menu": false, "run": func(): stop() if playing != null else play()},
	]

# If any of the selection has it off, the button turns them all on, as Studio's Anchor and
# Lock do.
func _toggle_on_selection(name: String) -> void:
	_prune()
	if _selection.is_empty():
		return
	var want := false
	for id in _selection:
		for p in world.get_properties(id):
			if p.name == name and not bool(p.value):
				want = true
	history.begin("%s %d" % [name, _selection.size()] if _selection.size() > 1 else name)
	for id in _selection:
		for p in world.get_properties(id):
			if p.name == name:
				history.set_property(id, name, want)
	history.end()

func _do(id: String) -> void:
	_prune()
	for action in actions():
		if action.id == id:
			if action.on:
				action.run.call()
			return

# An undo can take an instance away while it is still selected; a verb acting on that id
# would act somewhere surprising.
func _prune() -> void:
	if live() == null:
		return
	var alive := []
	for id in _selection:
		if not live().get_instance(id).is_empty():
			alive.append(id)
	if alive.size() != _selection.size():
		_select(alive)

# What is selected, as the text of the *.model.json files it would be saved as.
func _copy() -> void:
	_prune()
	_clipboard = []
	for id in _selection:
		_clipboard.append({"name": live().get_instance(id).get("name", ""), "model": Model.of(live(), id)})

func _cut() -> void:
	_copy()
	_delete()

# Into the selection rather than beside it, as Studio pastes.
func _paste() -> void:
	if _clipboard.is_empty() or playing != null:
		return
	var into := primary() if primary() != 0 else _workspace()
	history.begin("paste %d" % _clipboard.size() if _clipboard.size() > 1 else "paste")
	var made := []
	for one in _clipboard:
		var id := await history.add(into, one.name, one.model)
		if id != 0:
			made.append(id)
	history.end()
	if not made.is_empty():
		_select(made, true)

func _duplicate() -> void:
	_prune()
	if _selection.is_empty():
		return
	var into: int = live().get_instance(primary()).get("parent", 0)
	var copies := []
	for id in _selection:
		copies.append({"name": live().get_instance(id).get("name", ""), "model": Model.of(live(), id)})
	history.begin("duplicate %d" % copies.size() if copies.size() > 1 else "duplicate")
	var made := []
	for one in copies:
		var id := await history.add(into, one.name, one.model)
		if id != 0:
			made.append(id)
	history.end()
	if not made.is_empty():
		_select(made, true)

# A Model round the selection, where the selection was.
func _group() -> void:
	_prune()
	if _selection.is_empty() or playing != null:
		return
	if _selection.is_empty():
		return
	var into: int = world.get_instance(primary()).get("parent", 0)
	var moving := _selection.duplicate()
	history.begin("group %d" % moving.size())
	var model := await history.insert("Model", into)
	if model != 0:
		for id in moving:
			history.set_parent(id, model)
	history.end()
	if model != 0:
		await _settle_frames(2)
		_select_one(model, true)

func _ungroup() -> void:
	_prune()
	var taken := []
	history.begin("ungroup")
	for id in _selection:
		if world.get_instance(id).get("class_name", "") != "Model":
			continue
		var up: int = world.get_instance(id).get("parent", 0)
		for child in world.get_child_ids(id):
			history.set_parent(child, up)
			taken.append(child)
		history.destroy(id)
	history.end()
	await _settle_frames(2)
	_select(taken, true)

# Line the selection up along `axis`: edge -1 for the low side, +1 for the high, 0 for the
# centres. Roblox's Model tab has this and distribute().
func align(axis: int, edge: int) -> void:
	_prune()
	if _selection.size() < 2:
		return
	var box := {}
	for id in _selection:
		var f := _handles.frame_of(id)
		if not f.is_empty():
			box[id] = f
	if box.size() < 2:
		return
	var want := 0.0
	var first := true
	for id in box:
		var f: Dictionary = box[id]
		var reach: float = _reach_along(f, axis)
		var value: float = f.middle[axis] + (0.0 if edge == 0 else reach * edge)
		if first:
			want = value
			first = false
		elif edge < 0:
			want = minf(want, value)
		elif edge > 0:
			want = maxf(want, value)
		else:
			want += value
	if edge == 0:
		want /= box.size()
	history.begin("align %d" % box.size())
	for id in box:
		var f: Dictionary = box[id]
		var at: Vector3 = _prop_value(id, "Position")
		at[axis] += want - (f.middle[axis] + (0.0 if edge == 0 else _reach_along(f, axis) * edge))
		history.set_property(id, "Position", at)
	history.end()

# Even gaps along an axis, between the two that are already furthest apart.
func distribute(axis: int) -> void:
	_prune()
	if _selection.size() < 3:
		return
	# Only what has a body: a Model or a Folder has no frame, so no middle to spread.
	var order := []
	var middles := {}
	for id in _selection:
		var f := _handles.frame_of(id)
		if not f.is_empty():
			order.append(id)
			middles[id] = f.middle[axis]
	if order.size() < 3:
		return
	order.sort_custom(func(a, b): return middles[a] < middles[b])
	var low: float = middles[order[0]]
	var high: float = middles[order[order.size() - 1]]
	var step: float = (high - low) / float(order.size() - 1)
	history.begin("distribute %d" % order.size())
	for i in order.size():
		var at: Vector3 = _prop_value(order[i], "Position")
		at[axis] = low + step * i
		history.set_property(order[i], "Position", at)
	history.end()

static func _reach_along(f: Dictionary, axis: int) -> float:
	var b: Basis = f.basis
	return absf(b.x[axis]) * f.half.x + absf(b.y[axis]) * f.half.y + absf(b.z[axis]) * f.half.z

func _prop_value(id: int, name: String):
	for p in live().get_properties(id):
		if p.name == name:
			return p.value
	return Vector3.ZERO

# ---- solid modelling ----------------------------------------------------------------
# The boolean runs in Solid.gd out of a throwaway CSG tree and is baked onto the new
# instance, so the place file carries the shape and nothing recomputes it on open.
func _union() -> void:
	_prune()
	if _selection.size() < 2 or playing != null:
		return
	var parent: int = world.get_instance(_selection[0]).get("parent", 0)
	var made: int = await Solid.union(world, _handles, history, _selection.duplicate(), parent)
	if made == 0:
		_say("studio", "nothing to union: pick two or more parts with a shape", "warn")
		return
	await _settle_frames(8)
	_select_one(made)
	_say("studio", "unioned into one part", "studio")

func _separate() -> void:
	_prune()
	if _selection.size() != 1 or playing != null:
		return
	var made: Array = await Solid.separate(world, _handles, history, _selection[0])
	if made.is_empty():
		_say("studio", "nothing to separate: that is not a union, or it does not remember its parts", "warn")
		return
	await _settle_frames(8)
	_select(made)
	_say("studio", "separated into %d" % made.size(), "studio")

func _negate() -> void:
	_prune()
	if _selection.is_empty() or playing != null:
		return
	var made: Array = await Solid.negate(world, _handles, history, _selection.duplicate())
	if made.is_empty():
		_say("studio", "nothing to negate", "warn")
		return
	await _settle_frames(8)
	_select(made)
	_say("studio", "negated %d: union it with something to cut that shape out" % made.size(), "studio")

func _select_all() -> void:
	var all := []
	for id in live().get_child_ids(_workspace()):
		all.append(id)
	_select(all, true)

# Put the camera where it can see the selection, as Studio's F does.
func _focus() -> void:
	if _selection.is_empty():
		return
	var box := AABB()
	var seen := false
	for id in _outlined():
		var f := _handles.frame_of(id)
		if f.is_empty():
			continue
		var one := AABB(f.middle - f.half.length() * Vector3.ONE, f.half.length() * 2.0 * Vector3.ONE)
		box = one if not seen else box.merge(one)
		seen = true
	if not seen:
		return
	_frame(box)

# Far enough back for the box to fit both ways: the vertical half-angle is fov/2, the
# horizontal one widened by the aspect.
func _frame(box: AABB) -> void:
	var radius: float = maxf(box.size.length() * 0.5, 2.0)
	var vertical: float = deg_to_rad(_cam.fov) * 0.5
	var horizontal: float = atan(tan(vertical) * maxf(_cam.get_viewport().get_visible_rect().size.aspect(), 0.2))
	var span: float = radius / maxf(sin(minf(vertical, horizontal)), 0.05)
	# Back off the way the camera already looks, unless that is straight up or down, where
	# look_at with Vector3.UP has no basis to build and refuses.
	var away: Vector3 = _cam.global_transform.basis.z
	if absf(away.normalized().y) > 0.999:
		away = Vector3(0.6, 0.5, 0.6).normalized()
	_cam.global_position = box.get_center() + away * span
	_cam.look_at(box.get_center(), Vector3.UP)
	_yaw = _cam.rotation.y
	_pitch = _cam.rotation.x

# Everything the place draws, fitted in view.
func _frame_place() -> void:
	# A place that saved a Camera opens looking where it was left, as in Roblox Studio.
	if _camera_from_place():
		return
	var box := AABB()
	var seen := false
	for id in _descendants(_workspace(), []):
		var body := live().get_part_node(id)
		if body == null:
			continue
		var drawn := _drawn_mesh(body, live().get_instance(id).get("name", ""))
		if drawn == null:
			continue
		# Transform3D * AABB re-fits the box from all eight corners, so a place built on
		# the diagonal does not frame off to one side of itself.
		var one: AABB = drawn.global_transform * drawn.get_aabb()
		box = one if not seen else box.merge(one)
		seen = true
	if seen:
		_frame(box)

# The Workspace's Camera, if the place saved one anywhere but the origin. Roblox's camera
# looks down its -Z, as Godot's does; Orientation is YXZ Euler degrees.
func _camera_from_place() -> bool:
	var w := live()
	if w == null:
		return false
	for kid in w.get_child_ids(_workspace()):
		if w.get_instance(kid).get("class_name", "") != "Camera":
			continue
		var pos := Vector3.ZERO
		var ori := Vector3.ZERO
		for pr in w.get_properties(kid):
			if pr.name == "Position" and pr.value is Vector3:
				pos = pr.value
			elif pr.name == "Orientation" and pr.value is Vector3:
				ori = pr.value
		if pos == Vector3.ZERO and ori == Vector3.ZERO:
			return false
		_cam.global_transform = Transform3D(Basis.from_euler(Vector3(deg_to_rad(ori.x), deg_to_rad(ori.y), deg_to_rad(ori.z)), EULER_ORDER_YXZ), pos)
		_yaw = _cam.rotation.y
		_pitch = _cam.rotation.x
		return true
	return false

# Arrived when the tree stops growing between frames; then point the camera at it.
func _settle_then_frame() -> void:
	var was := -1
	for wait in 300:
		await get_tree().process_frame
		if world == null:
			return
		var now := _descendants(0, []).size()      # the whole tree, not just the Workspace
		if now == was:
			_opened = true
			_frame_place()
			return
		was = now
	_opened = true                                 # gave up waiting: let Save work anyway

func _settle_frames(n: int) -> void:
	for i in n:
		await get_tree().process_frame

func _delete():
	if _selection.is_empty():
		return
	if playing != null:
		for id in _selection:
			playing.destroy_instance(id)
		_select([])
		return
	history.begin("delete %d parts" % _selection.size() if _selection.size() > 1 else "delete")
	for id in _selection:
		history.destroy(id)
	history.end()
	_select([])

func _workspace() -> int:
	return _service_of(live(), "Workspace")

# A runaway script is killed every frame: say it once with the reason, then every 300th.
var _kills := {}
func _on_killed(script_name: String, reason: String):
	var n: int = _kills.get(script_name, 0) + 1
	_kills[script_name] = n
	if n == 1:
		_say(script_name, "killed: " + reason, "error")
	elif n % 300 == 0:
		_say(script_name, "killed %d times (still over budget)" % n, "error")

# ---- the menus --------------------------------------------------------------------
# Built fresh from the action table each time, so a greyed item is one that would do nothing.
func _open_menu(at: Vector2) -> void:
	if playing != null:
		return
	_menu.clear()
	var i := 0
	for action in actions():
		if action.get("menu", true) == false:
			continue          # a ribbon or quick-access button, not a menu verb
		if action.text == "":
			_menu.add_separator()
		else:
			_menu.add_item(action.text, i)
			_menu.set_item_disabled(_menu.get_item_index(i), not action.on)
			if action.key != "":
				# In the label, not as a PopupMenu accelerator: an accelerator fires
				# while a field has focus, making Ctrl+C copy the selection, not text.
				_menu.set_item_text(_menu.get_item_index(i), "%s  ·  %s" % [action.text, action.key])
			_menu.set_item_metadata(_menu.get_item_index(i), action.id)
		i += 1
	_menu.reset_size()
	_menu.position = Vector2i(get_window().position) + Vector2i(at)
	_menu.popup()

func _open_insert() -> void:
	if playing != null:
		return
	_insert.popup_centered(Vector2i(360, 460))
	var filter: LineEdit = _insert.get_meta("filter")
	filter.text = ""
	_fill_insert("")
	filter.grab_focus()

func _fill_insert(needle: String) -> void:
	var list: ItemList = _insert.get_meta("list")
	list.clear()
	for name in live().get_creatable_classes():
		if needle == "" or String(name).to_lower().contains(needle.to_lower()):
			list.add_item(name)
	if list.item_count > 0:
		list.select(0)

func _insert_chosen(at: int) -> void:
	var list: ItemList = _insert.get_meta("list")
	if at < 0 or at >= list.item_count:
		return
	var class_name_ := list.get_item_text(at)
	_insert.hide()
	var into := primary() if primary() != 0 else _workspace()
	var made := await history.insert(class_name_, into)
	if made != 0:
		_select_one(made, true)

# Dragged onto another row in the Explorer: one move, one thing to undo.
func _reparent(ids: Array, onto: int) -> void:
	if playing != null or ids.is_empty():
		return
	history.begin("move %d" % ids.size() if ids.size() > 1 else "move")
	for id in ids:
		history.set_parent(id, onto)
	history.end()

# ---- the command bar -----------------------------------------------------------------
# One line of Luau against the live world. It runs where a Script would: on the server.
func _run_command(text: String) -> void:
	var chunk := text.strip_edges()
	if chunk == "" or live() == null:
		return
	_said.append(chunk)
	_said_at = _said.size()
	_command.text = ""
	_say("command", chunk, "studio")
	live().run_chunk("command", chunk)

func _command_history(step: int) -> void:
	if _said.is_empty():
		return
	_said_at = clampi(_said_at + step, 0, _said.size())
	_command.text = "" if _said_at >= _said.size() else _said[_said_at]
	_command.caret_column = _command.text.length()

# ---- the script editor --------------------------------------------------------------
# A GuiObject is anything whose Size is a UDim2.
func _is_gui(id: int) -> bool:
	for p in world.get_properties(id):
		if p.name == "Size":
			return p.type == "UDim2"
	return false

# Always an id in the edit world, never in a playing copy: what a script is opened for is a
# fix to the place.
func _open_script(id: int, line := 0) -> void:
	if not ScriptPane.is_script(world, id):
		return
	_editor.world = world
	_editor.history = history
	if line > 0:
		_editor.go_to(id, line)
	else:
		_editor.open(id)
	_gap.visible = false
	_refresh_title()

# Where an instance is, dotted, as an error names it: "Workspace.Map.Build".
func _full_name(of_world: PulseBlockzWorld, id: int) -> String:
	var back := []
	var at := id
	for hop in 32:
		var info := of_world.get_instance(at)
		if info.is_empty():
			return ""
		back.append(str(info.get("name", "")))
		at = int(info.get("parent", 0))
		if at == 0:
			break
	back.reverse()
	return ".".join(back)

func close_script() -> void:
	_editor.close_all()
	_gap.visible = true
	_refresh_title()

# An open script with edits not yet committed counts: the title and _confirm both read this.
func unsaved() -> bool:
	return _dirty or (_editor != null and _editor.dirty())

func _refresh_title() -> void:
	get_window().title = "PulseBlockz Studio -- %s%s%s" % [place.dir.get_file(),
		" *" if unsaved() else "", "  [running]" if playing != null else ""]
	if _playmark != null:
		_playmark.visible = playing != null
		if playing != null:
			_playing_says.text = "  %s -- nothing built in here is kept  " % (
				"Playing" if playing.auto_join else "Running")
			_paint_playmark()

# The world draws StarterGui over the view; this only says whether to.
func set_gui_preview(on: bool) -> void:
	_gui_preview = on
	if world != null:
		world.gui_preview = on
	_set_setting("studio", "gui_preview", on)

func _paint_playmark() -> void:
	if _playmark == null:
		return
	var frame := StyleBoxFlat.new()
	frame.bg_color = Color(0, 0, 0, 0)
	frame.border_color = palette.playing
	frame.set_border_width_all(2)
	frame.content_margin_top = 0
	_playmark.add_theme_stylebox_override("panel", frame)
	var pill := StyleBoxFlat.new()
	pill.bg_color = palette.playing
	pill.corner_radius_bottom_left = 4
	pill.corner_radius_bottom_right = 4
	_playing_says.add_theme_stylebox_override("normal", pill)
	_playing_says.add_theme_color_override("font_color", Color(1, 1, 1))
	_playing_says.add_theme_font_size_override("font_size", 11)

func _confirm(question: String, then: Callable) -> void:
	if not unsaved():
		then.call()
		return
	if _ask == null:
		_ask = ConfirmationDialog.new()
		_ask.ok_button_text = "Discard"
		add_child(_ask)
	for was in _ask.confirmed.get_connections():
		_ask.confirmed.disconnect(was.callable)
	_ask.dialog_text = question
	_ask.confirmed.connect(then, CONNECT_ONE_SHOT)
	_ask.popup_centered()

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		_confirm("This place has changes that are not saved. Close anyway?", func(): get_tree().quit())

# ---- how it looks ------------------------------------------------------------------
const LOOK_FILE := "user://studio.cfg"

func set_look(which: String) -> void:
	palette = Palette.of(which)
	# One Theme on the root Window reaches the whole subtree, but a CanvasLayer is neither a
	# Control nor a Window and owns none: its children are handed the theme directly.
	var skin := palette.theme()
	get_window().theme = skin
	if _ui != null:
		_ui.theme = skin
	if _menu != null:
		_menu.theme = skin
	if _insert != null:
		_insert.theme = skin
	_explorer.palette = palette
	_explorer._version = -1          # rebuild: the row colours are in the items
	_properties.palette = palette
	_properties._shape = ""
	_properties.refresh()
	_editor.palette = palette
	_editor.repaint()
	if _box != null:
		_box.material_override.albedo_color = palette.outline
	if _marquee != null:
		var box := StyleBoxFlat.new()
		box.bg_color = Color(palette.accent.r, palette.accent.g, palette.accent.b, 0.12)
		box.border_color = palette.accent
		box.set_border_width_all(1)
		_marquee.add_theme_stylebox_override("panel", box)
	_status.add_theme_color_override("font_color", palette.text_dim)
	_paint_playmark()
	if _gui_handles != null:
		_gui_handles.palette = palette
		_gui_handles.queue_redraw()
	if _grid != null:
		_draw_grid()
	_redraw_output()
	_fill_band()
	_set_setting("studio", "look", which)

func _remembered_look() -> String:
	var which := str(_setting("studio", "look", "Dark"))
	return which if which in Palette.NAMES else "Dark"

# _settings holds the whole file: saving a fresh ConfigFile would drop every other section.
func _setting(where: String, key: String, fallback):
	return _settings.get_value(where, key, fallback)

func _set_setting(where: String, key: String, value) -> void:
	_settings.set_value(where, key, value)
	_settings.save(LOOK_FILE)

func _remember_layout(which: String, size: int) -> void:
	_set_setting("layout", which, maxi(size, 0))

# push_color bakes into the label's buffer, so _lines is kept and re-rendered on a repaint.
func _redraw_output() -> void:
	if _output == null:
		return
	_output.clear()
	for line in _lines:
		if _shown_levels.get(line.role, true):
			_write_line(line.from, line.text, line.role)

func _role_colour(role: String) -> Color:
	match role:
		"warn": return palette.warning
		"error": return palette.error
		"studio": return palette.accent
	return palette.text_dim

func clear_output() -> void:
	_lines.clear()
	_kills.clear()
	if _output != null:
		_output.clear()

func _say(from: String, text: String, role := "print") -> void:
	_lines.append({"from": from, "text": text, "role": role})
	if _lines.size() > 500:
		_lines.pop_front()
	if _shown_levels.get(role, true):
		_write_line(from, text, role)

# A Luau error reads "Workspace.Map.Build:12: attempt to index nil" -- the script's full name
# and the line in it, which is enough to jump there.
var _where := RegEx.create_from_string("^\\s*([A-Za-z0-9_.]+):(\\d+):")

# "<script>:<line>" if this Output line points into the place, "" if it is only text.
func _jump_of(from: String, text: String) -> String:
	var found := _where.search(text)
	if found == null:
		return ""
	var named := found.get_string(1)
	if _script_named(named) == 0:
		named = from                         # a compile error names the chunk, not the path
	return "%s:%s" % [named, found.get_string(2)] if _script_named(named) != 0 else ""

func _write_line(from: String, text: String, role: String) -> void:
	var go := _jump_of(from, text)
	_output.push_color(_role_colour(role))
	if go != "":
		_output.push_meta(go)
	_output.add_text("%s  %s\n" % [from, text])
	if go != "":
		_output.pop()
	_output.pop()

# By full name, in the edit world -- never the running copy: a fix belongs in the place.
func _script_named(full: String) -> int:
	if world == null or full == "":
		return 0
	var at := 0
	for part in full.split("."):
		var found := 0
		for c in world.get_child_ids(at):
			if world.get_instance(c).get("name", "") == part:
				found = c
				break
		if found == 0:
			return 0
		at = found
	return at if ScriptPane.is_script(world, at) else 0

func _jump_to(meta) -> void:
	var mark := str(meta).rsplit(":", true, 1)
	if mark.size() != 2:
		return
	var id := _script_named(mark[0])
	if id != 0:
		_open_script(id, mark[1].to_int())

# ---- the shell --------------------------------------------------------------------
func _build_ui():
	var layer := CanvasLayer.new()
	# Above a previewed ScreenGui, which takes 1 + DisplayOrder.
	layer.layer = 50
	add_child(layer)
	_ui = VBoxContainer.new()
	var page := _ui
	page.set_anchors_preset(Control.PRESET_FULL_RECT)
	page.add_theme_constant_override("separation", 0)
	layer.add_child(page)

	# A frame round the view while a place runs, as Roblox draws one: what is built in the
	# running copy is thrown away on Stop.
	_playmark = PanelContainer.new()
	_playmark.set_anchors_preset(Control.PRESET_FULL_RECT)
	_playmark.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_playmark.visible = false
	var mark_row := HBoxContainer.new()
	mark_row.mouse_filter = Control.MOUSE_FILTER_IGNORE   # it spans the width: PASS would still take the click
	mark_row.alignment = BoxContainer.ALIGNMENT_CENTER
	mark_row.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	_playing_says = Label.new()
	_playing_says.mouse_filter = Control.MOUSE_FILTER_IGNORE
	mark_row.add_child(_playing_says)
	_playmark.add_child(mark_row)
	layer.add_child(_playmark)
	_gui_handles = GuiHandles.new()
	_gui_handles.history = history
	_gui_handles.palette = palette
	layer.add_child(_gui_handles)

	_build_ribbon(page)

	# Three splitters, each holding one dock and "the rest". A SplitContainer has no stylebox,
	# so the middle stays transparent and clicks reach the viewport through it.
	_stack = VSplitContainer.new()
	_stack.size_flags_vertical = Control.SIZE_EXPAND_FILL
	page.add_child(_stack)

	_side = HSplitContainer.new()
	_side.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_stack.add_child(_side)

	_explorer = Explorer.new()
	_explorer.picked.connect(func(ids): _select(ids))
	# Ids are per-DataModel: one from the playing copy the Explorer is showing names a
	# different instance in the edit world, so the script is re-found by full name.
	_explorer.opened.connect(func(id):
		_open_script(id if live() == world else _script_named(_full_name(live(), id))))
	_explorer.dropped.connect(_reparent)
	_explorer.name_typed.connect(func(id, to): history.set_property(id, "Name", to))
	_explorer.menu_wanted.connect(func(at): _open_menu(at - Vector2(get_window().position)))
	var find := LineEdit.new()
	find.placeholder_text = "Filter"
	find.clear_button_enabled = true
	find.text_changed.connect(func(text): _explorer.set_filter(text))
	_side.add_child(_dock("Explorer", _explorer, find))

	_rest = HSplitContainer.new()
	_side.add_child(_rest)

	# One wrapper for both: a splitter lays out only its first two visible children.
	var middle := MarginContainer.new()
	middle.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	middle.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_rest.add_child(middle)

	_gap = Control.new()              # the world shows through here
	_gap.mouse_filter = Control.MOUSE_FILTER_IGNORE
	middle.add_child(_gap)
	var gap := _gap

	_editor = ScriptPane.new()      # takes the middle when a script is open
	_editor.dirtied.connect(func(): _refresh_title())
	_editor.closed.connect(func(): gap.visible = true; _refresh_title())
	middle.add_child(_editor)

	_marquee = Panel.new()            # the drag box, drawn over the viewport
	var box := StyleBoxFlat.new()
	box.bg_color = Color(1, 0.55, 0.1, 0.12)
	box.border_color = Color(1, 0.55, 0.1, 0.9)
	box.set_border_width_all(1)
	_marquee.add_theme_stylebox_override("panel", box)
	_marquee.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_marquee.visible = false
	layer.add_child(_marquee)
	_properties = Properties.new()
	_properties.ref_wanted.connect(func(name): _say("studio", "click an instance to set %s" % name, "studio"))
	var sift := LineEdit.new()
	sift.placeholder_text = "Filter"
	sift.clear_button_enabled = true
	sift.text_changed.connect(func(text): _properties.set_filter(text))
	_rest.add_child(_dock("Properties", _properties, sift))

	_menu = PopupMenu.new()
	_menu.id_pressed.connect(func(id): _do(str(_menu.get_item_metadata(_menu.get_item_index(id)))))
	layer.add_child(_menu)
	_build_insert(layer)

	_output = RichTextLabel.new()
	_output.scroll_following = true
	_output.bbcode_enabled = true
	_output.selection_enabled = true      # so a line can be highlighted and copied
	_output.context_menu_enabled = true   # right-click: Copy
	_output.meta_underlined = false
	_output.meta_clicked.connect(_jump_to)
	_output.add_theme_font_size_override("normal_font_size", 12)
	var levels := HBoxContainer.new()      # what the Output shows
	for level in [["print", "Print"], ["warn", "Warnings"], ["error", "Errors"]]:
		var b := Button.new()
		b.text = level[1]
		b.toggle_mode = true
		b.button_pressed = true
		b.toggled.connect(func(on): _shown_levels[level[0]] = on; _redraw_output())
		levels.add_child(b)
	var copy := Button.new()
	copy.text = "Copy"
	copy.tooltip_text = "Put the whole Output on the clipboard"
	copy.pressed.connect(func():
		var all := PackedStringArray()
		for line in _lines:
			all.append("%s  %s" % [line.from, line.text])
		DisplayServer.clipboard_set(char(10).join(all))
		_say("studio", "copied %d lines" % all.size(), "studio"))
	levels.add_child(copy)
	var wipe := Button.new()
	wipe.text = "Clear"
	wipe.tooltip_text = "Empty the Output"
	wipe.pressed.connect(clear_output)
	levels.add_child(wipe)
	var out := _dock("Output", _output, levels)
	out.custom_minimum_size = Vector2(0, OUTPUT_MIN)
	_stack.add_child(out)
	_anim = AnimationEditorScript.new()     # below the view and the Output, shown while a rig is posed
	_anim.studio = self
	page.add_child(_anim)
	_debugger = DebuggerPaneScript.new()    # and the debugger, while a script stands at a breakpoint
	_debugger.continue_pressed.connect(func(): _debug_go(0))
	_debugger.step_pressed.connect(func(kind): _debug_go(kind))
	_debugger.frame_picked.connect(func(script, line): var sid := _script_named(script); if sid != 0: _open_script(sid, line))
	page.add_child(_debugger)
	_editor.breakpoints_changed.connect(_on_breakpoints_changed)
	_editor.shown.connect(_on_script_shown)

	_command = LineEdit.new()   # a line of Luau against the live world
	_command.placeholder_text = "Run a line of Luau"
	_command.text_submitted.connect(_run_command)
	_command.gui_input.connect(func(e):
		if e is InputEventKey and e.pressed:
			if e.keycode == KEY_UP: _command_history(-1)
			elif e.keycode == KEY_DOWN: _command_history(1))
	var run_row := HBoxContainer.new()
	var run_label := Label.new()
	run_label.text = " > "
	run_label.add_theme_color_override("font_color", palette.accent)
	run_row.add_child(run_label)
	_command.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	run_row.add_child(_command)
	page.add_child(run_row)

	_bar = Label.new()          # what is selected, what the tools will do to it
	_bar.add_theme_color_override("font_color", palette.text_dim)
	var strip := PanelContainer.new()
	strip.add_child(_bar)
	page.add_child(strip)

	# split_offset is in pixels: a positive one is the first child's width, a negative one the
	# second's, which is why Properties and Output are negated.
	_side.split_offset = int(_setting("layout", "explorer", DOCK_WIDTH))
	_rest.split_offset = -int(_setting("layout", "properties", DOCK_WIDTH))
	_stack.split_offset = -int(_setting("layout", "output", OUTPUT_HEIGHT))
	_side.dragged.connect(func(at): _remember_layout("explorer", at))
	_rest.dragged.connect(func(at): _remember_layout("properties", -at))
	_stack.dragged.connect(func(at): _remember_layout("output", -at))

# Insert Object: the list is get_creatable_classes(), so it cannot drift from the class table.
func _build_insert(layer: CanvasLayer) -> void:
	_insert = Window.new()
	_insert.title = "Insert Object"
	_insert.wrap_controls = true
	_insert.visible = false
	_insert.close_requested.connect(func(): _insert.hide())
	var box := VBoxContainer.new()
	box.set_anchors_preset(Control.PRESET_FULL_RECT)
	var filter := LineEdit.new()
	filter.placeholder_text = "Search"
	var list := ItemList.new()
	list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	filter.text_changed.connect(func(text): _fill_insert(text))
	filter.text_submitted.connect(func(_t): _insert_chosen(list.get_selected_items()[0] if not list.get_selected_items().is_empty() else -1))
	list.item_activated.connect(func(at): _insert_chosen(at))
	box.add_child(filter)
	box.add_child(list)
	_insert.add_child(box)
	_insert.set_meta("filter", filter)
	_insert.set_meta("list", list)
	layer.add_child(_insert)

# Studs one notch of the wheel moves, scaled to fly speed so it neither crawls nor overshoots.
func _dolly() -> float:
	return maxf(_speed * 0.12, 0.5)

# Toward what is under the pointer, not along the camera's facing, as Studio's wheel is.
func _zoom(way: float, at: Vector2) -> void:
	var dir := _cam.project_ray_normal(at)
	if dir.length_squared() < 0.5:
		dir = -_cam.global_transform.basis.z
	_cam.global_position += dir * _dolly() * way

# ---- the ribbon --------------------------------------------------------------------
# Three rows, as Studio's: a title row with the File menu and the undo arrows, the tab strip,
# then a band of groups captioned underneath. Which verb sits in which group comes from the
# action table, not from here.
const RIBBON := {
	"Home": ["Clipboard", "Tools", "Insert", "Edit", "Test", "Collaborate", "Publish"],
	"Model": ["Tools", "Snap", "Align", "Solid", "Animation", "Edit"],
	"Avatar": ["Rig", "Animation"],
	"Test": ["Test", "Debug", "Players"],
	"Terrain": ["Ground", "Brush"],
	"View": ["Show", "Draw", "Look"],
	"Plugins": ["Manage"],           # plus a group per plugin toolbar
}

# ---- plugins -----------------------------------------------------------------------
# Roblox's local plugins: a .lua / .luau in the plugins folder is a Script run in the edit
# world with a `plugin` global. Its toolbars become groups on the Plugins tab, Selection is
# the Studio's own, and its settings live in a JSON beside the folder.
const PLUGINS_DIR := "user://plugins"
const PLUGIN_SETTINGS_FILE := "user://plugin_settings.json"
var _plugin_buttons: Array = []       # {id, plugin, toolbar, button, tooltip, icon, text, active, enabled}
var _plugin_active := ""              # the plugin that called Activate, if any
var _plugin_exclusive := false        # ... with the exclusive mouse: the view's clicks are its
var _plugin_settings := {}            # plugin -> key -> JSON
var _plugin_edits: Array = []         # what plugins changed since the last ChangeHistoryService waypoint: one undo entry at the next
var _plugin_icons := {}               # action id -> Texture2D, for a button whose Icon is a file

func _load_plugins() -> void:
	if world == null:
		return
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(PLUGINS_DIR))
	_plugin_settings = _read_plugin_settings()
	world.plugin_unload_all()
	_plugin_buttons = []
	_plugin_active = ""
	var dir := DirAccess.open(PLUGINS_DIR)
	var n := 0
	if dir != null:
		var files := dir.get_files()
		files.sort()
		for f in files:
			var ext := f.get_extension().to_lower()
			var model := ext == "rbxmx" or f.to_lower().ends_with(".model.json")
			if ext != "lua" and ext != "luau" and not model:
				continue
			var pname := f.get_basename().get_basename() if f.to_lower().ends_with(".model.json") else f.get_basename()
			var source := FileAccess.get_file_as_string(PLUGINS_DIR + "/" + f)
			var kept: Dictionary = _plugin_settings.get(pname, {})
			for key in kept.keys():
				world.plugin_setting(pname, key, str(kept[key]))
			if model:
				world.plugin_add_file(f, source)   # a model of scripts: each is a plugin script, named for the file
			else:
				world.plugin_add(pname, source)
			n += 1
	_say("studio", "%d plugins from %s" % [n, ProjectSettings.globalize_path(PLUGINS_DIR)], "studio")
	if _tab_bar != null and _tab_bar.current_tab < RIBBON.size() and RIBBON.keys()[_tab_bar.current_tab] == "Plugins":
		_fill_band()

func _open_plugins_folder() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(PLUGINS_DIR))
	OS.shell_open(ProjectSettings.globalize_path(PLUGINS_DIR))

func _read_plugin_settings() -> Dictionary:
	if not FileAccess.file_exists(PLUGIN_SETTINGS_FILE):
		return {}
	var parsed = JSON.parse_string(FileAccess.get_file_as_string(PLUGIN_SETTINGS_FILE))
	return parsed if parsed is Dictionary else {}

func _on_plugin_setting(plugin: String, key: String, json: String) -> void:
	if not _plugin_settings.has(plugin):
		_plugin_settings[plugin] = {}
	_plugin_settings[plugin][key] = json
	var f := FileAccess.open(PLUGIN_SETTINGS_FILE, FileAccess.WRITE)
	if f != null:
		f.store_string(JSON.stringify(_plugin_settings, "  "))

# A plugin's edits, in the shape the History records: property writes, arrivals, moves, and
# departures (kept as a model, so undo puts them back). Its own GUIs are not the place.
func _on_plugin_edit(kind: String, id: int, name: String, before, after) -> void:
	if world == null:
		return
	match kind:
		"property":
			_plugin_edits.append({"kind": "property", "id": id, "name": name, "before": before, "after": after, "label": "%s of %s" % [name, name]})
		"create":
			_plugin_edits.append(_plugin_exists(id, int(after), name, true))
		"parent":
			var was_in := _in_place(int(before))
			var now_in := _in_place(int(after))
			if was_in and now_in:
				_plugin_edits.append({"kind": "parent", "id": id, "before": int(before), "after": int(after), "label": "move %s" % name})
			elif now_in:
				_plugin_edits.append(_plugin_exists(id, int(after), name, true))
			elif was_in:
				var gone := _plugin_exists(id, int(before), name, false)
				gone.model = Model.of(world, id)
				_plugin_edits.append(gone)

func _plugin_exists(id: int, parent: int, name: String, present: bool) -> Dictionary:
	return {"kind": "exists", "present": present, "parent": parent, "parent_path": history._path_of(parent),
			"name": name, "id": id, "model": "", "label": ("insert %s" if present else "remove %s") % name}

func _in_place(id: int) -> bool:
	var at := id
	for hop in 64:
		if at == 0:
			return true
		var info := world.get_instance(at)
		if info.is_empty():
			return false
		var cls := str(info.get("class_name", ""))
		if cls == "CoreGui" or cls == "PluginDebugService":
			return false
		at = int(info.get("parent", -1))
	return false

func _on_plugin_waypoint(name: String) -> void:
	if _plugin_edits.is_empty():
		return
	var items := _plugin_edits
	_plugin_edits = []
	history.begin(name if name != "" else "plugin edit")
	for e in items:
		history._record(e)
	history.end()

func _on_plugin_buttons(buttons: Array) -> void:
	_plugin_buttons = buttons
	_plugin_icons.clear()
	for b in buttons:
		var icon := str(b.get("icon", ""))
		if icon == "" or icon.begins_with("rbxasset") or icon.begins_with("http"):
			continue
		var path := icon if FileAccess.file_exists(icon) else PLUGINS_DIR + "/" + icon
		if not FileAccess.file_exists(path):
			continue
		var img := Image.load_from_file(path)
		if img != null and not img.is_empty():
			if img.get_width() > 18:
				img.resize(18, int(18.0 * img.get_height() / img.get_width()))
			_plugin_icons["plugin_%d" % int(b.get("id", 0))] = ImageTexture.create_from_image(img)
	if _tab_bar != null and _tab_bar.current_tab < RIBBON.size() and RIBBON.keys()[_tab_bar.current_tab] == "Plugins":
		_fill_band()
	else:
		_refresh_commands()

func _on_plugin_active(plugin: String, active: bool, exclusive: bool) -> void:
	_plugin_active = plugin if active else ""
	_plugin_exclusive = active and exclusive

# The folder and a reload, then one action per plugin button, grouped by its toolbar.
func _plugin_actions() -> Array:
	var list := [
		{"id": "plugins_folder", "text": "Plugins Folder", "key": "", "on": true, "tabs": ["Plugins"], "group": "Manage",
		 "tip": "Open the folder local plugins are read from: a .lua / .luau file there is a plugin script, run in the edit world with a `plugin` global",
		 "run": func(): _open_plugins_folder()},
		{"id": "plugins_reload", "text": "Reload Plugins", "key": "", "on": playing == null and world != null, "tabs": ["Plugins"], "group": "Manage",
		 "tip": "Unload every plugin (its Unloading fires) and read the folder again", "run": func(): _load_plugins()},
	]
	for b in _plugin_buttons:
		var id: int = int(b.id)
		list.append({"id": "plugin_%d" % id, "text": str(b.text), "key": "", "on": bool(b.enabled) and playing == null,
					 "tabs": ["Plugins"], "group": str(b.toolbar), "tip": str(b.tooltip), "checked": bool(b.active),
					 "run": func(): _plugin_click(id)})
	return list

func _plugin_click(id: int) -> void:
	if world != null:
		world.plugin_click(id)

func _groups_of(tab: String) -> Array:
	var groups: Array = RIBBON[tab].duplicate()
	if tab == "Plugins":
		for b in _plugin_buttons:
			if not groups.has(str(b.toolbar)):
				groups.append(str(b.toolbar))
	return groups

func _build_ribbon(page: VBoxContainer) -> void:
	var title := PanelContainer.new()
	var title_row := HBoxContainer.new()
	title_row.add_theme_constant_override("separation", 8)
	title.add_child(title_row)
	page.add_child(title)

	var file := MenuButton.new()          # what Studio keeps behind File
	file.text = "File"
	file.flat = false
	var pop := file.get_popup()
	_file_pop = pop
	_recent_pop = PopupMenu.new()
	_recent_pop.name = "Recent"
	_recent_pop.id_pressed.connect(_open_recent)
	pop.add_child(_recent_pop)
	pop.add_item("New Place...", 5)
	pop.add_item("Open Place...", 0)
	pop.add_item("Open Roblox Place (.rbxl)...", 8)
	pop.add_submenu_item("Recent Places", "Recent")
	pop.add_item("Save    Ctrl+S", 1)
	pop.add_item("Reload", 2)
	pop.add_separator()
	pop.add_item("Insert from File...", 6)
	pop.add_item("Save Selection to File...", 7)
	pop.add_item("Save as Roblox Place (.rbxlx)...", 9)
	pop.add_separator()
	pop.add_check_item("Autosave every %d minutes" % int(AUTOSAVE_SECONDS / 60.0), 4)
	pop.add_separator()
	pop.add_item("Close Script", 3)
	pop.id_pressed.connect(_file_menu)
	_fill_recent()
	_refresh_file_menu()
	title_row.add_child(file)
	title_row.add_child(VSeparator.new())
	for id in ["save", "undo", "redo"]:
		title_row.add_child(_command_button(id, false, false))   # icon only, as a quick-access button is
	_status = Label.new()
	_status.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title_row.add_child(_status)

	_tab_bar = TabBar.new()
	for tab in RIBBON:
		_tab_bar.add_tab(tab)
	_tab_bar.tab_changed.connect(func(_i): _fill_band())
	page.add_child(_tab_bar)

	var band := PanelContainer.new()
	_band = HBoxContainer.new()
	_band.add_theme_constant_override("separation", 4)
	band.add_child(_band)
	page.add_child(band)
	_fill_band()

# ---- the debugger ------------------------------------------------------------------
# The gutter is the source of truth: a copy already playing is given whatever it now holds.
func _on_breakpoints_changed(id: int, lines: Array) -> void:
	var full := _full_name(world, id)
	if full == "":
		return
	var before: Array = _breakpoints.get(full, [])
	if lines.is_empty():
		_breakpoints.erase(full)
	else:
		_breakpoints[full] = lines.duplicate()
	if playing != null:
		for l in before:
			if not lines.has(l):
				playing.set_breakpoint(full, int(l), false)
		for l in lines:
			playing.set_breakpoint(full, int(l), true)

func _on_script_shown(id: int) -> void:
	var full := _full_name(world, id)
	_editor.set_breakpoints(_breakpoints.get(full, []))
	if _debug_paused and _debugger.at_script == full:
		_editor.mark_executing(_debugger.line)
	else:
		_editor.clear_executing()

func _on_script_paused(script: String, line: int, frames: Array) -> void:
	_debug_paused = true
	_debugger.show_pause(script, line, frames)
	var sid := _script_named(script)
	if sid != 0:
		_open_script(sid, line)
		_editor.mark_executing(line)
	_say("debugger", "paused at %s:%d" % [script, line], "warn")
	_refresh_commands()

func _on_script_resumed() -> void:
	_debug_paused = false
	_debugger.clear()
	_editor.clear_executing()
	_refresh_commands()

func _debug_go(kind: int) -> void:
	if playing == null or not _debug_paused:
		return
	if kind == 0:
		playing.debug_continue()
	else:
		playing.debug_step(kind)

func _toggle_animation() -> void:
	if _anim.visible:
		_anim.close()
		return
	_anim.world = world
	var rig: int = AnimationEditorScript.rig_of(world, _selection[0]) if not _selection.is_empty() else 0
	if rig == 0:
		_say("studio", "select a rig first: a Model with Motor6D joints between its parts (a character, an NPC)", "warn")
		return
	if not _anim.open_for(rig):
		_say("studio", "that rig's joints do not lead back to one root part; nothing to pose", "warn")

func _file_menu(id: int) -> void:
	match id:
		0: _open_dialog()
		1: save_place()
		2: _confirm("Reload throws away everything not saved. Go on?", func(): open_place(place.dir))
		3: close_script()
		4: set_autosave(not _autosave_on)
		5: new_place_dialog()
		6: insert_from_file()
		7: save_selection_to_file()
		8: import_place_dialog()
		9: save_rbxlx_dialog()

func _fill_band() -> void:
	call_deferred("_no_focus", self)
	for was in _band.get_children():
		_band.remove_child(was)
		was.queue_free()
	# Only the band's own buttons are dropped from the registry: the title row's quick-access
	# ones stand for the same actions and are not rebuilt here.
	for id in _commands.keys():
		var kept := []
		for b in _commands[id]:
			if is_instance_valid(b) and not b.get_meta("band", true):
				kept.append(b)
		if kept.is_empty():
			_commands.erase(id)
		else:
			_commands[id] = kept
	var tab: String = RIBBON.keys()[_tab_bar.current_tab] if _tab_bar.current_tab < RIBBON.size() else "Home"
	var first := true
	for group in _groups_of(tab):
		var body = _group_body(group, tab)
		if body == null:
			continue
		if not first:
			_band.add_child(VSeparator.new())
		first = false
		var stack := VBoxContainer.new()
		stack.add_theme_constant_override("separation", 2)
		stack.add_child(body)
		var caption := Label.new()
		caption.text = group
		caption.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		caption.add_theme_color_override("font_color", palette.text_faint)
		caption.add_theme_font_size_override("font_size", 10)
		stack.add_child(caption)
		_band.add_child(stack)
	_refresh_commands()

func _group_body(group: String, tab: String):
	if group == "Snap":
		return _snap_group()
	if group == "Show":
		return _show_group()
	if group == "Look":
		return _look_group()
	if group == "Draw":
		return _draw_group()
	if group == "Align":
		return _align_group()
	if group == "Players":
		return _players_group()
	if group == "Collaborate":
		return _collaborate_group()
	if group == "Ground":
		return _ground_group()
	if group == "Brush":
		return _brush_group()
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 3)
	var any := false
	for action in actions():
		if action.get("group", "") != group or not action.get("tabs", []).has(tab):
			continue
		row.add_child(_command_button(action.id))
		any = true
	return row if any else null

# Studio has two snap grids, one for moving and one for turning; so does this.
func _snap_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	var studs := SpinBox.new()
	studs.min_value = 0
	studs.max_value = 16
	studs.step = 0.25
	studs.value = _handles.snap if _handles != null else 1.0
	studs.prefix = "move "
	studs.suffix = "studs"
	studs.tooltip_text = "Studs a move or a scale lands on. 0 for none."
	studs.value_changed.connect(func(v): _handles.snap = v)
	var turn := SpinBox.new()
	turn.min_value = 0
	turn.max_value = 90
	turn.step = 5
	turn.value = _handles.rotate_snap if _handles != null else 15.0
	turn.prefix = "turn "
	turn.suffix = "deg"
	turn.tooltip_text = "Degrees a turn lands on. 0 for none."
	turn.value_changed.connect(func(v): _handles.rotate_snap = v)
	row.add_child(studs)
	row.add_child(turn)
	return row

func _look_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	for which in Palette.NAMES:
		var b := Button.new()
		b.text = which
		b.toggle_mode = true
		b.button_pressed = palette.name == which
		b.tooltip_text = {"Dark": "Dark panels", "Light": "Light panels",
						  "Clear": "Panels you can see the place through"}.get(which, which)
		b.pressed.connect(func(): set_look(which))
		row.add_child(b)
	return row

# Name and Roblox's own Material enum value.
const TERRAIN_MATS := [["Grass", 1280], ["LeafyGrass", 1284], ["Sand", 1296], ["Rock", 896], ["Snow", 1328],
	["Mud", 1344], ["Ground", 1360], ["Asphalt", 1376], ["Basalt", 788], ["CrackedLava", 804], ["Glacier", 1552],
	["Ice", 1536], ["Limestone", 820], ["Pavement", 836], ["Salt", 1392], ["Sandstone", 912], ["Slate", 800],
	["Cobblestone", 880], ["Concrete", 816], ["Brick", 848], ["WoodPlanks", 528], ["Water", 2048]]

# One Terrain in the Workspace, as in Roblox: everything is sculpted into that one instance.
func _ground_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	var make := Button.new()
	make.text = "Create"
	make.tooltip_text = "Lay a flat field to sculpt. Replaces whatever ground is there."
	make.pressed.connect(func(): _make_ground())
	row.add_child(make)
	var span := SpinBox.new()
	span.min_value = 8
	span.max_value = 256
	span.step = 8
	span.value = _ground_res
	span.prefix = "grid "
	span.tooltip_text = "Squares across. More is finer ground and a bigger file."
	span.value_changed.connect(func(v): _ground_res = int(v))
	row.add_child(span)
	return row

func _brush_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	var picked := OptionButton.new()
	for one in ["Add", "Subtract", "Smooth", "Flatten", "Paint"]:
		picked.add_item(one)
	picked.selected = _terrain.mode
	picked.item_selected.connect(func(i):
		_terrain.mode = i
		set_sculpting(true))
	row.add_child(picked)
	for one in [["size", "radius"], ["strength", "strength"]]:
		var box := SpinBox.new()
		box.min_value = 1
		box.max_value = 128
		box.step = 1
		box.value = _terrain.radius if one[1] == "radius" else _terrain.strength
		box.prefix = one[0] + " "
		box.value_changed.connect(func(v):
			if one[1] == "radius": _terrain.radius = v
			else: _terrain.strength = v)
		row.add_child(box)
	var stuff := OptionButton.new()
	for one in TERRAIN_MATS:
		stuff.add_item(one[0])
	for i in TERRAIN_MATS.size():
		if TERRAIN_MATS[i][1] == _terrain.material:
			stuff.selected = i
	stuff.item_selected.connect(func(i): _terrain.material = TERRAIN_MATS[i][1])
	row.add_child(stuff)
	var on := Button.new()
	on.text = "Sculpt"
	on.toggle_mode = true
	on.button_pressed = _sculpting
	on.tooltip_text = "The brush takes the pointer: drag on the ground to shape it"
	on.toggled.connect(func(v): set_sculpting(v))
	row.add_child(on)
	return row

# The brush and the handles cannot both own the left button.
func set_sculpting(on: bool) -> void:
	_sculpting = on
	if on:
		_select([])
	_refresh_commands()

func _make_ground() -> void:
	if world == null or playing != null:
		return
	if _terrain.world != world:
		_terrain.world = world
	_terrain.make_ground(_ground_res, 4.0, 0.0)
	_say("studio", "laid %d by %d studs of ground to sculpt" % [_ground_res * 4, _ground_res * 4], "studio")

func _players_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	var many := SpinBox.new()
	many.min_value = 0
	many.max_value = 8
	many.step = 1
	many.value = _test_players
	many.prefix = "+ "
	many.suffix = "players"
	many.tooltip_text = "Extra players seated when you Play, besides you. They have\ncharacters and fire PlayerAdded, but nobody is driving them."
	many.value_changed.connect(func(v):
		_test_players = int(v)
		_set_setting("studio", "test_players", _test_players))
	row.add_child(many)
	return row

func _draw_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	var ui := Button.new()
	ui.text = "UI"
	ui.toggle_mode = true
	ui.button_pressed = _gui_preview
	ui.tooltip_text = "Draw StarterGui over the view, as it will look in the game"
	ui.toggled.connect(func(on): set_gui_preview(on))
	row.add_child(ui)
	var grid := Button.new()
	grid.text = "Grid"
	grid.toggle_mode = true
	grid.button_pressed = _grid != null and _grid.visible
	grid.toggled.connect(func(on): _grid.visible = on)
	row.add_child(grid)
	var hulls := Button.new()
	hulls.text = "Collision"
	hulls.toggle_mode = true
	hulls.button_pressed = _collision_shown
	hulls.tooltip_text = "What the physics collides with, which is not always what is drawn"
	hulls.toggled.connect(func(on): _show_collision(on))
	row.add_child(hulls)
	return row

func _align_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	for one in [["X", 0], ["Y", 1], ["Z", 2]]:
		var b := Button.new()
		b.text = one[0]
		b.tooltip_text = "Line the selection up on %s (shift: spread it evenly)" % one[0]
		b.pressed.connect(func():
			if Input.is_key_pressed(KEY_SHIFT): distribute(one[1])
			else: align(one[1], 0))
		row.add_child(b)
	return row

func _show_group() -> HBoxContainer:
	var row := HBoxContainer.new()
	for name in ["Explorer", "Properties", "Output"]:
		var b := Button.new()
		b.text = name
		b.toggle_mode = true
		b.button_pressed = _docks.has(name) and _docks[name].visible
		b.toggled.connect(func(on): if _docks.has(name): _docks[name].visible = on)
		row.add_child(b)
	return row

# An action can have more than one button standing for it -- Undo is on the Home tab and in
# the title row at once -- so _commands holds a list per id.
func _command_button(id: String, labelled := true, in_band := true) -> Button:
	var b := Button.new()
	var art: Texture2D = Icons.icon(id, 18)
	if art == null and _plugin_icons.has(id):
		art = _plugin_icons[id]
	if art != null:
		b.icon = art
		if labelled:
			# Icon over label, as a ribbon button reads.
			b.vertical_icon_alignment = VERTICAL_ALIGNMENT_TOP
			b.alignment = HORIZONTAL_ALIGNMENT_CENTER
	b.set_meta("labelled", labelled or art == null)
	b.set_meta("band", in_band)
	b.pressed.connect(func(): _do(id))
	if not _commands.has(id):
		_commands[id] = []
	_commands[id].append(b)
	return b

# Text, tooltip and enabled state all come from the action table.
func _refresh_commands() -> void:
	if _commands.is_empty():
		return
	for action in actions():
		for b in _commands.get(action.id, []):
			if not is_instance_valid(b):
				continue
			if b.get_meta("labelled", true):
				b.text = action.text
			b.disabled = not action.on
			b.tooltip_text = action.get("tip", "%s  %s" % [action.text, action.key] if action.get("key", "") != "" else action.text)
			if action.has("checked"):
				b.toggle_mode = true
				b.button_pressed = action.checked

func _dock(title: String, body: Control, head: Control = null) -> PanelContainer:
	var panel := PanelContainer.new()
	_docks[title] = panel
	panel.custom_minimum_size = Vector2(DOCK_MIN, 0)
	var box := VBoxContainer.new()
	var label := Label.new()
	label.text = title
	label.add_theme_color_override("font_color", palette.text_dim)
	box.add_child(label)
	if head != null:
		box.add_child(head)      # the filter box, the level picker
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(body)
	panel.add_child(box)
	return panel

func _button(text: String, action: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.focus_mode = Control.FOCUS_NONE     # a ribbon button never keeps the keyboard: Space is the game's
	b.pressed.connect(action)
	return b

# Nothing in the chrome holds keyboard focus, as in Roblox Studio: after Play the keys go to
# the game, not the Stop button just clicked. Only BaseButtons, so text boxes keep theirs.
func _no_focus(node: Node) -> void:
	if node is BaseButton:
		node.focus_mode = Control.FOCUS_NONE
	for kid in node.get_children():
		_no_focus(kid)

func _tool(mode: int) -> void:
	_handles.mode = mode
	_refresh_commands()

