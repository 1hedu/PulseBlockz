# A server cannot hand a client code to run.
#   godot --headless --path . -s res://tests/no_code_over_the_wire_test.gd
#
# Over a real socket, the server holds a client script that would announce itself. The client
# gets neither its text nor its effect, only the place URI, and fetches that place from the chain
# checking each file against its hash before it runs. Source must never cross rbx_net.cpp.
extends SceneTree

const PORT := 8823

var server: PulseBlockzWorld
var client: PulseBlockzWorld
var t := 0.0
var phase := 0
var ok := 0
var bad := 0
var lines: Array[String] = []

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("WIRE %-58s %-10s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	server = PulseBlockzWorld.new()
	server.name = "Server"
	server.mode = PulseBlockzWorld.MODE_SERVER
	server.listen_port = PORT
	server.data_store_path = ""
	# The place the server runs, and the URI it hands a client to fetch.
	server.place_uri = "pblockz://" + "ab".repeat(32)
	root.add_child(server)

	# A client script where one belongs; its text must never reach the client.
	server.load_file("StarterPlayer/StarterPlayerScripts/Sneak.client.luau",
		"""
local Players = game:GetService("Players")
Players.LocalPlayer:SetAttribute("RanTheServersCode", true)
print("SNEAK ran")
""")

	client = PulseBlockzWorld.new()
	client.name = "Client"
	client.mode = PulseBlockzWorld.MODE_CLIENT
	client.server_address = "127.0.0.1"
	client.server_port = PORT
	client.player_name = "Ada"
	client.user_id = 7
	client.data_store_path = ""
	root.add_child(client)

func _process(delta: float) -> bool:
	t += delta
	if phase == 0 and t > 6.0:
		phase = 1
		_judge()
	if t > 25.0:
		printerr("no code over the wire: timed out")
		quit(1)
	return false

## The id at a tree path in a world, or 0.
func _at(w: PulseBlockzWorld, path: String) -> int:
	var id := 0
	for want in path.split("/"):
		var found := 0
		for cid in w.get_child_ids(id):
			if String((w.get_instance(cid) as Dictionary).get("name", "")) == want:
				found = cid
				break
		if found == 0:
			return 0
		id = found
	return id

func _judge() -> void:
	check("the client reached the server", client.get_local_player_id() != 0, true)

	# The instance replicates: tree shape is state. Source is not.
	var on_server := _at(server, "StarterPlayer/StarterPlayerScripts/Sneak")
	check("the server has the script", on_server != 0, true)
	var seen: Dictionary = client.get_instance(on_server) if on_server != 0 else {}
	var source := String(seen.get("Source", ""))
	check("the client never receives its text", source, "")

	# Only the script sets this attribute, so its absence means the script never ran.
	var me := client.get_local_player_id()
	var mine: Dictionary = client.get_instance(me) if me != 0 else {}
	check("nor ran it", mine.get("@RanTheServersCode", null), null)

	check("the server names the place it runs", client.server_place(), server.place_uri)

	for l in lines:
		print(l)
	print("WIRE %d passed, %d failed" % [ok, bad])
	print("no code over the wire: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
