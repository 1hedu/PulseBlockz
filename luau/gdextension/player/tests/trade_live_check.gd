# The swap card's balances on a joined server: read off the player's own machine by way of the
# desk, the whole path the Max button depends on, over the wire rather than in one process.
#   godot --headless --path . -s res://tests/trade_live_check.gd -- host:port
extends SceneTree

var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://trade-check"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s): lines.append(String(s)))
		w.script_error.connect(func(n, s): lines.append("ERR " + String(s))))
	_run.call_deferred()

func _run() -> void:
	var where := "play.safewrap.xyz:8800"
	for a in OS.get_cmdline_user_args():
		if a.contains(":") and not a.begins_with("--"):
			where = a
	var hp: Dictionary = player._split_server(where)
	print("TRADE join ", await player.join(String(hp.host), int(hp.port), true))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 55.0:
		phase = 1
		w.run_client_chunk("trade", """
local rs = game:GetService("ReplicatedStorage")
local remote = rs:WaitForChild("TradeRemote", 25)
if not remote then print("TRADE no TradeRemote") return end
local done = false
remote.OnClientEvent:Connect(function(kind, payload)
	if kind ~= "market" or done or (payload and payload.loading) then return end
	done = true
	for _, token in ipairs(payload.tokens or {}) do
		if token.symbol == "PLS" or token.symbol == "PLSX" then
			print(("TRADE %s: balance '%s', units '%s'"):format(token.symbol,
				tostring(token.balance), tostring(token.balance_units)))
		end
	end
end)
remote:FireServer("open")
task.wait(25)
if not done then print("TRADE the desk never answered") end
""")
	elif phase == 1 and t > 90.0:
		for l in lines:
			if l.begins_with("TRADE") or l.begins_with("ERR"):
				print("   ", l.substr(0, 200))
		quit(0)
	return false
