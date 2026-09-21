# Joins a server through the Player and dumps what the client side did: every script line, the
# scripts that exist on the client and which of them started, the local player's GUI and character.
#
#   godot --headless --path . -s res://tests/join_probe.gd -- host:port
extends SceneTree
var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://join-probe"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s): lines.append("PRINT %s: %s" % [String(n), String(s)]))
		w.script_warn.connect(func(n, s): lines.append("WARN %s: %s" % [String(n), String(s)]))
		w.script_error.connect(func(n, s): lines.append("ERR %s: %s" % [String(n), String(s)]))
		w.script_killed.connect(func(n, s): lines.append("KILLED %s: %s" % [String(n), String(s)])))
	_run.call_deferred()

func _run() -> void:
	var where := "127.0.0.1:8899"
	for a in OS.get_cmdline_user_args():
		if a.contains(":") and not a.begins_with("--"):
			where = a
	var hp: Dictionary = player._split_server(where)
	var got: Dictionary = await player.join(String(hp.host), int(hp.port), true)
	print("PROBE join ", got)

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 45.0:
		phase = 1
		print("PROBE card still up at 45s: %s" % (player._wait != null and is_instance_valid(player._wait)))
		w.run_client_chunk("probe", """
local Players = game:GetService("Players")
local lp = Players.LocalPlayer
print("PROBE LocalPlayer: " .. tostring(lp and lp.Name) .. " character=" .. tostring(lp and lp.Character and lp.Character.Name))
local sps = game:GetService("StarterPlayer"):FindFirstChildOfClass("StarterPlayerScripts")
local n = 0
for _, d in ipairs(sps and sps:GetDescendants() or {}) do if d:IsA("LocalScript") then n += 1 end end
print("PROBE StarterPlayerScripts: " .. tostring(sps ~= nil) .. " localscripts=" .. n)
local ps = lp and lp:FindFirstChildOfClass("PlayerScripts")
local m, on = 0, 0
for _, d in ipairs(ps and ps:GetDescendants() or {}) do
	if d:IsA("LocalScript") then m += 1 if d.Enabled then on += 1 end end
end
print("PROBE PlayerScripts: " .. tostring(ps ~= nil) .. " localscripts=" .. m .. " enabled=" .. on)
local pg = lp and lp:FindFirstChildOfClass("PlayerGui")
local guis = {}
for _, g in ipairs(pg and pg:GetChildren() or {}) do table.insert(guis, g.Name .. (g:IsA("ScreenGui") and (g.Enabled and "(on)" or "(off)") or "")) end
print("PROBE PlayerGui: " .. table.concat(guis, ", "))
local sg = game:GetService("StarterGui")
local sgs = {}
for _, g in ipairs(sg:GetChildren()) do table.insert(sgs, g.Name) end
print("PROBE StarterGui: " .. table.concat(sgs, ", "))
local rs = game:GetService("ReplicatedStorage")
local names = {}
for _, v in ipairs(rs:GetChildren()) do table.insert(names, v.Name) end
print("PROBE ReplicatedStorage: " .. table.concat(names, ", "))
print("PROBE MouseIcon: " .. tostring(game:GetService("UserInputService").MouseIcon ~= ""))
""")
	elif phase == 1 and t > 52.0:
		var counts := {"PRINT": 0, "WARN": 0, "ERR": 0, "KILLED": 0}
		for l in lines:
			var k: String = l.split(" ")[0]
			counts[k] = counts.get(k, 0) + 1
		print("PROBE lines: ", counts)
		for l in lines:
			if l.begins_with("ERR") or l.begins_with("KILLED") or l.begins_with("WARN") or l.find("PROBE") != -1:
				print("   ", l.substr(0, 240))
		print("PROBE first prints:")
		var shown := 0
		for l in lines:
			if l.begins_with("PRINT") and l.find("PROBE") == -1 and shown < 25:
				print("   ", l.substr(0, 200))
				shown += 1
		quit(0)
	return false
