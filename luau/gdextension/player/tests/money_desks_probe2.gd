# Second pass at the money desks: the teller's other two answers (receive, donate) and the
# trader's Add Liquidity card (pool_quote), plus a reverse quote. All reads; nothing signed.
#
#   godot --headless --path . -s res://tests/money_desks_probe2.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://money-desks-probe"
	root.add_child(player)
	_run.call_deferred()

func _run() -> void:
	var where := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://"):
			where = a
	var got: Dictionary = await player.play(where)
	print("MONEY2 play ok=", got.get("ok", false), " err=", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 32.0:
		phase = 1
		w.run_client_chunk("money2", """
local rs = game:GetService("ReplicatedStorage")
local function tag(s) print("MONEY2 " .. s) end

local bank = rs:WaitForChild("BankRemote", 30)
local trade = rs:WaitForChild("TradeRemote", 30)
local tokens = {}

bank.OnClientEvent:Connect(function(kind, p)
	p = p or {}
	if kind == "say" then
		local ids = {}
		for _, m in ipairs(p.menu or {}) do table.insert(ids, tostring(m.id)) end
		tag(("BANK say menu=%s input=%s placeholder=%s"):format(table.concat(ids, ","),
			tostring(p.input ~= nil), p.input and tostring(p.input.placeholder) or "-"))
		for _, l in ipairs(p.lines or {}) do tag("BANK   | " .. tostring(l)) end
	end
end)

trade.OnClientEvent:Connect(function(kind, p)
	p = p or {}
	if kind == "market" then
		tokens = p.tokens or {}
		tag(("TRADE market loading=%s tokens=%d"):format(tostring(p.loading), #tokens))
	elseif kind == "quoted" then
		tag(("TRADE quoted ok=%s out=%s to=%s min=%s impact=%s route=%s msg=%s"):format(
			tostring(p.ok), tostring(p.out), tostring(p.to_symbol), tostring(p.min_received),
			tostring(p.impact), tostring(p.route), tostring(p.message)))
	elseif kind == "pool_quoted" then
		tag(("POOL quoted ok=%s pool=%s a=%s/%s b=%s/%s price_ab=%s price_ba=%s share=%s a_bal=%s b_bal=%s a_short=%s b_short=%s msg=%s"):format(
			tostring(p.ok), tostring(p.pool), tostring(p.a_symbol), tostring(p.a_amount),
			tostring(p.b_symbol), tostring(p.b_amount), tostring(p.price_ab), tostring(p.price_ba),
			tostring(p.share), tostring(p.a_balance), tostring(p.b_balance),
			tostring(p.a_short), tostring(p.b_short), tostring(p.message)))
	end
end)

task.spawn(function()
	trade:FireServer("open")
	task.wait(22)
	tag("--- teller: receive, then donate ---")
	bank:FireServer("receive")
	task.wait(4)
	bank:FireServer("donate")
	task.wait(6)

	local pls, plsx
	for _, tk in ipairs(tokens) do
		if tk.symbol == "PLS" then pls = tk end
		if tk.symbol == "PLSX" then plsx = tk end
	end
	if pls and plsx then
		tag("--- trader: reverse quote 1000 PLSX -> PLS ---")
		trade:FireServer("quote", plsx.address, pls.address, "1000", 50)
		task.wait(8)
		tag("--- trader: Add Liquidity card, 1 PLS against PLSX ---")
		trade:FireServer("pool_quote", pls.address, plsx.address, "1", "a", 1)
		task.wait(10)
		tag("--- trader: a pair with no pool, USDC against DAI ---")
		local usdc, dai
		for _, tk in ipairs(tokens) do
			if tk.symbol == "USDC" then usdc = tk end
			if tk.symbol == "DAI" then dai = tk end
		end
		if usdc and dai then
			trade:FireServer("pool_quote", usdc.address, dai.address, "1", "a", 2)
		end
		task.wait(10)
	else
		tag("no PLS/PLSX to work with; tokens=" .. tostring(#tokens))
	end
	tag("done asking")
end)
""")
	elif phase == 1 and t > 130.0:
		quit(0)
	return false
