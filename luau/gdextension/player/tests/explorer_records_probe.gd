# Does the block explorer still draw a page, does the archivist still answer, and do
# announcements still arrive? Asked from the client, the way the panels ask.
#
#   godot --headless --path . -s res://tests/explorer_records_probe.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://explorer-records-probe"
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
		print("EXR play ", got.get("ok", false), " ", got.get("error", ""))
	else:
		var hp: Dictionary = player._split_server(where)
		got = await player.join(String(hp.host), int(hp.port), true)
		print("EXR join ", got)

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 32.0:
		phase = 1
		w.run_client_chunk("explorer_records", """
local rs = game:GetService("ReplicatedStorage")
local function T() return string.format("%.0f", os.clock()) end
local function say(s) print("EXR t=" .. T() .. " " .. s) end

-- ---- what is even here -----------------------------------------------------------
for _, name in ipairs({"Chain", "Scan", "ExplorerRemote", "RecordsRemote", "Contracts", "OwnWallet", "TownOwner"}) do
	local found = rs:WaitForChild(name, 25)
	say("present " .. name .. " = " .. tostring(found ~= nil))
end

local chain = rs:FindFirstChild("Chain")
if chain then
	say("Chain.AskScanHandles = " .. tostring(chain:GetAttribute("AskScanHandles")))
end

local Contracts = require(rs:WaitForChild("Contracts"))
say("Contracts.Announcements = " .. tostring(Contracts.Announcements))
say("Contracts.DuelRecords = " .. tostring(Contracts.DuelRecords))

-- ---- the explorer page, fetched by this machine -----------------------------------
local Scan = require(rs:WaitForChild("Scan"))
local function describe(tag, page)
	if not page then say(tag .. " NO ANSWER (60s)") return end
	if page.ok == false then
		say(tag .. " ok=false message=" .. tostring(page.message))
		return
	end
	local stats = page.stats or {}
	local rows = page.rows or {}
	local tables = page.tables or {}
	local cells = 0
	for _, tb in ipairs(tables) do cells = cells + #(tb.rows or {}) end
	say(("%s ok view=%s url=%s%s title=%q stats=%d rows=%d tables=%d tablerows=%d"):format(
		tag, tostring(page.view), tostring(page.host), tostring(page.url),
		tostring(page.title), #stats, #rows, #tables, cells))
	for _, s in ipairs(stats) do
		say(("%s   stat %s = %s"):format(tag, tostring(s.label), tostring(s.value)))
	end
	for i, tb in ipairs(tables) do
		local first = (tb.rows or {})[1]
		say(("%s   table%d %q rows=%d note=%s first=%s"):format(tag, i, tostring(tb.title),
			#(tb.rows or {}), tostring(tb.note),
			first and (table.concat(first.cells or {}, " | ") .. "  -> " .. tostring(first.view) .. "/" .. tostring(first.id)) or "-"))
	end
	return page
end

local homePage
task.spawn(function()
	Scan.page("home", "", function(p) homePage = describe("SCAN-home", p) end)
end)

-- ---- the explorer window the server opens ------------------------------------------
local board = rs:WaitForChild("ExplorerRemote", 20)
if board then
	board.OnClientEvent:Connect(function(kind, payload)
		if kind == "page" then
			say(("EXPLORER page loading=%s host=%s chain_id=%s block=%s base_fee=%s address=%s gas=%s txs_ever=%s items=%s txs=%d")
				:format(tostring(payload and payload.loading), tostring(payload and payload.host),
					tostring(payload and payload.chain_id), tostring(payload and payload.block),
					tostring(payload and payload.base_fee), tostring(payload and payload.address),
					tostring(payload and payload.gas), tostring(payload and payload.txs_ever),
					tostring(payload and payload.items), payload and #(payload.txs or {}) or -1))
			for _, c in ipairs((payload and payload.contracts) or {}) do
				say("EXPLORER   contract " .. tostring(c.name) .. " = " .. tostring(c.address))
			end
		else
			say("EXPLORER kind=" .. tostring(kind) .. " payload=" .. tostring(payload))
		end
	end)
end

-- ---- the archivist ------------------------------------------------------------------
local records = rs:WaitForChild("RecordsRemote", 20)
if records then
	records.OnClientEvent:Connect(function(kind, payload)
		if kind == "say" or kind == "open" or kind == "update" then
			local d = payload or {}
			local menu = {}
			for _, m in ipairs(d.menu or {}) do table.insert(menu, tostring(m.id)) end
			say(("RECORDS %s greeting=%q menu=[%s]"):format(kind, tostring(d.greeting or ""), table.concat(menu, ",")))
			for _, l in ipairs(d.lines or {}) do say("RECORDS   | " .. tostring(l)) end
		else
			say("RECORDS kind=" .. tostring(kind))
		end
	end)
end

-- ---- the duel record desk -------------------------------------------------------------
local fun = rs:WaitForChild("FunRemote", 20)
if fun then
	fun.OnClientEvent:Connect(function(kind, payload)
		if kind == "say" or kind == "open" then
			local d = payload or {}
			say(("FUN %s"):format(kind))
			for _, l in ipairs(d.lines or {}) do say("FUN   | " .. tostring(l)) end
		else
			say("FUN kind=" .. tostring(kind))
		end
	end)
end

-- ---- announcements, read the way the client reads them ----------------------------------
task.spawn(function()
	local OwnWallet = require(rs:WaitForChild("OwnWallet"))
	if not Contracts.Announcements or Contracts.Announcements == "" then
		say("ANN no Announcements contract")
		return
	end
	local answer = OwnWallet.request({ action = "read", calls = {
		{ to = Contracts.Announcements, fn = "count()", args = {}, returns = { "uint256" } },
		{ to = Contracts.Announcements, fn = "pinned()", args = {}, returns = { "bool", "uint256" } },
	} }, 60)
	if not answer then say("ANN count(): no answer in 60s") return end
	if not answer.ok then say("ANN count(): ok=false " .. tostring(answer.message)) return end
	local r = answer.results or {}
	say(("ANN count ok=%s words=%s   pinned ok=%s words=%s,%s"):format(
		tostring(r[1] and r[1].ok), tostring(r[1] and r[1].words and r[1].words[1]),
		tostring(r[2] and r[2].ok), tostring(r[2] and r[2].words and r[2].words[1]),
		tostring(r[2] and r[2].words and r[2].words[2])))
	local n = tonumber(r[1] and r[1].words and r[1].words[1]) or 0
	if n > 0 then
		local a2 = OwnWallet.request({ action = "read", calls = {
			{ to = Contracts.Announcements, fn = "announcement(uint256)", args = { n - 1 },
				returns = { "string", "uint64" } },
		} }, 60)
		local one = a2 and a2.ok and a2.results and a2.results[1]
		say(("ANN newest ok=%s text=%q at=%s"):format(tostring(one and one.ok),
			tostring(one and one.words and one.words[1]), tostring(one and one.words and one.words[2])))
	end
end)

-- ---- asking, waiting, asking again --------------------------------------------------------
local function ask(remote, label, action, arg)
	if not remote then say("no " .. label) return end
	say("ask " .. label .. ":" .. action)
	remote:FireServer(action, arg)
end

task.wait(4)
ask(board, "explorer", "open")
task.wait(6)
ask(records, "records", "contracts")
task.wait(8)
ask(records, "records", "verify")
task.wait(8)
ask(records, "records", "chain")
task.wait(14)
ask(records, "records", "account")
task.wait(10)
ask(records, "records", "history")
task.wait(10)
ask(records, "records", "announcements")
task.wait(6)
ask(fun, "funmaster", "leaderboard")
task.wait(6)
ask(fun, "funmaster", "mine")
task.wait(4)
task.spawn(function()
	Scan.page("blocks", "", function(p) describe("SCAN-blocks", p) end)
end)
task.wait(16)
-- Second round: the chain read behind the archivist has had a minute now.
ask(records, "records", "chain")
task.wait(10)
ask(records, "records", "account")
task.wait(8)
ask(board, "explorer", "open")
task.wait(4)
ask(records, "records", "scan")
task.wait(6)
-- A page an Engram can be cut from, and the button's own refusal path.
ask(board, "explorer", "keep", "home")
task.wait(5)
say("done asking")
""")
	elif phase == 1 and t > 172.0:
		for l in lines:
			if l.find("EXR") != -1 or l.begins_with("ERR") or l.begins_with("WARN") or l.find("announcement") != -1 or l.find("[scan]") != -1:
				print("   ", l.substr(0, 300))
		quit(0)
	return false
