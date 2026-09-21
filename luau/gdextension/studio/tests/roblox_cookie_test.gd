# A Roblox session cookie goes to Roblox and nowhere else.
#
#   godot --headless --path . -s res://tests/roblox_cookie_test.gd
#
# A place's own meshes are private assets: assetdelivery answers 401 to an anonymous fetch, so
# a place imports as untextured blocks. Signing in fixes that, and the cookie is full account
# access -- cloud_fetch_base is settable, so an unguarded one would go wherever it is pointed.
extends SceneTree

const PORT := 8137

var ok := 0
var bad := 0
var server := TCPServer.new()
var world: PulseBlockzWorld

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	print("COOKIE %-52s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	_run.call_deferred()

## Everything sent on the first connection to arrive, as text.
func _knock_text() -> String:
	var out := ""
	while server.is_connection_available():
		var c := server.take_connection()
		if c == null:
			continue
		for i in 40:
			c.poll()
			if c.get_available_bytes() > 0:
				out += c.get_utf8_string(c.get_available_bytes())
			if out.find("\r\n\r\n") != -1:
				break
			OS.delay_msec(10)
	return out

func _run() -> void:
	check("a listener is up", server.listen(PORT, "127.0.0.1"), OK)
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = false
	world.data_store_path = ""
	root.add_child(world)

	check("no cookie by default", world.has_cloud_cookie(), false)

	# The value is taken with or without the tag Roblox writes in front of it, and without a
	# trailing semicolon. Checked by length: nothing reads the value back out.
	world.set_cloud_cookie("_|WARNING:-DO-NOT-SHARE|_ABC")
	check("a bare value is kept whole", world.cloud_cookie_len(), 28)
	world.set_cloud_cookie(".ROBLOSECURITY=_|WARNING:-DO-NOT-SHARE|_ABC;")
	check("the .ROBLOSECURITY= tag is stripped", world.cloud_cookie_len(), 28)
	check("and it says it has one", world.has_cloud_cookie(), true)

	# A Windows editor saves the cookie file with a UTF-8 BOM, and U+FEFF is not whitespace, so
	# strip_edges() leaves it on the front. Sent in the header it makes Roblox answer 401 with a
	# cookie that is otherwise perfectly good.
	world.set_cloud_cookie("﻿_|WARNING:-DO-NOT-SHARE|_ABC")
	check("a UTF-8 BOM is stripped", world.cloud_cookie_len(), 28)

	# Anything left that is not ASCII is a mis-encoded file (UTF-16, say), not a cookie. A
	# header value cannot carry it, so it is refused outright rather than sent broken.
	world.set_cloud_cookie("_|WARNING:-DO-NOT-SHARE|_é")
	check("a non-ascii value is refused whole", world.has_cloud_cookie(), false)
	world.set_cloud_cookie("_|WARNING:-DO-NOT-SHARE|_ABC")

	# Pointed at this listener rather than Roblox: the fetch goes out, the cookie does not.
	world.cloud_fetch_base = "http://127.0.0.1:%d/v1/asset/?id=" % PORT
	world.set_cloud_cookie("SECRETCOOKIEVALUE")
	# PreloadAsync is the engine's own route into cloud_local, so this exercises the real
	# fetch rather than whatever a headless renderer would or would not ask for.
	world.load_file("ServerScriptService/Ask.server.luau", """
local cp = game:GetService("ContentProvider")
local d = Instance.new("Decal")
d.Texture = "rbxassetid://123456789"
d.Parent = workspace
cp:PreloadAsync({d})
print("ASKED")
""")
	var seen := ""
	for i in 150:
		await process_frame
		seen += _knock_text()
		if seen.find("\r\n\r\n") != -1:
			break

	check("the fetch was made", seen.find("GET ") != -1, true)
	check("no Cookie header on a base that is not Roblox", seen.to_lower().find("cookie:") != -1, false)
	check("and the value is nowhere in the request", seen.find("SECRETCOOKIEVALUE") != -1, false)
	check("the fetch still identifies as Roblox", seen.find("Roblox/WinInet") != -1, true)

	server.stop()
	print("COOKIE %d passed, %d failed" % [ok, bad])
	print("roblox cookie: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
