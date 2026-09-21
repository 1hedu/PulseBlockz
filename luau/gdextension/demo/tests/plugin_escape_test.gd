# A place's script cannot make itself a plugin.
#
#   godot --headless --path . -s res://tests/plugin_escape_test.gd
#
# Plugin capability is "runs under PluginDebugService", and only the host puts things there. A
# script that could parent itself in -- by Parent, Instance.new's second argument, or a clone --
# would then write Source, require the text, and switch HttpService on. Every route must fail,
# the script must still be where it was, and a place file addressed at the service is refused.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var ok := 0
var bad := 0
var said: Array[String] = []
var lines: Array[String] = []

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("ESCAPE %-56s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = false
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("ServerScriptService/Escape.server.luau", """
local svc = game:GetService("PluginDebugService")
local function try(tag, f)
	local ok, err = pcall(f)
	print(tag .. " " .. tostring(ok) .. " " .. tostring(err))
end
try("PARENT", function() script.Parent = svc end)
print("STILLHOME " .. tostring(script.Parent == game:GetService("ServerScriptService")))
try("NEW", function() Instance.new("Script", svc) end)
try("CLONE", function() local c = script:Clone(); c.Parent = svc end)
try("DEEP", function()
	local f = Instance.new("Folder"); f.Parent = svc
	local s = Instance.new("Script"); s.Parent = f
end)
print("EMPTY " .. tostring(#svc:GetChildren()))
try("SOURCE", function() local m = Instance.new("ModuleScript"); m.Source = "return 1" end)
try("HTTP", function() game:GetService("HttpService").HttpEnabled = true end)
print("DONE")
""")
	# A place file addressed at the service, as a published bundle could carry: refused by the loader.
	world.load_file("PluginDebugService/Evil.server.luau", "print('EVIL RAN')")

func _process(delta: float) -> bool:
	t += delta
	if t < 4.0:
		return false
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("Parent = the service is refused", heard.call("PARENT false"), true)
	check("and names the capability", heard.call("PARENT false") and heard.call("lacking capability Plugin"), true)
	check("the script is still where it was", heard.call("STILLHOME true"), true)
	check("Instance.new(cls, service) is refused", heard.call("NEW false"), true)
	check("a clone cannot go there either", heard.call("CLONE false"), true)
	check("nor anything under it", heard.call("DEEP false"), true)
	check("the service stays empty", heard.call("EMPTY 0"), true)
	check("Source stays a plugin's to write", heard.call("SOURCE false"), true)
	check("HttpEnabled stays a plugin's to write", heard.call("HTTP false"), true)
	check("a place file cannot be placed under the service", heard.call("a place file cannot go there"), true)
	check("and it never ran", heard.call("EVIL RAN"), false)
	var other_errors := false
	for l in said:
		if l.begins_with("ERROR") and l.find("a place file cannot go there") == -1:
			other_errors = true
	check("no other script error", other_errors, false)
	for l in lines:
		print(l)
	print("ESCAPE %d passed, %d failed" % [ok, bad])
	print("plugin escape: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
