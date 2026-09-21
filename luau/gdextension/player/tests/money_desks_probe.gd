# The three wallet-facing desks, asked from the client the way their panels ask:
#   Penn the teller   (BankRemote)    balance / items / about / ask_send -> send_to token menu
#   Rebh the trader   (TradeRemote)   open -> "market" (base token list + depth), quote, positions
#   Doug the screener (ScreenerRemote) open -> "board" (rows), search -> filtered board,
#                                      and the whole-market search through ReplicatedStorage.Market
#
# Nothing here signs anything: every action used is a read or a menu.
#
#   godot --headless --path . -s res://tests/money_desks_probe.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://money-desks-probe"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s): lines.append("PRINT %s: %s" % [String(n), String(s)]))
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
		print("MONEY play ok=", got.get("ok", false), " err=", got.get("error", ""))
	else:
		var hp: Dictionary = player._split_server(where)
		got = await player.join(String(hp.host), int(hp.port), true)
		print("MONEY join ", got)
	var U = preload("res://host/Uses.gd")
	print("MONEY uses declared=", U.declared, " uses=", U.uses)

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 32.0:
		phase = 1
		var U = preload("res://host/Uses.gd")
		print("MONEY uses(at ask) declared=", U.declared, " uses=", U.uses)
		w.run_client_chunk("money", """
local rs = game:GetService("ReplicatedStorage")
local function tag(s) print("MONEY " .. s) end
local function clock() return string.format("t=%.0f", os.clock()) end

local bank = rs:WaitForChild("BankRemote", 30)
local trade = rs:WaitForChild("TradeRemote", 30)
local board = rs:WaitForChild("ScreenerRemote", 30)
local marketMod = rs:WaitForChild("Market", 10)
tag(("remotes bank=%s trade=%s screener=%s Market=%s"):format(
	tostring(bank ~= nil), tostring(trade ~= nil), tostring(board ~= nil), tostring(marketMod ~= nil)))

local tokens = {}

if bank then
	bank.OnClientEvent:Connect(function(kind, p)
		p = p or {}
		if kind == "open" or kind == "update" then
			tag(("BANK %s %s greeting=%s"):format(kind, clock(), tostring(p.greeting)))
		elseif kind == "say" then
			local ids = {}
			for _, m in ipairs(p.menu or {}) do
				table.insert(ids, tostring(m.id) .. (m.arg and ("[" .. tostring(m.label) .. "]") or ""))
			end
			tag(("BANK say %s menu=%s input=%s"):format(clock(), table.concat(ids, ","), tostring(p.input ~= nil)))
			for _, l in ipairs(p.lines or {}) do tag("BANK   | " .. tostring(l)) end
		else
			tag("BANK " .. tostring(kind))
		end
	end)
end

if trade then
	trade.OnClientEvent:Connect(function(kind, p)
		p = p or {}
		if kind == "market" then
			tokens = p.tokens or {}
			tag(("TRADE market %s loading=%s tokens=%d chain=%s router=%s gas=%s can_sign=%s note=%s"):format(
				clock(), tostring(p.loading), #tokens, tostring(p.chain_id), tostring(p.router),
				tostring(p.gas), tostring(p.can_sign), tostring(p.note)))
			for _, tk in ipairs(tokens) do
				local d = (p.depth or {})[tk.symbol]
				tag(("TRADE  token %s addr=%s bal=%s out/1000PLS=%s liq=%s"):format(
					tostring(tk.symbol), tostring(tk.address), tostring(tk.balance),
					d and tostring(d.out) or "-", d and tostring(d.liquidity) or "-"))
			end
		elseif kind == "quoted" then
			tag(("TRADE quoted %s ok=%s out=%s to=%s min=%s impact=%s route=%s msg=%s"):format(
				clock(), tostring(p.ok), tostring(p.out), tostring(p.to_symbol), tostring(p.min_received),
				tostring(p.impact), tostring(p.route), tostring(p.message)))
		elseif kind == "positions" then
			tag(("TRADE positions %s ok=%s n=%s msg=%s"):format(clock(), tostring(p.ok),
				tostring(p.positions and #p.positions or "nil"), tostring(p.message)))
			for _, pos in ipairs(p.positions or {}) do
				tag(("TRADE  pos %s/%s lp=%s"):format(tostring(pos.symbol), tostring(pos.symbol_b or "PLS"), tostring(pos.balance)))
			end
		else
			tag("TRADE " .. tostring(kind))
		end
	end)
end

if board then
	board.OnClientEvent:Connect(function(kind, p)
		p = p or {}
		if kind == "board" then
			tag(("SCREEN board %s loading=%s rows=%s matched=%s total=%s query='%s' note=%s"):format(
				clock(), tostring(p.loading), tostring(p.rows and #p.rows or "nil"), tostring(p.matched),
				tostring(p.total), tostring(p.query), tostring(p.note)))
			for i, r in ipairs(p.rows or {}) do
				if i <= 4 then
					tag(("SCREEN  row %d %s dex=%s price=%s liq=%s vol=%s h24=%s"):format(
						i, tostring(r.pair), tostring(r.dex), tostring(r.price),
						tostring(r.liquidity), tostring(r.volume), tostring(r.h24)))
				end
			end
		else
			tag("SCREEN " .. tostring(kind))
		end
	end)
end

task.spawn(function()
	if trade then trade:FireServer("open") end
	if board then board:FireServer("open") end
	if bank then bank:FireServer("balance") end
	task.wait(20)

	tag("--- second ask, after the chain read ---")
	if bank then bank:FireServer("items") bank:FireServer("about") end
	if trade then trade:FireServer("open") end
	if board then board:FireServer("open") end
	task.wait(20)

	local from, to
	for _, tk in ipairs(tokens) do
		if tk.symbol == "PLS" then from = tk end
		if tk.symbol == "PLSX" then to = tk end
	end
	if from and to then
		tag("asking a quote: 1 PLS -> PLSX")
		trade:FireServer("quote", from.address, to.address, "1", 50)
	elseif #tokens >= 2 then
		tag(("no PLS/PLSX pair in the list; quoting %s -> %s"):format(tostring(tokens[1].symbol), tostring(tokens[2].symbol)))
		trade:FireServer("quote", tokens[1].address, tokens[2].address, "1", 50)
	else
		tag(("cannot ask a quote: the base token list has %d entr(y/ies)"):format(#tokens))
	end
	if trade then trade:FireServer("positions") end
	task.wait(15)

	tag("--- the teller's send list (menu only, nothing signed) ---")
	if bank then
		bank:FireServer("ask_send")
		task.wait(3)
		bank:FireServer("send_to", "", "0x000000000000000000000000000000000000dEaD")
	end
	task.wait(8)

	tag("--- the screener's search box ---")
	if board then board:FireServer("search", "PLS") end
	task.wait(6)
	if board then board:FireServer("search", "") end
	if marketMod then
		local Market = require(marketMod)
		tag("asking the whole market for 'HEX' through this client's own host")
		Market.search("HEX", function(res)
			tag(("MARKETSEARCH %s ok=%s rows=%s cached=%s site=%s msg=%s"):format(
				clock(), tostring(res and res.ok), tostring(res and res.rows and #res.rows or "nil"),
				tostring(res and res.cached), tostring(res and res.site), tostring(res and res.message)))
			for i, r in ipairs((res and res.rows) or {}) do
				if i <= 3 then
					local b = r.baseToken or {}
					local q = r.quoteToken or {}
					tag(("MARKETSEARCH  row %d %s/%s dex=%s liq=%s price=%s"):format(
						i, tostring(b.symbol), tostring(q.symbol), tostring(r.dexId),
						tostring((r.liquidity or {}).usd), tostring(r.priceUsd)))
				end
			end
		end)
	end
	task.wait(25)

	tag("--- last look ---")
	if trade then trade:FireServer("open") end
	if board then board:FireServer("open") end
	if bank then bank:FireServer("balance") end
	tag("done asking")
end)
""")
	elif phase == 1 and t > 172.0:
		print("MONEY ---- collected ----")
		for l in lines:
			if l.find("MONEY") != -1 or l.find("board:") != -1 or l.find("bank:") != -1 \
				or l.begins_with("ERR") or l.begins_with("WARN"):
				print("   ", l.substr(0, 300))
		quit(0)
	return false
