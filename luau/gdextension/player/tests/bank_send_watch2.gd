# The window between the wallet's first "ok" publish and the pools landing: does the teller
# claim an empty wallet while the same payload already carries the PLS balance? Polls once a
# second from the moment the place is up. Reads only; nothing is signed.
#
#   godot --headless --path . -s res://tests/bank_send_watch2.gd -- pblockz://...
extends SceneTree
var player: Node
var t := 0.0
var phase := 0

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://bank-send-watch"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s):
			if String(s).begins_with("SENDWATCH"):
				print(String(s))))
	_run.call_deferred()

func _run() -> void:
	var where := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://"):
			where = a
	var got: Dictionary = await player.play(where)
	print("SENDWATCH2 play ok=", got.get("ok", false), " err=", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 3.0:
		phase = 1
		w.run_client_chunk("sendwatch2", """
local rs = game:GetService("ReplicatedStorage")
local http = game:GetService("HttpService")
local function tag(s) print("SENDWATCH " .. s) end
local bank = rs:WaitForChild("BankRemote", 40)
local chain = rs:WaitForChild("Chain", 40)
if not bank then tag("no BankRemote") return end

local step = nil
bank.OnClientEvent:Connect(function(kind, payload)
	if kind == "say" or kind == "open" then step = payload end
end)
local function ask(id, arg, typed)
	step = nil
	bank:FireServer(id, arg, typed)
	local waited = 0
	while step == nil and waited < 8 do task.wait(0.1) waited += 0.1 end
	return step
end

task.spawn(function()
	for i = 1, 90 do
		local raw = chain and chain:GetAttribute("Wallet")
		local d = nil
		if type(raw) == "string" and raw ~= "" then
			local ok, doc = pcall(function() return http:JSONDecode(raw) end)
			if ok and type(doc) == "table" then d = doc end
		end
		local px = d and d.pulsex
		local bal = ask("balance", nil, "")
		local line = bal and bal.lines and bal.lines[1] or "(none)"
		local got = ask("send_to", nil, "0x000000000000000000000000000000000000dEaD")
		local offered = 0
		for _, item in ipairs(got and got.menu or {}) do
			if item.id == "send_token" then offered += 1 end
		end
		local said = got and got.lines and got.lines[#got.lines] or "(none)"
		tag(("#%02d at=%.1f wallet=%s status=%s gas=%s pulsex=%s | balance=%s | offered=%d last=%s"):format(
			i, os.clock() % 10000, tostring(d ~= nil), tostring(d and d.status), tostring(d and d.gas),
			tostring(type(px)), tostring(line), offered, tostring(said)))
		task.wait(1)
	end
	tag("done")
end)
""")
	elif phase == 1 and t > 130.0:
		quit(0)
	return false
