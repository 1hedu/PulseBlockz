# rbxassetid fetching: godot --headless --path . -s res://tests/cloud_test.gd
#
# A loopback server stands in for assetdelivery.roblox.com -- 1001 a PNG, 1002 a mesh, 1003 a WAV.
# A Decal, a MeshPart and a Sound each fetch once, are told apart by their bytes, and are cached.
extends SceneTree

const PORT := 8804
var world: PulseBlockzWorld
var server := TCPServer.new()
var peers: Array = []
var served: Array[String] = []
var t := 0.0
var phase := 0
var failed := 0
var logs: Array[String] = []

func _initialize() -> void:
	for f in ["1001.png", "1002.mesh", "1003.wav"]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path("user://roblox_cache/" + f))
	server.listen(PORT, "127.0.0.1")
	var main := Node.new()
	main.name = "Main"
	root.add_child(main)
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_SERVER
	world.edit_mode = true
	world.auto_join = false
	world.track_properties = true
	world.default_controls = false
	world.default_camera = false
	world.cloud_fetch_base = "http://127.0.0.1:%d/v1/asset/?id=" % PORT
	main.add_child(world)
	world.script_print.connect(func(n, s): logs.append(n + ": " + s))
	world.script_warn.connect(func(n, s): logs.append("WARN " + n + ": " + s))
	world.script_error.connect(func(n, e): logs.append("ERROR " + n + ": " + e))
	world.run_chunk("setup", """
		local board = Instance.new("Part"); board.Name = "Board"; board.Anchored = true; board.Size = Vector3.new(4, 4, 1); board.Position = Vector3.new(0, 5, 0); board.Parent = workspace
		local face = Instance.new("Decal"); face.Name = "Face"; face.Texture = "rbxassetid://1001"; face.Face = Enum.NormalId.Front; face.Parent = board
		local snd = Instance.new("Sound"); snd.Name = "Thud"; snd.SoundId = "rbxassetid://1003"; snd.Parent = board
		-- MeshPart.MeshId is nobody's to write from a script, as on Roblox: the part is made
		-- from its mesh, and the call yields until the mesh has come down.
		task.spawn(function()
			local mp = game:GetService("AssetService"):CreateMeshPartAsync(Content.fromUri("http://www.roblox.com/asset/?id=1002"))
			mp.Name = "Rock"; mp.Anchored = true; mp.Size = Vector3.new(2, 2, 2); mp.Position = Vector3.new(6, 5, 0); mp.Parent = workspace
		end)
	""")

func check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok:
		failed += 1

# ---- the stand-in asset delivery ----------------------------------------------------
func _body_for(id: String) -> Array:   # [bytes, content type]
	match id:
		"1001":
			var img := Image.create(8, 8, false, Image.FORMAT_RGBA8)
			img.fill(Color(1, 0, 0))
			return [img.save_png_to_buffer(), "image/png"]
		"1002":
			# 12-byte header, then the stride of a vertex and of a face; one triangle
			var b := StreamPeerBuffer.new()
			b.put_data("version 2.00\n".to_utf8_buffer())
			b.put_u16(12); b.put_u8(36); b.put_u8(12); b.put_u32(3); b.put_u32(1)
			for v in [[0, 0, 0], [1, 0, 0], [0, 1, 0]]:
				b.put_float(v[0]); b.put_float(v[1]); b.put_float(v[2])   # position
				b.put_float(0); b.put_float(0); b.put_float(1)            # normal
				b.put_float(v[0]); b.put_float(v[1]); b.put_float(0)      # uv
			b.put_u32(0); b.put_u32(1); b.put_u32(2)
			return [b.data_array, "application/octet-stream"]
		"1003":
			var pcm := PackedByteArray()
			pcm.resize(8000 * 2)   # a second of 16-bit silence at 8 kHz
			var b := StreamPeerBuffer.new()
			b.put_data("RIFF".to_utf8_buffer()); b.put_u32(36 + pcm.size()); b.put_data("WAVE".to_utf8_buffer())
			b.put_data("fmt ".to_utf8_buffer()); b.put_u32(16); b.put_u16(1); b.put_u16(1); b.put_u32(8000); b.put_u32(16000); b.put_u16(2); b.put_u16(16)
			b.put_data("data".to_utf8_buffer()); b.put_u32(pcm.size()); b.put_data(pcm)
			return [b.data_array, "audio/wav"]
	return [PackedByteArray(), ""]

func _serve() -> void:
	while server.is_connection_available():
		peers.append({"peer": server.take_connection(), "buf": PackedByteArray(), "sent": false})
	for p in peers:
		var c: StreamPeerTCP = p.peer
		c.poll()
		if p.sent:
			continue
		var avail := c.get_available_bytes()
		if avail > 0:
			p.buf += c.get_data(avail)[1]
		var text: String = p.buf.get_string_from_ascii()
		if not text.contains("\r\n\r\n"):
			continue
		var line := text.split("\r\n")[0]
		var id := ""
		var at: int = line.find("id=")
		if at >= 0:
			id = line.substr(at + 3).split(" ")[0]
		var body_type := _body_for(id)
		var body: PackedByteArray = body_type[0]
		var head: String
		if body.is_empty():
			head = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"
		else:
			head = "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % [body_type[1], body.size()]
		c.put_data(head.to_utf8_buffer())
		c.put_data(body)
		served.append(id)
		p.sent = true

func _service(cls: String) -> int:
	for id in world.get_child_ids(0):
		if world.get_instance(id).get("class_name", "") == cls:
			return id
	return 0

func _in(parent: int, name: String) -> int:
	if parent == 0:
		return 0
	for id in world.get_child_ids(parent):
		if world.get_instance(id).get("name", "") == name:
			return id
	return 0

func _prop(id: int, name: String):
	for p in world.get_properties(id, true):
		if p.name == name:
			return p.value
	return null

func _process(delta: float) -> bool:
	t += delta
	_serve()
	match phase:
		0:
			if served.size() >= 3 or t > 15.0:
				check(served.has("1001") and served.has("1002") and served.has("1003"), "each rbxassetid was fetched from asset delivery, once: %s" % [served])
				phase = 1
		1:
			if t > 3.0 and (served.size() >= 3 or t > 16.0):
				var ws := _service("Workspace")
				var board := _in(ws, "Board")
				var rock := _in(ws, "Rock")
				var thud := _in(board, "Thud")
				check(FileAccess.file_exists("user://roblox_cache/1001.png") and FileAccess.file_exists("user://roblox_cache/1002.mesh") and FileAccess.file_exists("user://roblox_cache/1003.wav"),
					"the bytes are told apart and cached as user://roblox_cache/<id>.png / .mesh / .wav")
				var board_mesh: Node3D = world.get_part_mesh(board)
				var face: MeshInstance3D = board_mesh.get_node_or_null("Face") if board_mesh else null
				var face_tex: bool = face != null and face.material_override is StandardMaterial3D and face.material_override.albedo_texture != null and face.material_override.albedo_texture.get_width() == 8
				check(face_tex, "the Decal wears the fetched PNG (8 by 8)")
				var rock_mesh: MeshInstance3D = world.get_part_mesh(rock)
				var verts := 0
				var box := AABB()
				if rock_mesh != null and rock_mesh.mesh != null and rock_mesh.mesh.get_surface_count() > 0:
					verts = rock_mesh.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX].size()
					box = rock_mesh.mesh.get_aabb()
				check(verts == 3 and box.size.x > 1.9 and box.size.y > 1.9, "the MeshPart draws the fetched Roblox mesh, fitted to its Size: %d vertices, %s" % [verts, box])
				check(_prop(thud, "IsLoaded") == true and float(_prop(thud, "TimeLength") if _prop(thud, "TimeLength") != null else 0.0) > 0.9, "the Sound loaded the fetched WAV: IsLoaded, TimeLength %s" % [_prop(thud, "TimeLength")])
				check(logs.any(func(l): return l.begins_with("Roblox: fetched rbxassetid://1002: mesh")), "the Output says what came: %s" % [logs.filter(func(l): return l.contains("fetched"))])
				if failed > 0: for l in logs: print("    ", l)
				print("cloud: " + ("PASS" if failed == 0 else "%d FAILED" % failed))
				phase = 2
				quit(1 if failed > 0 else 0)
	return false
