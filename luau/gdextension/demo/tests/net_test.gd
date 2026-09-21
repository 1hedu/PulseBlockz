# A Server and two Clients over ENet in one process: join, map replication, movement seen by
# everyone, chat, a client-reported lava touch and respawn, RemoteEvents, terrain, and leaving.
#   godot --headless --path . -s res://tests/net_test.gd
extends SceneTree

const PORT := 8801
var server: PulseBlockzWorld
var client: PulseBlockzWorld
var bob: PulseBlockzWorld
var bob_root := 0
var left_name := ""
var t := 0.0
var phase := 0
var start_pos: Vector3
var failed := 0
var logs: Array[String] = []
var joined_peer := 0
var _handshook := false
var client_sync: Node
var connected_as := 0
var old_body: Node3D
var died_at := 0.0
var left_at := 0.0
var terrain_at := 0.0
var chat_at := 0.0
var chat_step := 0
var chat_pos: Vector3

func _initialize() -> void:
	var main := Node3D.new()
	root.add_child(main)
	server = PulseBlockzWorld.new()
	server.max_millis_per_call = 4.0      # Main.gd's budgets, so the demo's Evil script is caught per frame here too
	server.max_frame_millis = 8.0
	server.name = "Server"
	server.mode = PulseBlockzWorld.MODE_SERVER
	server.listen_port = PORT
	# What a joiner is told to fetch. Nothing is published here, so the client builds the same
	# files off disk instead; either way it runs the place it has, not one the server hands it.
	server.place_uri = "pblockz://" + "cd".repeat(32)
	server.bind_address = "127.0.0.1"   # loopback only: no firewall prompt
	server.physics_layer = 2
	main.add_child(server)
	var sync: Node = load("res://ScriptSync.gd").new()
	sync.world_path = ^"../Server"
	main.add_child(sync)
	# The client loads the demo's scripts itself; the server sends no code (rbx_net.cpp), so a
	# client with none of its own runs none. Disk stands in for the chain here: both halves read
	# the same directory in the same order, so they build the same instances at the same ids
	# (demo2/tests/same_ids_test.gd) and the server's state lands on the client's instances.
	client_sync = load("res://ScriptSync.gd").new()
	client_sync.world_path = ^"../Client"
	main.add_child(client_sync)
	client = PulseBlockzWorld.new()
	client.max_millis_per_call = 4.0
	client.max_frame_millis = 8.0
	client.name = "Client"
	client.mode = PulseBlockzWorld.MODE_CLIENT
	client.server_address = "127.0.0.1"
	client.server_port = PORT
	client.player_name = "Ada"
	client.user_id = 7
	client.physics_layer = 1
	# The tree is sent only once the client says Ready, which it does after building its own copy
	# of the place: a snapshot arriving earlier has no instances to land on.
	client.hold_for_place = true
	main.add_child(client)
	bob = PulseBlockzWorld.new()
	bob.max_millis_per_call = 4.0
	bob.max_frame_millis = 8.0
	bob.name = "Bob"
	bob.mode = PulseBlockzWorld.MODE_CLIENT
	bob.server_port = PORT
	bob.player_name = "Bob"
	bob.user_id = 8
	bob.default_controls = false   # stands still; the keys are Ada's
	bob.physics_layer = 3
	main.add_child(bob)
	for w in [server, client, bob]:
		var tag: String = w.name
		w.script_print.connect(func(n, s): logs.append(tag + " " + n + ": " + s))
		w.script_error.connect(func(n, e): logs.append("ERROR " + tag + " " + n + ": " + e))
	server.client_joined.connect(func(n, pid, peer): if n == "Ada": joined_peer = peer; logs.append("Server joined %s %d peer %d" % [n, pid, peer]))
	server.client_left.connect(func(n, pid, peer): left_name = n)
	client.server_connected.connect(func(pid): connected_as = pid)

func _key(code: Key, pressed: bool) -> void:
	var ev := InputEventKey.new()
	ev.physical_keycode = code
	ev.keycode = code
	ev.pressed = pressed
	Input.parse_input_event(ev)

func check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok: failed += 1

func _has(s: String) -> bool:
	for l in logs: if l.find(s) != -1: return true
	return false

func _static_parts(w: Node) -> int:
	var n := 0
	for c in w.get_children(): if c is StaticBody3D: n += 1
	return n

func _process(delta: float) -> bool:
	t += delta
	# The server's tree is already here, carrying no script Source: load_place() loads the checked
	# place over those instances to fill it in, and only then does ready_for_place() add the local
	# player, which copies StarterPlayerScripts into PlayerScripts.
	if not _handshook and client.is_server_connected():
		_handshook = true
		client_sync.load_place()
		client.ready_for_place()
		if OS.get_environment("NET_TEST_LOGS") != "":
			print("SEQ loaded %d file(s), connected=%s" % [client_sync.names().size(), client.is_server_connected()])
	var body: Node3D = client.get_local_character_node()
	match phase:
		0:
			if t > 2.5:
				check(connected_as > 0 and client.is_server_connected(), "the client connected and was welcomed as player %d" % connected_as)
				check(joined_peer > 0 and server.get_client_count() == 2, "the server saw both join (Ada is peer %d, %d clients)" % [joined_peer, server.get_client_count()])
				check(bob.is_server_connected() and bob.get_local_character_node() != null, "Bob is in too, with a character (player %d)" % bob.get_local_player_id())
				bob_root = bob.get_part_id(bob.get_local_character_node()) if bob.get_local_character_node() else 0
				check(bob_root > 0 and client.get_part_node(bob_root) != null and server.get_part_node(bob_root) != null, "Bob's character stands in Ada's scene and the server's (root %d)" % bob_root)
				check(client.get_local_player_id() == connected_as, "LocalPlayer id matches: %d" % client.get_local_player_id())
				check(_static_parts(client) >= 10, "the map replicated to the client: %d static parts (server has %d)" % [_static_parts(client), _static_parts(server)])
				check(body != null, "the local character has a CharacterBody3D on the client (character %d)" % client.get_local_character_id())
				if body == null: return _finish()
				var root_id: int = client.get_part_id(body)
				var srv_root: Node3D = server.get_part_node(root_id)
				check(srv_root != null and srv_root is CharacterBody3D, "the server mirrors that character too (root %d)" % root_id)
				start_pos = body.global_position
				var bob_pos: Vector3 = bob.get_local_character_node().global_position
				var apart := Vector2(start_pos.x - bob_pos.x, start_pos.z - bob_pos.z).length()
				check(apart > 2.5 and apart < 3.5 and absf(start_pos.y - bob_pos.y) < 0.5, "the two characters spawned beside each other, not inside: %.2f studs apart" % apart)
				_key(KEY_W, true)
				phase = 1
		1:
			if t > 3.1:
				var moved: Vector3 = body.global_position - start_pos
				check(moved.length() > 5.0 and moved.z < -5.0, "W walks the client's character: %s" % moved)
				var srv_root: Node3D = server.get_part_node(client.get_part_id(body))
				var gap := (srv_root.global_position - body.global_position).length() if srv_root else 1e9
				check(gap < 1.5, "the server follows the reported pose: %.2f studs behind" % gap)
				var n := 0
				if srv_root: for c in srv_root.get_children(): if c is MeshInstance3D and c.name != "ForceField": n += 1   # not the spawn bubble
				check(n == 8, "the server's copy wears the R6 limbs and the Cap: %d meshes" % n)
				var bobs_view: Node3D = bob.get_part_node(client.get_part_id(body))
				var gap2 := (bobs_view.global_position - body.global_position).length() if bobs_view else 1e9
				check(bobs_view is CharacterBody3D and gap2 < 2.0, "Bob sees Ada walk: %.2f studs behind" % gap2)
				# Roblox name tags: a client draws the other players' names over their heads, never its own
				var tag: Control = bob.get_node_or_null("CoreGui/PlayerNames/Ada")
				var tag_name: Label = tag.get_node_or_null("Name") if tag else null
				var bob_head: Node3D = null
				if bobs_view: for c in bobs_view.get_children(): if c is MeshInstance3D and c.name == "Head": bob_head = c
				var expect: Vector2 = bob.get_camera().unproject_position(bob_head.global_position + Vector3(0, 1.4, 0)) if bob_head else Vector2()
				check(tag != null and tag.visible and tag_name != null and tag_name.visible and tag_name.text == "Ada" and (tag.global_position + Vector2(120, 26)).distance_to(expect) < 1.5,
					"Bob's engine draws Ada's name over his head: %s near %s" % [tag.global_position if tag else Vector2(), expect])
				check(tag != null and not tag.get_node("Health").visible, "no health bar while he is unhurt")
				check(bob.get_node_or_null("CoreGui/PlayerNames/Bob") == null and client.get_node_or_null("CoreGui/PlayerNames/Bob") != null, "nobody sees their own name; Ada sees Bob's")
				# Roblox player list: grouped under the Team (src/teams/Explorers.model.json), drawn on clients only
				var list: Node = client.get_node_or_null("CoreGui/PlayerList/Panel/List")
				var names: Array[String] = []
				if list: for c in list.get_children(): names.append(c.name)
				check(names == ["Stats", "Explorers", "Ada", "Bob"], "the player list groups the players under their Team: %s" % [names])
				var row: Node = list.get_node_or_null("Ada") if list else null
				check(row != null and row.get_node("Name").text == "Ada" and row.get_node("TeamColor").color.is_equal_approx(Color(13 / 255.0, 105 / 255.0, 172 / 255.0)), "a row is the DisplayName with the TeamColor")
				# the leaderstats folder the demo's Main.server.luau builds
				var stats: Node = list.get_node_or_null("Stats") if list else null
				var col_bricks: Label = stats.get_node_or_null("Bricks") if stats else null
				var col_deaths: Label = stats.get_node_or_null("Deaths") if stats else null
				check(col_bricks != null and col_bricks.text == "Bricks" and col_deaths != null and col_deaths.text == "Deaths", "leaderstats are the columns")
				var my_bricks: Label = row.get_node_or_null("Bricks") if row else null
				var my_deaths: Label = row.get_node_or_null("Deaths") if row else null
				check(my_bricks != null and my_bricks.text.is_valid_int() and my_deaths != null and my_deaths.text == "0", "a row shows its values: %s / %s" % [my_bricks.text if my_bricks else "", my_deaths.text if my_deaths else ""])
				check(my_bricks != null and col_bricks != null and my_bricks.global_position.x > row.get_node("Name").global_position.x and absf(my_bricks.global_position.x - col_bricks.global_position.x) < 0.5, "in columns under the names")
				check(bob.get_node_or_null("CoreGui/PlayerList/Panel/List/Ada") != null and server.get_node_or_null("CoreGui/PlayerList") == null, "Bob has one too; the server draws no CoreGui")
				check(client.get_node_or_null("CoreGui/Health") == null, "no health bar while I am unhurt")
				old_body = body
				_key(KEY_W, false)
				phase = 11
		11:
			# The resting pose is resent reliably after the unreliable stream
			if t > 3.8:
				var srv_root: Node3D = server.get_part_node(client.get_part_id(body))
				var bobs_view: Node3D = bob.get_part_node(client.get_part_id(body))
				var rest_gap := (srv_root.global_position - body.global_position).length() if srv_root else 1e9
				var rest_gap2 := (bobs_view.global_position - body.global_position).length() if bobs_view else 1e9
				check(body.velocity.length() < 0.01 and rest_gap < 0.05 and rest_gap2 < 0.05, "at rest, the server's and Bob's copies sit exactly where Ada stopped: %.3f / %.3f" % [rest_gap, rest_gap2])
				# Roblox chat: / focuses the box, which then takes the keys; Enter sends
				_key(KEY_SLASH, true)
				_key(KEY_SLASH, false)
				chat_at = t
				phase = 12
		12:
			var input: LineEdit = client.get_node_or_null("CoreGui/Chat/Window/Box/Input")
			if chat_step == 0 and t > chat_at + 0.3:
				check(input != null and input.has_focus(), "/ focuses the chat box")
				chat_pos = body.global_position
				_key(KEY_W, true)
				chat_step = 1
			elif chat_step == 1 and t > chat_at + 0.9:
				_key(KEY_W, false)
				# a walk is ten studs; 0.5 leaves room for a fresh character settling onto the floor
				check((body.global_position - chat_pos).length() < 0.5, "W typed into the box does not walk: moved %.3f" % (body.global_position - chat_pos).length())
				if input:
					input.text = "hi Bob"
					input.text_submitted.emit("hi Bob")
				chat_step = 2
			elif chat_step == 2 and t > chat_at + 1.8:
				check(_has("Ada\tsaid\thi Bob"), "Player.Chatted on the server")
				check(input != null and not input.has_focus() and input.text == "", "Enter sends, clears and leaves the box")
				var last_of := func(w: PulseBlockzWorld) -> String:
					var msgs: Node = w.get_node_or_null("CoreGui/Chat/Window/Box/Messages")
					return msgs.get_child(msgs.get_child_count() - 1).get_parsed_text() if msgs and msgs.get_child_count() > 0 else "nothing"
				check(last_of.call(bob) == "Ada: hi Bob" and last_of.call(client) == "Ada: hi Bob", "the line is in Bob's chat window and Ada's: %s / %s" % [last_of.call(bob), last_of.call(client)])
				var bobs_view: Node3D = bob.get_part_node(client.get_part_id(body))
				var bob_head: Node3D = null
				if bobs_view: for c in bobs_view.get_children(): if c is MeshInstance3D and c.name == "Head": bob_head = c
				var expect: Vector2 = bob.get_camera().unproject_position(bob_head.global_position + Vector3(0, 1.4, 0)) if bob_head else Vector2()
				var bubble: Control = bob.get_node_or_null("CoreGui/BubbleChat/Ada")
				check(bubble != null and bubble.visible and bubble.get_node("Text").text == "hi Bob" and bubble.global_position.y + bubble.size.y < expect.y - 30 and absf(bubble.global_position.x + bubble.size.x / 2 - expect.x) < 2,
					"a bubble over Ada's head on Bob's screen: %s (%s) over %s" % [bubble.global_position if bubble else Vector2(), bubble.size if bubble else Vector2(), expect])
				check(server.get_node_or_null("CoreGui/Chat") == null, "the server draws no chat")
				_key(KEY_W, true)
				phase = 2
		2:
			# W is still held; the lava is ahead and the touch is the client's to report
			if _has("Ada\tdied") or t > 16.0:
				_key(KEY_W, false)
				check(_has("Ada\tdied"), "the lava killed the character (client-reported Touched, Humanoid.Died on the server)")
				died_at = t
				phase = 21
		21:
			if t > died_at + 1.0:
				var health: Control = client.get_node_or_null("CoreGui/Health")
				var fill: Control = health.get_node_or_null("Bar/Fill") if health else null
				check(health != null and health.visible and fill != null and fill.size.x < 0.5, "my own health bar shows while hurt: empty after dying")
				phase = 3
		3:
			if t > died_at + 6.5:   # Players.RespawnTime is 5
				check(_has("Client") and _has("ouch"), "the client saw Died too")
				check(body != null and body != old_body and is_instance_valid(body), "a new character body after RespawnTime")
				var at := body.global_position if body else Vector3(99, 99, 99)
				check(Vector2(at.x, at.z - 4).length() < 1.5 and at.y > 0.5 and at.y < 5.0, "back at the spawn: %s" % at)
				var health: Control = client.get_node_or_null("CoreGui/Health")
				check(health != null and not health.visible, "and the health bar goes when the new character is whole")
				var deaths: Label = bob.get_node_or_null("CoreGui/PlayerList/Panel/List/Ada/Deaths")
				check(deaths != null and deaths.text == "1", "Ada's Deaths stat is 1 on Bob's list: %s" % (deaths.text if deaths else "none"))
				bob.disconnect_from_server()
				left_at = t
				phase = 4
		4:
			if t > left_at + 1.0:
				check(left_name == "Bob" and server.get_client_count() == 1, "Bob leaving: client_left on the server, %d client left" % server.get_client_count())
				check(server.get_part_node(bob_root) == null and client.get_part_node(bob_root) == null, "his character is gone from the server's scene and Ada's")
				check(not bob.is_server_connected(), "and Bob knows he is offline")
				# Terrain a server script builds goes back into the tree as a host write, and host
				# writes replicate like the physics snapshot
				server.run_chunk("terrain", "workspace.Terrain:FillBlock(CFrame.new(0, 4, 0), Vector3.new(16, 8, 16), Enum.Material.Rock)")
				# An UnreliableRemoteEvent rides this wire as a RemoteEvent does
				server.run_chunk("fast", "local f = Instance.new('UnreliableRemoteEvent') f.Name = 'Fast' f.Parent = game.ReplicatedStorage "
					+ "f.OnServerEvent:Connect(function(pl, m) print('fast', pl.Name, m) f:FireAllClients('back') end)")
				client.run_chunk("fastc", "task.spawn(function() local f = game.ReplicatedStorage:WaitForChild('Fast') "
					+ "f.OnClientEvent:Connect(function(m) print('fastback', m) end) f:FireServer('ping') end)")
				terrain_at = t
				phase = 5
		5:
			if t > terrain_at + 1.5:
				check(absf(server.terrain_height_at(0, 0) - 10.0) < 0.6,
					"Terrain:FillBlock on the server built a block (its top read as Roblox reads occupancy, 2 past the face): %s" % server.terrain_height_at(0, 0))
				check(absf(client.terrain_height_at(0, 0) - 10.0) < 0.6,
					"and it reached Ada's client over the wire: %s" % client.terrain_height_at(0, 0))
				check(client.terrain_material_at(Vector3(0, 4, 0)) == "Rock", "material and all")
				return _finish()
	return false

func _finish() -> bool:
	if OS.get_environment("NET_TEST_LOGS") != "":
		for w in [["server", server], ["client", client]]:
			var id := 0
			for want in ["ReplicatedStorage"]:
				for cid in (w[1] as PulseBlockzWorld).get_child_ids(id):
					if String(((w[1] as PulseBlockzWorld).get_instance(cid) as Dictionary).get("name", "")) == want:
						id = cid
						break
			var kids: Array[String] = []
			for cid in (w[1] as PulseBlockzWorld).get_child_ids(id):
				kids.append(String(((w[1] as PulseBlockzWorld).get_instance(cid) as Dictionary).get("name", "")))
			print("RS %s: %s" % [w[0], ", ".join(kids)])
			for cid in (w[1] as PulseBlockzWorld).get_child_ids(id):
				var nm := String(((w[1] as PulseBlockzWorld).get_instance(cid) as Dictionary).get("name", ""))
				print("   ID %s %s = %d" % [w[0], nm, cid])
		for l in logs:
			print("LOG ", l)
	for l in logs:
		if l.begins_with("ERROR"): check(false, l)
	check(_has("Server ServerScriptService.Main: server up"), "the server ran its Scripts")
	check(_has("Client Players.Ada.PlayerScripts.Client: client up"), "LocalScript ran on the client")
	check(_has("Client Players.Ada.PlayerScripts.Client: my character is here"), "LocalPlayer.Character reached the client")
	check(_has("dropped a brick on\tAda"), "RemoteEvent client -> server over the wire")
	check(_has("fast\tAda\tping") and _has("fastback\tback"), "an UnreliableRemoteEvent made at runtime replicates, fires up to the server and back down")
	check(_has("welcome\tAda\tuserId\t7"), "PlayerAdded on the server with the Hello's name and userId")
	print("net: %s" % ("PASS" if failed == 0 else "%d FAILED" % failed))
	quit(1 if failed else 0)
	return true
