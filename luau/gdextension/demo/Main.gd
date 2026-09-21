# The demo scene: a PulseBlockzWorld named "World" runs res://scripts (a Rojo layout) on worker
# threads, never blocking this node, and mirrors every Part it puts under Workspace into the
# scene as a body and mesh. This script forwards console output, frame stats and ScriptSync loads.
extends Node3D

@onready var world: PulseBlockzWorld = $World

# Command line, after "--": --server[=PORT] (8800), --connect=HOST[:PORT], --name=, --user=.
# With none of them the mode is Play Solo: server and client in one, Scripts running on the server
# side and LocalScripts on the client as on Roblox, and a player named `player_name` joining after
# the first frame with a character (WASD/space) and an orbit camera (right-drag, wheel).
# In _enter_tree, not _ready: the World reads these when its own _ready runs.
func _enter_tree():
	var w: PulseBlockzWorld = $World
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--server"):
			w.mode = PulseBlockzWorld.MODE_SERVER
			w.listen_port = int(a.get_slice("=", 1)) if a.contains("=") else 8800
		elif a.begins_with("--connect="):
			var host := a.get_slice("=", 1)
			w.mode = PulseBlockzWorld.MODE_CLIENT
			w.server_address = host.get_slice(":", 0)
			if host.contains(":"): w.server_port = int(host.get_slice(":", 1))
		elif a.begins_with("--name="):
			w.player_name = a.get_slice("=", 1)
		elif a.begins_with("--user="):
			w.user_id = int(a.get_slice("=", 1))

func _ready():
	if world.mode == PulseBlockzWorld.MODE_SERVER:
		print("server: listening on %d" % world.listen_port)
		world.client_joined.connect(func(n, id, peer): print("%s joined as player %d (peer %d)" % [n, id, peer]))
		world.client_left.connect(func(n, id, peer): print("%s left (player %d)" % [n, id]))
	elif world.mode == PulseBlockzWorld.MODE_CLIENT:
		print("client: connecting to %s:%d as %s" % [world.server_address, world.server_port, world.player_name])
		world.server_connected.connect(func(id): print("connected as player %d" % id))
		world.server_disconnected.connect(func(): print("server gone"))
	world.threaded = true                # default; set false for inline/lockstep
	world.max_millis_per_call = 4.0      # one thread, one resumption
	world.max_frame_millis = 8.0         # all scripts, one frame
	world.max_memory_mb = 32
	world.script_print.connect(func(n, t): print("[%s] %s" % [n, t]))
	world.script_warn.connect(func(n, t): push_warning("[%s] %s" % [n, t]))
	world.script_error.connect(func(n, e): push_error("[%s] %s" % [n, e]))
	world.script_killed.connect(_on_killed)
	world.frame_finished.connect(_on_frame)
	world.player_joined.connect(func(n, id): print("%s joined as player %d" % [n, id]))
	world.leave_game.connect(func(): get_tree().quit())    # the escape menu's Leave: no lobby to go back to
	$ScriptSync.script_loaded.connect(func(n, again): print("%s %s" % [n, "reloaded" if again else "loaded"]))

# A runaway script is killed again every frame: warn once, with the reason, then count.
var _kills := {}
func _on_killed(script_name: String, reason: String):
	var n: int = _kills.get(script_name, 0) + 1
	_kills[script_name] = n
	if n == 1:
		push_warning("%s killed: %s" % [script_name, reason])
	elif n % 300 == 0:
		print("%s killed %d times (still over budget)" % [script_name, n])

var _worst := 0.0
var _last := {}
func _on_frame(stats: Dictionary):
	_worst = max(_worst, stats.millis)
	_last = stats

func _process(_d):
	if Engine.get_process_frames() % 120 == 0 and not _last.is_empty():
		print("fps %d  worker frame worst %.2f ms  %d instances / %d parts  %d threads" % [
			Engine.get_frames_per_second(), _worst, _last.instances, _last.parts, _last.threads_live])
		_worst = 0.0
