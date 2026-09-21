# What a joined player gets: godot --headless --path . -s res://tests/join_verify.gd -- host:port
#
# Proves the wait card stays black and says what the place is -- the picture is the place's own to
# draw, and two of them is a picture, black, and the picture again -- and that the place's assets
# and its cursor reach the client.
extends SceneTree

var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []
var card_seen := ""

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://join-verify"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s): lines.append(String(s)))
		w.script_warn.connect(func(n, s): lines.append("WARN " + String(s)))
		w.script_error.connect(func(n, s): lines.append("ERR " + String(s))))
	_run.call_deferred()

func _run() -> void:
	var where := "127.0.0.1:8899"
	for a in OS.get_cmdline_user_args():
		if a.contains(":") and not a.begins_with("--"):
			where = a
	var hp: Dictionary = player._split_server(where)
	var got: Dictionary = await player.join(String(hp.host), int(hp.port), true)
	print("VERIFY join ", got)

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	# Sampled every frame the card is up: _wait is gone once the place arrives.
	if player._wait != null and is_instance_valid(player._wait):
		var found: String = player._server_splash(w)
		if found != "" and card_seen == "":
			card_seen = found
			var drawn: bool = player._wait_card != null and is_instance_valid(player._wait_card) 				and player._wait_card.texture != null
			print("VERIFY the server sends a picture (%s) and the card does not wear it: %s"
				% [found.substr(0, 40), not drawn])
	if phase == 0 and t > 60.0:
		phase = 1
		print("VERIFY card still up at 60s: %s" % (player._wait != null and is_instance_valid(player._wait)))
		print("VERIFY a picture was found for it: %s" % (card_seen if card_seen != "" else "none"))
		w.run_client_chunk("cursor", """
local rs = game:GetService("ReplicatedStorage")
local uis = game:GetService("UserInputService")
local folder = rs:WaitForChild("PlaceAssets", 20)
local names = {}
for _, v in ipairs(folder and folder:GetChildren() or {}) do names[v.Name] = v.Value end
print("VERIFY assets: " .. tostring(folder and #folder:GetChildren() or 0)
	.. " cursor=" .. (names.Cursor and "yes" or "no")
	.. " splash=" .. (names.Splash and "yes" or "no"))
task.wait(4)
print("VERIFY MouseIcon: " .. (uis.MouseIcon ~= "" and uis.MouseIcon:sub(1, 46) or "not set"))
""")
	elif phase == 1 and t > 75.0:
		for l in lines:
			if l.begins_with("VERIFY") or l.begins_with("ERR") or l.begins_with("WARN cursor"):
				print("   ", l.substr(0, 200))
		quit(0)
	return false
