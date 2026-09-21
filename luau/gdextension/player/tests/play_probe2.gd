# Second pass over the same area: the two things the first probe could not see.
#
#  * HeartsRemote pushes once on spawn, before a client chunk can be listening, so the first
#    probe only saw its effect (the Hearts ScreenGui switched on). This one makes the town
#    push again, by going over the edge -- Health.luau kills anything below Y=-25.
#  * The rod Funmaster Mike hands over is an item, not a thing in your hand. This wears it and
#    then works the rod WITHOUT casting: a cast is a transaction and is not this probe's to send.
#
#   godot --headless --path . -s res://tests/play_probe2.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://play-probe"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(_n, s):
			var line := String(s)
			if line.begins_with("PLAY2"):
				print(line))
		w.script_warn.connect(func(n, s): print("PLAY2WARN %s: %s" % [String(n), String(s)]))
		w.script_error.connect(func(n, s): print("PLAY2ERR %s: %s" % [String(n), String(s)])))
	_run.call_deferred()

func _run() -> void:
	var where := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://"):
			where = a
	var got: Dictionary = await player.play(where)
	print("PLAY2 play ok=", got.get("ok", false), " ", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 30.0:
		phase = 1
		w.run_client_chunk("play2", CHUNK)
	elif phase == 1 and t > 165.0:
		print("PLAY2 probe done")
		quit(0)
	return false

const CHUNK := """
local rs = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")
local me = Players.LocalPlayer
local function say(s) print("PLAY2 " .. s) end
local function T() return string.format("t=%.0f", os.clock()) end

local hearts = rs:WaitForChild("HeartsRemote", 25)
local sfx = rs:FindFirstChild("CombatSfx")
local news = rs:FindFirstChild("CombatNews")
local fish = rs:FindFirstChild("FishingRemote")
local fun = rs:FindFirstChild("FunRemote")
local wardrobe = rs:FindFirstChild("WardrobeRemote")
local notify = rs:FindFirstChild("Notify")

if hearts then
	hearts.OnClientEvent:Connect(function(half, maxHalf, fire, colours, fights)
		say(("HEARTS %s %s/%s fire=%s colours=%s fights=%s"):format(T(), tostring(half),
			tostring(maxHalf), tostring(fire),
			tostring(type(colours) == "table" and #colours or colours), tostring(fights)))
	end)
end
if sfx then
	sfx.OnClientEvent:Connect(function(role, speed)
		say(("SFX %s role=%s speed=%s"):format(T(), tostring(role), tostring(speed)))
	end)
end
if news then
	news.OnClientEvent:Connect(function(line, n)
		say(("NEWS %s %s (%s)"):format(T(), tostring(line), tostring(n)))
	end)
end
if fish then
	fish.OnClientEvent:Connect(function(kind, a, b)
		say(("FISH %s kind=%s a=%s b=%s"):format(T(), tostring(kind), tostring(a), tostring(b)))
	end)
end
if fun then
	fun.OnClientEvent:Connect(function(kind, data)
		local d = type(data) == "table" and data or {}
		local ls = type(d.lines) == "table" and d.lines or {}
		say(("FUN %s kind=%s lines=%d"):format(T(), tostring(kind), #ls))
		for i = 1, math.min(#ls, 2) do say("FUN   | " .. tostring(ls[i])) end
	end)
end
if notify then
	notify.OnClientEvent:Connect(function(d)
		d = type(d) == "table" and d or {}
		say(("NOTIFY %s %s / %s"):format(T(), tostring(d.Title), tostring(d.Text)))
	end)
end

local gui = me:WaitForChild("PlayerGui", 20)
local heartsGui = gui and gui:FindFirstChild("Hearts")
say(("GUI Hearts Enabled=%s before anything happens"):format(tostring(heartsGui and heartsGui.Enabled)))

local ch = me.Character
for _ = 1, 40 do
	if ch then break end
	task.wait(0.5)
	ch = me.Character
end
local hum = ch and ch:FindFirstChildOfClass("Humanoid")
local root = ch and ch:FindFirstChild("HumanoidRootPart")
say(("BODY %s health=%s/%s at y=%s"):format(tostring(ch and ch.Name), tostring(hum and hum.Health),
	tostring(hum and hum.MaxHealth), tostring(root and string.format("%.1f", root.Position.Y))))
if hum then
	hum.HealthChanged:Connect(function(h) say(("HEALTH %s -> %s"):format(T(), tostring(h))) end)
	hum.Died:Connect(function() say("DIED " .. T()) end)
end
me.CharacterAdded:Connect(function(c)
	say(("RESPAWN %s as %s"):format(T(), tostring(c.Name)))
end)

-- ---- 1. the rod, worn ------------------------------------------------------------------
local ROD = "Fishing Rod"
say(("ROD in hand before wearing: %s"):format(tostring(ch and ch:FindFirstChild(ROD) ~= nil)))
if wardrobe then
	wardrobe:FireServer("wear", ROD)
	task.wait(8)
	local worn = ch and ch:FindFirstChild(ROD)
	say(("ROD after wear: %s class=%s"):format(tostring(worn ~= nil),
		tostring(worn and worn.ClassName)))
end

-- The wind-up and the lunge. Nothing is thrown and nothing is signed: "flourish" is the
-- server's own name for letting a wind-up go with nowhere to cast to.
if fish then
	say("FISH charge -> flourish (a lunge, no cast, nothing signed)")
	fish:FireServer("charge")
	task.wait(1.2)
	fish:FireServer("flourish")
	task.wait(4)
	say("FISH cast with no wind-up behind it (expect refused)")
	fish:FireServer("cast", Vector3.new(0, 0, 0))
	task.wait(4)
end

-- ---- 2. the leaderboard door ------------------------------------------------------------
if fun then
	say("FUN asking for the fishing leaderboard")
	fun:FireServer("fishboard")
	task.wait(6)
	local board = gui and gui:FindFirstChild("FishBoard")
	if not board then
		for _, g in ipairs(gui:GetChildren()) do
			if g:IsA("ScreenGui") and g.Enabled and g.Name:lower():find("fish") then board = g end
		end
	end
	say(("FUN fishboard gui=%s"):format(board and (board.Name .. " Enabled=" .. tostring(board.Enabled)) or "nothing opened"))
end

-- ---- 3. over the edge --------------------------------------------------------------------
-- Health.luau: under y=-6 you are told you are going, under y=-25 you are dead. This is the
-- only way one player alone can make the town take hearts off them.
if root then
	say(("VOID %s dropping the body to y=-60"):format(T()))
	for _ = 1, 20 do
		root.CFrame = CFrame.new(0, -60, 0)
		task.wait(0.1)
	end
	task.wait(8)
	local ch2 = me.Character
	local hum2 = ch2 and ch2:FindFirstChildOfClass("Humanoid")
	local root2 = ch2 and ch2:FindFirstChild("HumanoidRootPart")
	say(("VOID after: character=%s health=%s/%s y=%s"):format(tostring(ch2 and ch2.Name),
		tostring(hum2 and hum2.Health), tostring(hum2 and hum2.MaxHealth),
		tostring(root2 and string.format("%.1f", root2.Position.Y))))
	say(("GUI Hearts Enabled=%s after the fall"):format(tostring(heartsGui and heartsGui.Enabled)))
	local rod2 = ch2 and ch2:FindFirstChild(ROD)
	say(("ROD after respawn: %s"):format(tostring(rod2 ~= nil)))
end

task.wait(6)
say("SUMMARY end of second pass")
"""
