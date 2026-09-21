# Team Create: godot --headless --path . -s res://tests/team_test.gd
#
# A host and a guest edit world on loopback. The whole place mirrors to the guest --
# ServerScriptService and a Script's Source included -- and the guest's edits land on the host.
extends SceneTree

const PORT := 8803
var host: PulseBlockzWorld
var guest: PulseBlockzWorld
var t := 0.0
var at := 0.0
var phase := 0
var failed := 0
var logs: Array[String] = []
var shared := 0

func _initialize() -> void:
	var main := Node.new()
	main.name = "Main"
	root.add_child(main)
	host = PulseBlockzWorld.new()
	host.name = "Host"
	host.mode = PulseBlockzWorld.MODE_SERVER
	host.edit_mode = true
	host.auto_join = false
	host.listen_port = PORT
	host.bind_address = "127.0.0.1"
	host.track_properties = true
	host.default_controls = false
	host.default_camera = false
	main.add_child(host)
	guest = PulseBlockzWorld.new()
	guest.name = "Guest"
	guest.mode = PulseBlockzWorld.MODE_CLIENT
	guest.edit_mode = true
	guest.auto_join = true
	guest.server_address = "127.0.0.1"
	guest.server_port = PORT
	guest.player_name = "Bob"
	guest.user_id = 8
	guest.track_properties = true
	guest.default_controls = false
	guest.default_camera = false
	main.add_child(guest)
	for w in [host, guest]:
		var tag: String = w.name
		w.script_print.connect(func(n, s): logs.append(tag + " " + n + ": " + s))
		w.script_error.connect(func(n, e): logs.append("ERROR " + tag + " " + n + ": " + e))
	host.run_chunk("setup", "local p = Instance.new('Part') p.Name = 'Shared' p.Anchored = true p.Position = Vector3.new(0, 5, 0) p.Parent = workspace local s = Instance.new('Script') s.Name = 'Secret' s.Source = 'print(1)' s.Parent = game:GetService('ServerScriptService')")

func check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok:
		failed += 1

func _service(w: PulseBlockzWorld, cls: String) -> int:
	for id in w.get_child_ids(0):
		if w.get_instance(id).get("class_name", "") == cls:
			return id
	return 0

func _in(w: PulseBlockzWorld, parent: int, name: String) -> int:
	if parent == 0:
		return 0
	for id in w.get_child_ids(parent):
		if w.get_instance(id).get("name", "") == name:
			return id
	return 0

func _prop(w: PulseBlockzWorld, id: int, name: String):
	for p in w.get_properties(id, true):   # hidden ones too: a Script's Source
		if p.name == name:
			return p.value
	return null

func _process(delta: float) -> bool:
	t += delta
	match phase:
		0:
			if guest.is_server_connected() or t > 10.0:
				check(guest.is_server_connected(), "the guest joined the host's edit world")
				at = t
				phase = 1
		1:
			if t > at + 1.0:
				shared = _in(guest, _service(guest, "Workspace"), "Shared")
				var secret := _in(guest, _service(guest, "ServerScriptService"), "Secret")
				check(shared != 0, "the host's place is mirrored on the guest (Workspace.Shared)")
				check(secret != 0 and str(_prop(guest, secret, "Source")) == "print(1)", "everything replicates in Team Create: ServerScriptService and a Script's Source included: %d %s" % [secret, _prop(guest, secret, "Source") if secret != 0 else "-"])
				check(_in(host, _service(host, "Players"), "Bob") != 0, "the guest is a Player on the host: who is here")
				check(_in(host, _service(host, "Workspace"), "Bob") == 0, "with no character: an edit world spawns none")
				guest.set_property(shared, "Transparency", 0.5)
				guest.create_instance("Part", _service(guest, "Workspace"))
				at = t
				phase = 2
		2:
			if t > at + 1.0:
				check(is_equal_approx(float(_prop(host, shared, "Transparency")), 0.5), "a guest's property write lands on the host")
				check(is_equal_approx(float(_prop(guest, shared, "Transparency")), 0.5), "and comes back to the guest's mirror")
				var made_h := _in(host, _service(host, "Workspace"), "Part")
				var made_g := _in(guest, _service(guest, "Workspace"), "Part")
				check(made_h != 0 and made_g == made_h, "a guest's new Instance arrives on both, one id: %d %d" % [made_h, made_g])
				if made_g != 0:
					guest.set_parent(made_g, _service(guest, "ReplicatedStorage"))
				guest.run_chunk("cmd", "print('from the guest', workspace.Shared.Transparency)")
				at = t
				phase = 3
		3:
			if t > at + 1.0:
				check(_in(host, _service(host, "ReplicatedStorage"), "Part") != 0, "a guest's move lands on the host")
				check(logs.any(func(l): return l.begins_with("Host") and l.contains("from the guest	0.5")), "a guest's command bar runs on the host: %s" % [logs.filter(func(l): return l.contains("guest"))])
				var moved := _in(guest, _service(guest, "ReplicatedStorage"), "Part")
				if moved != 0:
					guest.destroy_instance(moved)
				guest.add_model("Workspace", "Kit", '{"className":"Model","children":[{"className":"Part","name":"Brick","properties":{"Anchored":true}}]}')
				at = t
				phase = 4
		4:
			if t > at + 1.0:
				check(_in(host, _service(host, "ReplicatedStorage"), "Part") == 0, "a guest's Destroy lands on the host")
				var kit := _in(host, _service(host, "Workspace"), "Kit")
				check(kit != 0 and _in(host, kit, "Brick") != 0, "a guest's inserted model lands on the host")
				var kit_g := _in(guest, _service(guest, "Workspace"), "Kit")
				check(kit_g != 0 and kit_g == kit, "and on the guest, the same id")
				print("team: " + ("PASS" if failed == 0 else "%d FAILED" % failed))
				quit(1 if failed > 0 else 0)
	return false
