# A file cannot bring a plugin with it. A plugin Script runs even in an edit world
# (Runtime::Impl::shouldRun checks isPluginScript before opts.runScripts), and isPluginScript
# matches on the identity of the service addPlugin uses, not on the class.
#
#   godot --headless --path . -s res://tests/plugin_provenance_test.gd
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var ok := 0
var bad := 0
var lines: Array[String] = []
var said: Array[String] = []

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("PLUGIN %-52s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "Edit"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.edit_mode = true            # the tree is there, nothing of the place runs
	world.auto_join = false
	world.data_store_path = ""
	world.script_print.connect(func(_n, line): said.append(String(line)))
	world.script_warn.connect(func(_n, line): said.append(String(line)))
	world.script_error.connect(func(_n, line): said.append("ERROR " + String(line)))
	root.add_child(world)

	# Door one: a place file carrying a plugin.
	#
	# Roblox's XML, not JSON: import_place reads Roblox's own formats and rejects anything else.
	# Fed JSON it reported "not a Roblox binary model" and imported nothing, so the check below
	# passed because the file had never been opened. The importer's own report is asserted now.
	world.import_place(("""<roblox version="4">
 <Item class="PluginDebugService" referent="RBX0">
  <Properties><string name="Name">PluginDebugService</string></Properties>
  <Item class="Script" referent="RBX1">
   <Properties>
    <string name="Name">Smuggled</string>
    <ProtectedString name="Source">print('SMUGGLED BY THE FILE')</ProtectedString>
   </Properties>
  </Item>
 </Item>
</roblox>
""").to_utf8_buffer())

	# Door two: a script making its own service. Run as a chunk, which has the reach a plugin
	# has, because an edit world runs no place scripts.
	world.run_chunk("door2", """
local fake = Instance.new("PluginDebugService")
fake.Parent = workspace
local s = Instance.new("Script")
s.Name = "Fake"
s.Source = "print('SMUGGLED BY A SCRIPT')"
s.Parent = fake
print("door2 planted")
""")

	world.plugin_add("Real", "print('A REAL PLUGIN RAN')")

func _process(delta: float) -> bool:
	t += delta
	if t < 6.0:
		return false
	var heard := func(s: String) -> bool:
		var hit := false
		for l in said:
			if l.find(s) != -1:
				hit = true
		return hit
	check("the file really was imported", heard.call("imported") and heard.call("instance"), true)
	check("and the importer did not reject it", heard.call("not a Roblox") or heard.call("ERROR import"), false)
	check("a script could plant its own service", heard.call("door2 planted"), true)
	check("a plugin the person installed still runs", heard.call("A REAL PLUGIN RAN"), true)
	check("a plugin carried in by a file does not", heard.call("SMUGGLED BY THE FILE"), false)
	check("nor one a script parented under its own", heard.call("SMUGGLED BY A SCRIPT"), false)
	for l in lines:
		print(l)
	print("PLUGIN %d passed, %d failed" % [ok, bad])
	print("plugin provenance: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
