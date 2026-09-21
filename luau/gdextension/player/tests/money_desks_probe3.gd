# Does the trader's base token list ever fill? Asks the desk again every 12 s for two
# minutes and prints what it answers with, beside what the host is doing. Reads only.
#
#   godot --headless --path . -s res://tests/money_desks_probe3.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://money-desks-probe3"
	root.add_child(player)
	_run.call_deferred()

func _run() -> void:
	var where := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://"):
			where = a
	var got: Dictionary = await player.play(where)
	print("MONEY3 play ok=", got.get("ok", false), " err=", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 20.0:
		phase = 1
		w.run_client_chunk("money3", """
local rs = game:GetService("ReplicatedStorage")
local function tag(s) print("MONEY3 " .. s) end
local trade = rs:WaitForChild("TradeRemote", 30)
local board = rs:WaitForChild("ScreenerRemote", 30)

trade.OnClientEvent:Connect(function(kind, p)
	p = p or {}
	if kind == "market" then
		tag(("TRADE market at=%.0f loading=%s tokens=%d gas=%s note=%s"):format(
			os.clock() % 100000, tostring(p.loading), #(p.tokens or {}), tostring(p.gas), tostring(p.note)))
	end
end)
board.OnClientEvent:Connect(function(kind, p)
	p = p or {}
	if kind == "board" then
		tag(("SCREEN board at=%.0f loading=%s rows=%d total=%s note=%s"):format(
			os.clock() % 100000, tostring(p.loading), #(p.rows or {}), tostring(p.total), tostring(p.note)))
	end
end)

task.spawn(function()
	for i = 1, 11 do
		tag(("ask #%d at=%.0f"):format(i, os.clock() % 100000))
		trade:FireServer("open")
		board:FireServer("open")
		task.wait(12)
	end
	tag("done asking")
end)
""")
	elif phase == 1 and t > 165.0:
		quit(0)
	return false
