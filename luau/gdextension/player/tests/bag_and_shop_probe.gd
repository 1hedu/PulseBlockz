# The item economy, asked from the client of a live solo session: the bag (WardrobeRemote),
# Finch's shelf (ShopRemote), Kara (TailorRemote) and the showcase remote's presence.
#
#   godot --headless --path . -s res://tests/bag_and_shop_probe.gd -- pblockz://...
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
		print("BAGSHOP play ", got.get("ok", false), " ", got.get("error", ""))
	else:
		var hp: Dictionary = player._split_server(where)
		got = await player.join(String(hp.host), int(hp.port), true)
		print("BAGSHOP join ", got)

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 30.0:
		phase = 1
		w.run_client_chunk("bagshop", """
local rs = game:GetService("ReplicatedStorage")
local function tag(s) print("BAGSHOP " .. s) end
local function at() return string.format("t=%.0f", os.clock()) end

local ward = rs:WaitForChild("WardrobeRemote", 30)
local shop = rs:WaitForChild("ShopRemote", 30)
local tailor = rs:WaitForChild("TailorRemote", 30)
local showcase = rs:WaitForChild("ShowcaseRemote", 5)
tag(("remotes wardrobe=%s shop=%s tailor=%s showcase=%s"):format(
	tostring(ward ~= nil), tostring(shop ~= nil), tostring(tailor ~= nil), tostring(showcase ~= nil)))
if not ward or not shop or not tailor then return end

local function mounted(name)
	local onchain = rs:FindFirstChild("OnChain")
	local made = rs:FindFirstChild("Made")
	local f = (onchain and onchain:FindFirstChild(tostring(name)))
		or (made and made:FindFirstChild(tostring(name)))
	if not f then
		local hash = tostring(name):find("#", 1, true)
		if hash and made then f = made:FindFirstChild(tostring(name):sub(1, hash - 1)) end
	end
	return f ~= nil
end

-- ---- the bag ----------------------------------------------------------------
local last = nil
local shown = 0
ward.OnClientEvent:Connect(function(kind, a, b, c, d)
	if kind == "items" then
		last = a
		local bar = 0
		for _, n in ipairs(c or {}) do if n ~= "" then bar = bar + 1 end end
		tag(("bag %s n=%d waiting=%s standing=%s/tier=%s barfilled=%d"):format(at(), #a,
			tostring(d), tostring(b and b.name), tostring(b and b.tier), bar))
		shown = shown + 1
		if shown <= 3 then
			for i, it in ipairs(a) do
				if i > 25 then tag("bag  ...more") break end
				tag(("bag  %s | model=%s mounted=%s qty=%s slot=%s tier=%s/%s thumb=%s worn=%s acts=%s shows=%s"):format(
					tostring(it.name), tostring(it.model), tostring(mounted(it.model)), tostring(it.qty),
					tostring(it.slot == "" and "-" or it.slot), tostring(it.tier), tostring(it.tier_name),
					tostring(it.thumb ~= nil and it.thumb ~= "" and "yes" or "NO"),
					tostring(it.worn), tostring(it.acts), tostring(it.shows)))
			end
		end
	elseif kind == "note" then
		tag("bag note: " .. tostring(a))
	else
		tag("bag other kind=" .. tostring(kind))
	end
end)

-- ---- the shelf --------------------------------------------------------------
local shelfSeen = false
shop.OnClientEvent:Connect(function(kind, payload)
	if kind == "shelf" then
		shelfSeen = true
		local items = (payload or {}).items or {}
		local priced, thumbed, locked, ownedn = 0, 0, 0, 0
		for _, it in ipairs(items) do
			if it.tier_pls ~= nil then priced = priced + 1 end
			if it.thumb ~= nil and it.thumb ~= "" then thumbed = thumbed + 1 end
			if it.locked then locked = locked + 1 end
			if it.owned then ownedn = ownedn + 1 end
		end
		local h = (payload or {}).holder or {}
		tag(("shelf %s n=%d with_tier_pls=%d with_thumb=%d locked=%d owned=%d can_buy=%s holder=%s/next=%s/needed=%s gas=%s"):format(
			at(), #items, priced, thumbed, locked, ownedn,
			tostring(items[1] and items[1].can_buy), tostring(h.name), tostring(h.next),
			tostring(h.needed), tostring((payload or {}).gas)))
		for i, it in ipairs(items) do
			if i > 12 then tag("shelf  ...more") break end
			tag(("shelf  %s | id=%s tier=%s/%s pls=%s slot=%s thumb=%s locked=%s owned=%s"):format(
				tostring(it.name), tostring(it.id), tostring(it.tier), tostring(it.tier_name),
				tostring(it.tier_pls), tostring(it.slot == "" and "-" or it.slot),
				tostring(it.thumb ~= nil and it.thumb ~= "" and "yes" or "NO"),
				tostring(it.locked), tostring(it.owned)))
		end
	elseif kind == "say" then
		for _, l in ipairs((payload or {}).lines or {}) do
			if tostring(l) ~= "" then tag("shop say: " .. tostring(l)) end
		end
		tag(("shop say menu=%d thumb=%s"):format(#(((payload or {}).menu) or {}),
			tostring((payload or {}).thumb ~= "" and "yes" or "no")))
	elseif kind == "open" or kind == "update" then
		tag(("shop %s greeting: %s"):format(tostring(kind), tostring((payload or {}).greeting)))
	else
		tag("shop other kind=" .. tostring(kind))
	end
end)

-- ---- the tailor -------------------------------------------------------------
tailor.OnClientEvent:Connect(function(kind, payload)
	if kind == "say" then
		for _, l in ipairs((payload or {}).lines or {}) do
			if tostring(l) ~= "" then tag("tailor say: " .. tostring(l)) end
		end
		local m = {}
		for _, e in ipairs(((payload or {}).menu) or {}) do table.insert(m, tostring(e.id)) end
		tag("tailor say menu=[" .. table.concat(m, ",") .. "]")
	elseif kind == "open" or kind == "update" then
		tag(("tailor %s greeting: %s"):format(tostring(kind), tostring((payload or {}).greeting)))
	elseif kind == "paint" then
		tag(("tailor paint w=%s h=%s palette=%d"):format(tostring((payload or {}).w),
			tostring((payload or {}).h), #(((payload or {}).palette) or {})))
	else
		tag("tailor other kind=" .. tostring(kind))
	end
end)

-- ---- ask, wait, ask again ---------------------------------------------------
ward:FireServer("list")
task.wait(20)
ward:FireServer("list")
shop:FireServer("shop")
task.wait(10)
shop:FireServer("how")
tailor:FireServer("how")
task.wait(10)
tailor:FireServer("mine")
task.wait(5)
ward:FireServer("list")
task.wait(20)

-- Wear and remove: the first thing in the bag that is not on, then put it back as it was.
local pick = nil
for _, it in ipairs(last or {}) do
	if not it.worn and tostring(it.model) ~= "" then pick = it break end
end
if not pick then
	for _, it in ipairs(last or {}) do if tostring(it.model) ~= "" then pick = it break end end
end
if pick then
	tag(("wear asking for %s (model=%s, currently worn=%s)"):format(
		tostring(pick.name), tostring(pick.model), tostring(pick.worn)))
	ward:FireServer(pick.worn and "remove" or "wear", pick.model)
	task.wait(8)
	local state = "gone from bag"
	for _, it in ipairs(last or {}) do
		if it.model == pick.model then state = "worn=" .. tostring(it.worn) end
	end
	tag(("wear after %s: %s -> %s"):format(pick.worn and "remove" or "wear", tostring(pick.model), state))
	ward:FireServer(pick.worn and "wear" or "remove", pick.model)
	task.wait(8)
	local back = "gone from bag"
	for _, it in ipairs(last or {}) do
		if it.model == pick.model then back = "worn=" .. tostring(it.worn) end
	end
	tag(("wear restored %s: %s"):format(tostring(pick.model), back))
	-- A model the chain never gave this player: the server must not honour it.
	ward:FireServer("wear", "NotAThingAnybodyOwns")
	task.wait(6)
	local bogus = false
	for _, it in ipairs(last or {}) do if it.model == "NotAThingAnybodyOwns" then bogus = true end end
	tag("wear bogus honoured=" .. tostring(bogus))
else
	tag("wear no item in the bag to try")
end

shop:FireServer("shop")
task.wait(10)
tag("shelf ever seen=" .. tostring(shelfSeen))
tag("done")
""")
	elif phase == 1 and t > 195.0:
		for l in lines:
			if l.find("BAGSHOP") != -1 or l.begins_with("ERR") or l.begins_with("WARN"):
				print("   ", l.substr(0, 260))
		quit(0)
	return false
