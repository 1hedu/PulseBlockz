# Second pass over the item economy: wearing and taking off, whether the shelf knows what
# you already hold, and what the tailor counts as "drawn at my table".
#
#   godot --headless --path . -s res://tests/bag_and_shop_probe2.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://bag-shop-probe"
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
	var got: Dictionary
	if where.begins_with("pblockz://"):
		got = await player.play(where)
		print("BAG2 play ", got.get("ok", false), " ", got.get("error", ""))
	else:
		var hp: Dictionary = player._split_server(where)
		got = await player.join(String(hp.host), int(hp.port), true)
		print("BAG2 join ", got)

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 30.0:
		phase = 1
		w.run_client_chunk("bag2", """
local rs = game:GetService("ReplicatedStorage")
local function tag(s) print("BAG2 " .. s) end

local ward = rs:WaitForChild("WardrobeRemote", 30)
local shop = rs:WaitForChild("ShopRemote", 30)
local tailor = rs:WaitForChild("TailorRemote", 30)
if not ward or not shop or not tailor then tag("a remote is missing") return end

local bag = {}
local notes = 0
ward.OnClientEvent:Connect(function(kind, a, b, c, d)
	if kind == "items" then
		bag = a
		local wornN = 0
		for _, it in ipairs(a) do if it.worn then wornN = wornN + 1 end end
		tag(("bag n=%d worn=%d waiting=%s"):format(#a, wornN, tostring(d)))
	elseif kind == "note" then
		notes = notes + 1
		tag("bag note: " .. tostring(a))
	end
end)

local shelf = {}
shop.OnClientEvent:Connect(function(kind, payload)
	if kind == "shelf" then shelf = (payload or {}).items or {} end
end)

local giveMenu = nil
tailor.OnClientEvent:Connect(function(kind, payload)
	if kind == "say" and giveMenu == nil then
		local n, first = 0, {}
		for _, e in ipairs(((payload or {}).menu) or {}) do
			if tostring(e.id) == "pick_give" then
				n = n + 1
				if #first < 4 then table.insert(first, tostring(e.label)) end
			end
		end
		if n > 0 then
			giveMenu = n
			tag(("tailor give-list n=%d first=[%s]"):format(n, table.concat(first, " | ")))
		end
	end
end)

ward:FireServer("list")
task.wait(25)
ward:FireServer("list")
shop:FireServer("shop")
task.wait(12)

-- Does the shelf know what is already in the bag? Matched by the name the chain gave both.
local mine = {}
for _, it in ipairs(bag) do mine[tostring(it.name)] = true end
local same, flagged, examples = 0, 0, {}
for _, it in ipairs(shelf) do
	if mine[tostring(it.name)] then
		same = same + 1
		if it.owned then flagged = flagged + 1 end
		if #examples < 6 then
			table.insert(examples, ("%s(id=%s owned=%s locked=%s)"):format(
				tostring(it.name), tostring(it.id), tostring(it.owned), tostring(it.locked)))
		end
	end
end
tag(("shelf n=%d bag n=%d same-name=%d of-those-marked-owned=%d"):format(#shelf, #bag, same, flagged))
tag("shelf overlap: " .. table.concat(examples, ", "))

tailor:FireServer("ask_give")
task.wait(8)

-- Wear, check, take off, check. The first thing in the bag that is not already on.
local pick = nil
for _, it in ipairs(bag) do
	if not it.worn and tostring(it.model) ~= "" then pick = it break end
end
if not pick then
	tag("wear nothing to try")
else
	local function stateOf(model)
		for _, it in ipairs(bag) do if it.model == model then return tostring(it.worn) end end
		return "gone"
	end
	tag(("wear asking wear %s (model=%s slot=%s)"):format(tostring(pick.name), tostring(pick.model), tostring(pick.slot)))
	ward:FireServer("wear", pick.model)
	task.wait(8)
	tag(("wear after wear: %s worn=%s"):format(tostring(pick.model), stateOf(pick.model)))
	ward:FireServer("remove", pick.model)
	task.wait(8)
	tag(("wear after remove: %s worn=%s"):format(tostring(pick.model), stateOf(pick.model)))
	ward:FireServer("wear", "NotAThingAnybodyOwns")
	task.wait(6)
	tag("wear bogus honoured=" .. tostring(stateOf("NotAThingAnybodyOwns") ~= "gone"))
end
tag("done notes=" .. tostring(notes))
""")
	elif phase == 1 and t > 140.0:
		for l in lines:
			if l.find("BAG2") != -1 or l.begins_with("ERR") or l.begins_with("WARN"):
				print("   ", l.substr(0, 260))
		quit(0)
	return false
