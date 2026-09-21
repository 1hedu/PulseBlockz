# Round two, narrowed: where does a scan page stop? Watches the two attributes the whole
# channel is made of -- ReplicatedStorage.Chain.AskScan (what the client asked for) and
# .ScanResult (what the host answered) -- while asking for two pages.
#
#   godot --headless --path . -s res://tests/explorer_scan_probe.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://explorer-scan-probe"
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
	print("SCANP play ", got.get("ok", false), " ", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 32.0:
		phase = 1
		w.run_client_chunk("scan_channel", """
local rs = game:GetService("ReplicatedStorage")
local function T() return string.format("%.0f", os.clock()) end
local function say(s) print("SCANP t=" .. T() .. " " .. s) end

local chain = rs:WaitForChild("Chain", 25)
say("Chain=" .. tostring(chain ~= nil) .. " AskScanHandles=" .. tostring(chain and chain:GetAttribute("AskScanHandles")))

local Scan = require(rs:WaitForChild("Scan"))

local answered = {}
chain:GetAttributeChangedSignal("ScanResult"):Connect(function()
	local raw = chain:GetAttribute("ScanResult")
	say("ScanResult CHANGED len=" .. tostring(type(raw) == "string" and #raw or -1) .. " head=" .. tostring(raw):sub(1, 120))
end)

say("asking home")
Scan.page("home", "", function(p)
	answered.home = true
	say("HANDLER home ok=" .. tostring(p and p.ok) .. " message=" .. tostring(p and p.message) .. " title=" .. tostring(p and p.title))
end)

task.wait(35)
say("asking blocks (the first one has had 35s)")
Scan.page("blocks", "", function(p)
	answered.blocks = true
	say("HANDLER blocks ok=" .. tostring(p and p.ok) .. " message=" .. tostring(p and p.message))
end)

for i = 1, 16 do
	local ask = chain:GetAttribute("AskScan")
	say(("watch %d  ScanSeq=%s  AskScan=%s  ScanResult=%s"):format(i,
		tostring(chain:GetAttribute("ScanSeq")),
		tostring(ask),
		tostring(chain:GetAttribute("ScanResult")):sub(1, 90)))
	task.wait(6)
end
say("end: home answered=" .. tostring(answered.home == true) .. " blocks answered=" .. tostring(answered.blocks == true))
""")
	elif phase == 1 and t > 178.0:
		for l in lines:
			if l.find("SCANP") != -1 or l.begins_with("ERR") or l.find("Scan") != -1:
				print("   ", l.substr(0, 300))
		quit(0)
	return false
