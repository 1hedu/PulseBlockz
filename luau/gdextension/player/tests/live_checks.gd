# Checks on a joined server: godot --headless --path . -s res://tests/live_checks.gd -- [host:port] [--no-wallet]
# Plays one of the place's own sounds here and watches its clock move, then asks the place's
# trading desk for a quote the way its panel does.
extends SceneTree

var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []
var allow := true

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://live-checks"
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
		if a == "--no-wallet":
			allow = false
		elif a.contains(":") and not a.begins_with("--"):
			where = a
	var hp: Dictionary = player._split_server(where)
	print("CHECK wallet allowed: ", allow)
	print("CHECK join ", await player.join(String(hp.host), int(hp.port), allow))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 50.0:
		phase = 1
		w.run_client_chunk("sound", """
local rs = game:GetService("ReplicatedStorage")
local folder = rs:WaitForChild("PlaceAssets", 20)
local best, bestLen = nil, 0
for _, v in ipairs(folder and folder:GetChildren() or {}) do
	if v:IsA("StringValue") and v.Name:lower():find("music") then best = v break end
end
if not best then
	for _, v in ipairs(folder and folder:GetChildren() or {}) do
		if v:IsA("StringValue") and (v.Value:find("wav") or v.Value:find("ogg") or v.Value:find("mp3")) then best = v break end
	end
end
local s = Instance.new("Sound")
s.Name = "CheckSound"
s.SoundId = best and best.Value or ""
s.Volume = 1
s.Looped = true
s.Parent = workspace
local waited = 0
while not s.IsLoaded and waited < 10 do task.wait(0.25) waited += 0.25 end
print("CHECK sound " .. (best and best.Name or "none") .. " loaded=" .. tostring(s.IsLoaded) .. " length=" .. string.format("%.2f", s.TimeLength))
s:Play()
task.wait(0.1)
local a = s.TimePosition
task.wait(0.8)
print("CHECK sound playing=" .. tostring(s.IsPlaying) .. " clock " .. string.format("%.2f -> %.2f", a, s.TimePosition) .. " loudness=" .. tostring(s.PlaybackLoudness))
s:Destroy()
""")
	elif phase == 1 and t > 62.0:
		phase = 2
		w.run_client_chunk("trade", """
local rs = game:GetService("ReplicatedStorage")
local remote = rs:WaitForChild("TradeRemote", 20)
if not remote then print("CHECK trade: no TradeRemote") return end
local seen = {}
remote.OnClientEvent:Connect(function(kind, payload)
	if seen[kind] then return end
	seen[kind] = true
	if kind == "market" then
		local n = payload and payload.tokens and #payload.tokens or 0
		print("CHECK market: " .. tostring(n) .. " token(s), can_sign=" .. tostring(payload and payload.can_sign) .. " note=" .. tostring(payload and payload.note))
	elseif kind == "quoted" then
		print("CHECK quoted: ok=" .. tostring(payload and payload.ok) .. " out=" .. tostring(payload and payload.out) .. " message=" .. tostring(payload and payload.message))
	end
end)
remote:FireServer("watch")
task.wait(3)
remote:FireServer("quote", "", "0x8a810ea8B121d08342E9e7696f4a9915cBE494B7", "1", 50)
task.wait(25)
print("CHECK trade: done waiting")
""")
	elif phase == 2 and t > 105.0:
		for l in lines:
			if l.begins_with("CHECK") or l.begins_with("WARN") or l.begins_with("ERR"):
				print("   ", l.substr(0, 220))
		quit(0)
	return false
