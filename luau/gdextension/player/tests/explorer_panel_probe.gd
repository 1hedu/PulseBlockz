# Round three: what the Explorer window actually shows after a page that did not arrive,
# and whether asking Unger for the scan a second time retries it. Reads the panel's own
# labels out of PlayerGui rather than guessing.
#
#   godot --headless --path . -s res://tests/explorer_panel_probe.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://explorer-panel-probe"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s): lines.append("PRINT %s" % String(s)))
		w.script_warn.connect(func(n, s): lines.append("WARN %s: %s" % [String(n), String(s)]))
		w.script_error.connect(func(n, s): lines.append("ERR %s: %s" % [String(n), String(s)])))
	_run.call_deferred()

func _run() -> void:
	var where := "127.0.0.1:8899"
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://") or (a.contains(":") and not a.begins_with("--")):
			where = a
	var got: Dictionary = await player.play(where)
	print("PANEL play ", got.get("ok", false), " ", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 32.0:
		phase = 1
		w.run_client_chunk("explorer_panel", """
local rs = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")
local gui = Players.LocalPlayer:WaitForChild("PlayerGui")
local function T() return string.format("%.0f", os.clock()) end
local function say(s) print("PANEL t=" .. T() .. " " .. s) end

local board = rs:WaitForChild("ExplorerRemote", 25)
board.OnClientEvent:Connect(function(kind, payload)
	say("event " .. tostring(kind) .. " " .. (kind == "note" and tostring(payload) or ""))
end)

local function dump(label)
	local screen = gui:FindFirstChild("Explorer")
	if not screen then say(label .. ": no Explorer ScreenGui") return end
	local texts = {}
	local scrolls = 0
	for _, d in ipairs(screen:GetDescendants()) do
		if d:IsA("ScrollingFrame") then
			local drawn = 0
			for _, c in ipairs(d:GetChildren()) do
				if c:IsA("Frame") or c:IsA("TextLabel") or c:IsA("TextButton") then drawn = drawn + 1 end
			end
			scrolls = scrolls + 1
			say(("%s: page ScrollingFrame has %d drawn children"):format(label, drawn))
		end
		if (d:IsA("TextLabel") or d:IsA("TextBox")) and d.Text ~= "" then
			table.insert(texts, string.format("%q", d.Text:sub(1, 70)))
		end
	end
	say(("%s: Enabled=%s scrollingframes=%d labels=%s"):format(label, tostring(screen.Enabled), scrolls, table.concat(texts, " ")))
end

task.wait(3)
say("asking the town to open the explorer")
board:FireServer("open")
task.wait(62)
dump("after 62s")
task.wait(12)
dump("after 74s")
say("asking again, the way walking back to Unger would")
board:FireServer("open")
task.wait(20)
dump("after a second open")
say("cutting an Engram of a block, which needs no page")
board:FireServer("keep", "block", "25440755")
task.wait(8)
dump("after keep")
say("done")
""")
	elif phase == 1 and t > 165.0:
		for l in lines:
			if l.find("PANEL") != -1 or l.begins_with("ERR") or l.find("Engram") != -1:
				print("   ", l.substr(0, 400))
		quit(0)
	return false
