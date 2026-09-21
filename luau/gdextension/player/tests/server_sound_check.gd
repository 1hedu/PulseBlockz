# Does a sound the SERVER plays reach a joined player?
#
#   godot --headless --path . -s res://tests/server_sound_check.gd -- [host:port]
#
# The trampoline's bounce is made and played by a server script on the bouncing character's
# root: the class of sound a player hears because somebody else acted. Walks the character onto
# the pad and watches the Sound the server made, on this machine's own clock.
extends SceneTree

var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://server-sound"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s): lines.append(String(s)))
		w.script_warn.connect(func(n, s): lines.append("WARN " + String(s)))
		w.script_error.connect(func(n, s): lines.append("ERR " + String(s))))
	_run.call_deferred()

func _run() -> void:
	var where := "play.safewrap.xyz:8800"
	for a in OS.get_cmdline_user_args():
		if a.contains(":") and not a.begins_with("--"):
			where = a
	var hp: Dictionary = player._split_server(where)
	print("SOUND join ", await player.join(String(hp.host), int(hp.port), true))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 45.0:
		phase = 1
		w.run_client_chunk("bounce", """
local Players = game:GetService("Players")
local me = Players.LocalPlayer
local pad = nil
for _, d in ipairs(workspace:GetDescendants()) do
	if d:IsA("BasePart") and d.Name:lower():find("trampoline") then pad = d break end
end
print("SOUND pad: " .. (pad and (pad:GetFullName() .. " at " .. tostring(pad.Position)) or "none found"))
local char = me.Character or me.CharacterAdded:Wait()
local root = char:WaitForChild("HumanoidRootPart", 10)
if pad and root then
	root.CFrame = CFrame.new(pad.Position + Vector3.new(0, 6, 0))
end
-- Every Sound the server made, watched for a minute: the one that plays, and whether this
-- machine's own clock moves inside it.
local seen = {}
local ticks = 0
while ticks < 300 do
	task.wait(0.1) ticks += 1
	for _, d in ipairs(workspace:GetDescendants()) do
		if d:IsA("Sound") and d.Playing and not seen[d] then
			seen[d] = true
			local at0 = d.TimePosition
			task.spawn(function()
				task.wait(0.3)
				print("SOUND played: " .. d:GetFullName() .. " loaded=" .. tostring(d.IsLoaded)
					.. " length=" .. string.format("%.2f", d.TimeLength)
					.. " clock " .. string.format("%.2f -> %.2f", at0, d.TimePosition)
					.. " loudness=" .. string.format("%.0f", d.PlaybackLoudness))
			end)
		end
	end
	if ticks % 50 == 0 and root and pad then
		root.CFrame = CFrame.new(pad.Position + Vector3.new(0, 6, 0))
	end
end
print("SOUND watch done")
""")
	elif phase == 1 and t > 125.0:
		for l in lines:
			if l.begins_with("SOUND") or l.begins_with("ERR"):
				print("   ", l.substr(0, 200))
		quit(0)
	return false
