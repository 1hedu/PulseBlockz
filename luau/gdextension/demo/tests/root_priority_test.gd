# RootPriority says which part of an assembly is its root.
#
#   godot --headless --path . -s res://tests/root_priority_test.gd
#
# Roblox picks the root by highest RootPriority, then by anchoring, then by mass. A place that
# sets it is saying which part the assembly should be steered by, so it has to outrank both.
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
	lines.append("ROOTPRI %-52s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = false
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("ServerScriptService/Root.server.luau", """
local function weld(a, b)
	local w = Instance.new("Weld")
	w.Part0, w.Part1 = a, b
	w.Parent = a
end

-- A small part welded to a big one. By mass the big one roots the assembly.
local big = Instance.new("Part")
big.Size = Vector3.new(8, 8, 8)
big.Position = Vector3.new(0, 20, 0)
big.Anchored = false
big.Name = "Big"
big.Parent = workspace

local small = Instance.new("Part")
small.Size = Vector3.new(1, 1, 1)
small.Position = Vector3.new(0, 25, 0)
small.Anchored = false
small.Name = "Small"
small.Parent = workspace
weld(big, small)

print("READ " .. tostring(small.RootPriority))
print("BYMASS " .. tostring(small.AssemblyRootPart and small.AssemblyRootPart.Name))

-- The small one asks to be the root, and outranks the heavier part.
small.RootPriority = 10
task.wait(0.2)
print("BYPRIORITY " .. tostring(small.AssemblyRootPart and small.AssemblyRootPart.Name))

-- It outranks an anchored part too, which would otherwise win.
big.Anchored = true
task.wait(0.2)
print("OVERANCHOR " .. tostring(small.AssemblyRootPart and small.AssemblyRootPart.Name))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 6.0:
		return false
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("RootPriority reads, and starts at zero", heard.call("READ 0"), true)
	check("nothing set: the heavier part roots it", heard.call("BYMASS Big"), true)
	check("set: the part that asked roots it", heard.call("BYPRIORITY Small"), true)
	check("and it outranks an anchored part", heard.call("OVERANCHOR Small"), true)
	check("no script error", heard.call("ERROR"), false)
	for l in lines:
		print(l)
	print("ROOTPRI %d passed, %d failed" % [ok, bad])
	print("root priority: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
