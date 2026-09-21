# Does Penn's "Send someone money" ever say the wallet is empty while the same payload
# still shows PLS? Drives the teller's balance answer and the send menu every 10 s, and
# prints, beside each, whether the published Wallet payload still carries its "pulsex" key.
# Reads only: it stops at the token menu and signs nothing.
#
#   godot --headless --path . -s res://tests/bank_send_watch.gd -- pblockz://...
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
				print(String(s)))
		w.script_error.connect(func(n, s): print("SENDWATCH ERR ", String(n), ": ", String(s))))
	_run.call_deferred()

func _run() -> void:
	var where := ""
	for a in OS.get_cmdline_user_args():
		if a.begins_with("pblockz://"):
			where = a
	var got: Dictionary = await player.play(where)
	print("SENDWATCH play ok=", got.get("ok", false), " err=", got.get("error", ""))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 25.0:
		phase = 1
		w.run_client_chunk("sendwatch", """
local rs = game:GetService("ReplicatedStorage")
local http = game:GetService("HttpService")
local function tag(s) print("SENDWATCH " .. s) end
local bank = rs:WaitForChild("BankRemote", 30)
local chain = rs:WaitForChild("Chain", 30)
if not bank then tag("no BankRemote") return end

local step = nil
bank.OnClientEvent:Connect(function(kind, payload)
	if kind == "say" or kind == "open" then step = payload end
end)
local function ask(id, arg, typed)
	step = nil
	bank:FireServer(id, arg, typed)
	local waited = 0
	while step == nil and waited < 12 do task.wait(0.25) waited += 0.25 end
	return step
end

-- What this machine last published for itself, as the desks see it.
local function payload()
	local raw = chain and chain:GetAttribute("Wallet")
	if type(raw) ~= "string" or raw == "" then return nil end
	local ok, doc = pcall(function() return http:JSONDecode(raw) end)
	if not ok or type(doc) ~= "table" then return nil end
	return doc
end

task.spawn(function()
	for i = 1, 18 do
		local at = ("%.0f"):format(os.clock() % 100000)
		local d = payload()
		local px = d and d.pulsex
		tag(("payload #%d at=%s status=%s gas=%s tokens=%d pulsex=%s pulsex_tokens=%d"):format(
			i, at, tostring(d and d.status), tostring(d and d.gas),
			#((d or {}).tokens or {}), tostring(type(px)), #((type(px) == "table" and px.tokens) or {})))

		local bal = ask("balance", nil, "")
		local line = bal and bal.lines and bal.lines[1] or "(no answer)"
		ask("ask_send", nil, "")
		local got = ask("send_to", nil, "0x000000000000000000000000000000000000dEaD")
		local offered = 0
		for _, item in ipairs(got and got.menu or {}) do
			if item.id == "send_token" then offered += 1 end
		end
		local said = got and got.lines and table.concat(got.lines, " | ") or "(no answer)"
		tag(("teller  #%d balance=%s send_offered=%d said=%s"):format(i, tostring(line), offered, tostring(said)))
		task.wait(10)
	end
	tag("done")
end)
""")
	elif phase == 1 and t > 225.0:
		quit(0)
	return false
