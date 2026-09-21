# The parts of the town with no chain in them, asked from the client: fishing, duels, combat
# and hearts, emotes, the Funmaster, whisper/chat, and the action bar.
#
#   godot --headless --path . -s res://tests/play_probe.gd -- pblockz://...
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
			if line.begins_with("PLAY"):
				print(line))
		w.script_warn.connect(func(n, s): print("PLAYWARN %s: %s" % [String(n), String(s)]))
		w.script_error.connect(func(n, s): print("PLAYERR %s: %s" % [String(n), String(s)])))
	_run.call_deferred()

func _run() -> void:
	var where := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://"):
			where = a
	var got: Dictionary = await player.play(where)
	print("PLAY play ok=", got.get("ok", false), " ", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 32.0:
		phase = 1
		w.run_client_chunk("play", CHUNK)
	elif phase == 1 and t > 185.0:
		print("PLAY probe done")
		quit(0)
	return false

const CHUNK := """
local rs = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")
local TextChatService = game:GetService("TextChatService")
local me = Players.LocalPlayer
local function say(s) print("PLAY " .. s) end
local function T() return string.format("t=%.0f", os.clock()) end

-- ---- 1. is every desk in my area actually there -----------------------------------------
local first = rs:WaitForChild("FunRemote", 25)
say(("FunRemote = %s"):format(first and first.ClassName or "MISSING"))
for _, n in ipairs({ "FishingRemote", "DuelRemote", "HeartsRemote", "CombatSfx", "CombatNews",
		"WhisperChannel", "Notify" }) do
	local o = rs:WaitForChild(n, 5)
	say(("%s = %s"):format(n, o and o.ClassName or "MISSING"))
end
local duelFolder = rs:FindFirstChild("Duels")
say(("ReplicatedStorage.Duels = %s (%d live duel(s))"):format(
	duelFolder and duelFolder.ClassName or "MISSING",
	duelFolder and #duelFolder:GetChildren() or -1))

local fun = rs:FindFirstChild("FunRemote")
local fish = rs:FindFirstChild("FishingRemote")
local duel = rs:FindFirstChild("DuelRemote")
local hearts = rs:FindFirstChild("HeartsRemote")
local sfx = rs:FindFirstChild("CombatSfx")
local news = rs:FindFirstChild("CombatNews")
local whisper = rs:FindFirstChild("WhisperChannel")
local wardrobe = rs:FindFirstChild("WardrobeRemote")

-- ---- 2. listen before asking anything ---------------------------------------------------
local heard = {}
local function mark(k) heard[k] = (heard[k] or 0) + 1 end

if fun then
	fun.OnClientEvent:Connect(function(kind, data)
		mark("fun:" .. tostring(kind))
		if kind == "say" or kind == "open" then
			local d = type(data) == "table" and data or {}
			local ls = type(d.lines) == "table" and d.lines or (d.greeting and { d.greeting } or {})
			local ids = {}
			for _, m in ipairs(type(d.menu) == "table" and d.menu or {}) do
				table.insert(ids, tostring(m.id) .. (m.arg and ("/" .. tostring(m.arg)) or ""))
			end
			say(("FUN %s %s lines=%d menu=[%s]"):format(T(), tostring(kind), #ls, table.concat(ids, " ")))
			for i = 1, math.min(#ls, 3) do say("FUN   | " .. tostring(ls[i])) end
		else
			say(("FUN %s %s"):format(T(), tostring(kind)))
		end
	end)
end
if fish then
	fish.OnClientEvent:Connect(function(kind, a, b)
		mark("fish:" .. tostring(kind))
		say(("FISH %s kind=%s a=%s b=%s"):format(T(), tostring(kind), tostring(a), tostring(b)))
	end)
end
if duel then
	duel.OnClientEvent:Connect(function(kind, text)
		mark("duel:" .. tostring(kind))
		say(("DUEL %s kind=%s text=%s"):format(T(), tostring(kind), tostring(text)))
	end)
end
if hearts then
	hearts.OnClientEvent:Connect(function(half, maxHalf, fire, colours, fights)
		mark("hearts")
		say(("HEARTS %s %s/%s fire=%s colours=%s fights=%s"):format(T(), tostring(half),
			tostring(maxHalf), tostring(fire), tostring(type(colours) == "table" and #colours or colours),
			tostring(fights)))
	end)
end
if sfx then
	sfx.OnClientEvent:Connect(function(role, speed)
		mark("sfx")
		say(("SFX %s role=%s speed=%s"):format(T(), tostring(role), tostring(speed)))
	end)
end
if news then
	news.OnClientEvent:Connect(function(line, n)
		mark("news")
		say(("NEWS %s %s (%s)"):format(T(), tostring(line), tostring(n)))
	end)
end
if wardrobe then
	wardrobe.OnClientEvent:Connect(function(what, items, _standing, slots)
		if what ~= "items" then return end
		mark("bar")
		local filled, shown = 0, {}
		for i = 1, 10 do
			local s = type(slots) == "table" and slots[i] or nil
			if type(s) == "string" and s ~= "" then filled = filled + 1 end
			table.insert(shown, tostring(s == nil and "nil" or (s == "" and "-" or s)))
		end
		say(("BAR %s slots=%d filled=%d items=%d [%s]"):format(T(),
			type(slots) == "table" and #slots or -1, filled,
			type(items) == "table" and #items or -1, table.concat(shown, ",")))
	end)
end
TextChatService.MessageReceived:Connect(function(msg)
	mark("chat")
	local src = msg.TextSource
	say(("CHAT %s from=%s prefix=%q text=%q"):format(T(),
		src and tostring(src.Name) or "system", tostring(msg.PrefixText or ""), tostring(msg.Text or "")))
end)

-- ---- 3. what the client drew for itself -------------------------------------------------
local gui = me:WaitForChild("PlayerGui", 20)
task.wait(3)
for _, n in ipairs({ "TownChat", "DuelScore", "Hearts", "ActionBar", "FishingHud" }) do
	local g = gui and gui:FindFirstChild(n)
	say(("GUI %s = %s"):format(n, g and ("there, Enabled=" .. tostring(g.Enabled)) or "MISSING"))
end
local cmds = TextChatService:FindFirstChild("TextChatCommands")
local aliases = {}
for _, c in ipairs(cmds and cmds:GetChildren() or {}) do
	table.insert(aliases, ("%s=%s"):format(c.Name, tostring(c.PrimaryAlias)))
end
table.sort(aliases)
say(("COMMANDS %d [%s]"):format(#aliases, table.concat(aliases, " ")))

local ch = me.Character
for _ = 1, 40 do
	if ch then break end
	task.wait(0.5)
	ch = me.Character
end
local hum = ch and ch:FindFirstChildOfClass("Humanoid")
say(("BODY character=%s humanoid=%s health=%s/%s"):format(tostring(ch and ch.Name),
	tostring(hum ~= nil), tostring(hum and hum.Health), tostring(hum and hum.MaxHealth)))
say(("ATTRS Fights=%s OnFire=%s Duel=%s wallet=%s"):format(tostring(me:GetAttribute("Fights")),
	tostring(me:GetAttribute("OnFire")), tostring(me:GetAttribute("Duel")),
	tostring(me:GetAttribute("WalletAddress"))))

-- ---- 4. the action bar ------------------------------------------------------------------
if wardrobe then wardrobe:FireServer("list") end
task.wait(6)

-- ---- 5. whisper -------------------------------------------------------------------------
-- A RemoteFunction: the channel to whisper on, or nil and why. Solo, so the only name
-- to try is my own, which Whisper.server.luau answers with a joke rather than an error.
if whisper then
	local ok, a, b = pcall(function() return whisper:InvokeServer(me.Name) end)
	say(("WHISPER self ok=%s -> %s / %s"):format(tostring(ok), tostring(a), tostring(b)))
	local ok2, a2, b2 = pcall(function() return whisper:InvokeServer("nobody_by_that_name") end)
	say(("WHISPER stranger ok=%s -> %s / %s"):format(tostring(ok2), tostring(a2), tostring(b2)))
	local ok3, a3, b3 = pcall(function() return whisper:InvokeServer("") end)
	say(("WHISPER empty ok=%s -> %s / %s"):format(tostring(ok3), tostring(a3), tostring(b3)))
end

-- ---- 6. chat ----------------------------------------------------------------------------
local channels = TextChatService:FindFirstChild("TextChannels")
local general = channels and channels:FindFirstChild("RBXGeneral")
say(("CHANNELS %s general=%s"):format(tostring(channels ~= nil), tostring(general ~= nil)))
if general then
	local ok, why = pcall(function() general:SendAsync("play probe says hello") end)
	say(("CHAT send ok=%s %s"):format(tostring(ok), tostring(why)))
end
task.wait(4)

-- ---- 7. emotes: a looped one leaves an attribute on the body, so it can be read back ----
if general and ch then
	pcall(function() general:SendAsync("/sit") end)
	task.wait(3)
	say(("EMOTE after /sit: Emote=%s hip=%s"):format(tostring(ch:GetAttribute("Emote")),
		tostring(hum and hum.HipHeight)))
	pcall(function() general:SendAsync("/lay") end)
	task.wait(3)
	say(("EMOTE after /lay: Emote=%s"):format(tostring(ch:GetAttribute("Emote"))))
	pcall(function() general:SendAsync("/roll") end)
	task.wait(3)
	say(("EMOTE after /roll: Emote=%s"):format(tostring(ch:GetAttribute("Emote"))))
	pcall(function() general:SendAsync("/wave") end)
	task.wait(2)
	say(("EMOTE after /wave: Emote=%s (wave does not hold)"):format(tostring(ch:GetAttribute("Emote"))))
	pcall(function() general:SendAsync("/lay") end)   -- same one again: stands back up
	task.wait(2)
	say(("EMOTE after second /lay: Emote=%s"):format(tostring(ch:GetAttribute("Emote"))))
end

-- ---- 8. fishing: a cast with no wind-up behind it is refused, and costs nothing ---------
if fish then
	say("FISH firing cast with no wind-up (expect a refusal)")
	fish:FireServer("cast", Vector3.new(0, 0, 0))
	task.wait(4)
	fish:FireServer("charge")
	task.wait(1)
	fish:FireServer("uncharge")
	task.wait(3)
	say("FISH charge/uncharge sent (no rod = nothing should happen)")
end

-- ---- 9. the Funmaster: reading only, nothing that asks the wallet to sign ---------------
if fun then
	fun:FireServer("ranked")
	task.wait(6)
	fun:FireServer("mode", "1v1")
	task.wait(6)
	fun:FireServer("back")
	task.wait(5)
	fun:FireServer("fishing")
	task.wait(10)
	fun:FireServer("not_a_real_action")
	task.wait(4)
	say("FUN sent an unknown action; a silent drop is what the server script does")
end

-- ---- 10. did the rod arrive ------------------------------------------------------------
if wardrobe then wardrobe:FireServer("list") end
task.wait(8)
if fish then
	say("FISH firing cast again, now that a rod may be in hand")
	fish:FireServer("cast", Vector3.new(0, 0, 0))
	task.wait(4)
end

local keys = {}
for k, v in pairs(heard) do table.insert(keys, ("%s x%d"):format(k, v)) end
table.sort(keys)
say(("HEARD %s"):format(table.concat(keys, ", ")))
say("SUMMARY end of asks")
"""
