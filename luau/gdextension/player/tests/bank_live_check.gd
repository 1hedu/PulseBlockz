# What the teller offers to send, on a joined server: nothing is signed, it stops at the menu.
#
#   godot --headless --path . -s res://tests/bank_live_check.gd -- [host:port]
extends SceneTree

var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://bank-check"
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
	print("BANKLIVE join ", await player.join(String(hp.host), int(hp.port), true))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 55.0:
		phase = 1
		w.run_client_chunk("bank", """
local rs = game:GetService("ReplicatedStorage")
local remote = rs:WaitForChild("BankRemote", 25)
if not remote then print("BANKLIVE no BankRemote") return end
local step = nil
remote.OnClientEvent:Connect(function(kind, payload)
	if kind == "say" or kind == "open" then step = payload end
end)
local function waitForStep()
	step = nil
	local waited = 0
	while step == nil and waited < 15 do task.wait(0.25) waited += 0.25 end
	return step
end
remote:FireServer("ask_send", nil, "")
waitForStep()
remote:FireServer("send_to", nil, "0x000000000000000000000000000000000000dEaD")
local got = waitForStep()
local offered = 0
for _, item in ipairs(got and got.menu or {}) do
	if item.id == "send_token" then
		offered += 1
		print("BANKLIVE offered: " .. tostring(item.label))
	end
end
print("BANKLIVE the teller offered " .. tostring(offered) .. " thing(s) to send")
""")
	elif phase == 1 and t > 85.0:
		for l in lines:
			if l.begins_with("BANKLIVE") or l.begins_with("ERR"):
				print("   ", l.substr(0, 200))
		quit(0)
	return false
