# The shelf a live server offers a wallet below the bottom tier, over the wire.
#   godot --headless --path . -s res://tests/shelf_live_check.gd -- [host:port]
extends SceneTree

var player: Node
var t := 0.0
var phase := 0
var lines: Array[String] = []

func _initialize() -> void:
	player = load("res://Player.tscn").instantiate()
	player.show_title = false
	player.profile_dir = "user://shelf-check"
	root.add_child(player)
	player.session_started.connect(func(_m):
		var w: PulseBlockzWorld = player.session.get_node("World")
		w.script_print.connect(func(n, s): lines.append(String(s)))
		w.script_error.connect(func(n, s): lines.append("ERR " + String(s))))
	_run.call_deferred()

func _run() -> void:
	var where := "play.safewrap.xyz:8800"
	for a in OS.get_cmdline_user_args():
		if a.contains(":") and not a.begins_with("--"):
			where = a
	var hp: Dictionary = player._split_server(where)
	print("SHELF join ", await player.join(String(hp.host), int(hp.port), true))

func _process(delta: float) -> bool:
	t += delta
	if player.session == null:
		return false
	var w: PulseBlockzWorld = player.session.get_node("World")
	if phase == 0 and t > 55.0:
		phase = 1
		w.run_client_chunk("shelf", """
local rs = game:GetService("ReplicatedStorage")
local remote = rs:WaitForChild("ShopRemote", 20)
if not remote then print("SHELF no ShopRemote") return end
local heard = false
remote.OnClientEvent:Connect(function(kind, payload)
	if kind == "shelf" and not heard then
		heard = true
		local holder = payload.holder or {}
		local offered, locked = 0, 0
		for _, item in ipairs(payload.items or {}) do
			if item.locked then locked += 1 else offered += 1 end
		end
		print(("SHELF you are '%s' (tier %s), %s PLS; %d offered, %d locked; next %s at %s")
			:format(tostring(holder.name), tostring(holder.tier), tostring(payload.gas),
				offered, locked, tostring(holder.next), tostring(holder.needed)))
	elseif kind == "say" or kind == "greeting" then
		local text = type(payload) == "table" and (payload.text or payload.greeting) or payload
		if type(text) == "string" then print("SHELF says: " .. text:sub(1, 150):gsub("\\n", " / ")) end
	end
end)
task.wait(2)
remote:FireServer("shop")
task.wait(20)
if not heard then print("SHELF the shelf never came back") end
""")
	elif phase == 1 and t > 85.0:
		for l in lines:
			if l.begins_with("SHELF") or l.begins_with("ERR"):
				print("   ", l.substr(0, 240))
		quit(0)
	return false
