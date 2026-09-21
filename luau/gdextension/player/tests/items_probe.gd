# What a joined player is told they own: the wardrobe's own "items" message, listened for from
# the client, beside what this wallet holds on chain.
#
#   godot --headless --path . -s res://tests/items_probe.gd -- host:port
extends SceneTree
var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://items-probe"
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
	# No title screen here, so nothing has unlocked the wallet: do what the door does, on this
	# machine, in memory. A keystore with a passphrase stays shut and the probe plays as a guest.
	var w = preload("res://host/Wallet.gd")
	if w.unlocked == "":
		w.unlocked = preload("res://host/Keyfile.gd").unlock("user://player.key", "")
	print("ITEMS wallet: %s" % ("unlocked" if w.unlocked != "" else "guest (locked or none)"))
	var got: Dictionary
	if where.begins_with("pblockz://"):
		got = await player.play(where)
		print("ITEMS play ", got.get("ok", false), " ", got.get("error", ""))
	else:
		var hp: Dictionary = player._split_server(where)
		# allow_wallet: the desks may ask this wallet, as they would for a signed-in player.
		got = await player.join(String(hp.host), int(hp.port), true)
		print("ITEMS join ", got)
	var sw = player.session.get_node_or_null("Wallet")
	var addr := "-"
	if sw != null:
		addr = String(sw.wallet_address)
	print("ITEMS after play: unlocked=%d chars, session wallet=%s, address=%s" % [w.unlocked.length(), sw != null, addr])

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 30.0:
		phase = 1
		w.run_client_chunk("items", """
local rs = game:GetService("ReplicatedStorage")
local remote = rs:WaitForChild("WardrobeRemote", 20)
if not remote then print("ITEMS no WardrobeRemote") return end
remote.OnClientEvent:Connect(function(kind, items, standing, bar, waiting)
	if kind ~= "items" then return end
	print("ITEMS t=" .. string.format("%.0f", os.clock()) .. " " .. tostring(#items) .. " item(s), chain still reading: " .. tostring(waiting))
	for _, it in ipairs(items) do
		print(("ITEMS  id=%s qty=%s model=%s kind=%s name=%s thumb=%s"):format(
			tostring(it.id), tostring(it.qty), tostring(it.model), tostring(it.kind),
			tostring(it.name), tostring(it.thumb ~= "" and "yes" or "no")))
	end
end)
-- Ask for a fresh list the way the panel does when it opens.
for i = 1, 12 do
	remote:FireServer("list")
	task.wait(20)
end
""")
	elif phase == 1 and t > 280.0:
		for l in lines:
			if l.find("ITEMS") != -1 or l.begins_with("ERR") or l.begins_with("WARN"):
				print("   ", l.substr(0, 220))
		quit(0)
	return false
