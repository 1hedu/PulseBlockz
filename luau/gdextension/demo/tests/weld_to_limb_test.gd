# A part welded to a character's limb rides that limb.
#
#   godot --headless --path . -s res://tests/weld_to_limb_test.gd
#
# How a place dresses a rig it built itself: the visible mesh is welded to an invisible limb and
# carried by it. A weld that touches a character part is left out of the world's weld assemblies,
# because limbs are posed by the rig rather than by physics -- so whatever hung off one used to
# be orphaned, and stayed where it spawned while the character walked away.
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
	lines.append("WELDLIMB %-50s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("ServerScriptService/Weld.server.luau", """
local Players = game:GetService("Players")
local p = Players:GetPlayers()[1] or Players.PlayerAdded:Wait()
local char = p.Character or p.CharacterAdded:Wait()
local torso = char:WaitForChild("Torso", 10) or char:WaitForChild("UpperTorso", 10)
local root = char:WaitForChild("HumanoidRootPart", 10)
task.wait(1)

-- Dressing, the way a place that builds its own rig does it: a visible part welded to a limb,
-- kept in a Folder rather than directly under the character.
local folder = Instance.new("Folder")
folder.Name = "Armor"
folder.Parent = char
local shell = Instance.new("Part")
shell.Name = "Shell"
shell.Size = Vector3.new(2.2, 2.2, 1.2)
shell.CFrame = torso.CFrame
shell.Anchored = false
shell.CanCollide = false
shell.Massless = true
shell.Parent = folder
local w = Instance.new("WeldConstraint")
w.Part0 = torso
w.Part1 = shell
w.Parent = shell

task.wait(1)
local startOffset = (shell.Position - torso.Position).Magnitude
print("OFFSET0 " .. string.format("%.3f", startOffset))

-- Walk the character a good distance and see whether the shell came along.
root.CFrame = root.CFrame + Vector3.new(12, 0, 0)
task.wait(1.5)
local endOffset = (shell.Position - torso.Position).Magnitude
print("OFFSET1 " .. string.format("%.3f", endOffset))
print("MOVED " .. string.format("%.1f", (shell.Position - Vector3.new(0,0,0)).Magnitude))
print("TORSOX " .. string.format("%.1f", torso.Position.X))
print("SHELLX " .. string.format("%.1f", shell.Position.X))
print("SHELLY " .. string.format("%.1f", shell.Position.Y))
print("TORSOY " .. string.format("%.1f", torso.Position.Y))
print("APART " .. string.format("%.1f", (shell.Position - torso.Position).Magnitude))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 12.0:
		return false
	var num := func(tag: String) -> float:
		for l in said:
			if l.begins_with(tag + " "):
				return float(l.substr(tag.length() + 1))
		return 1e9
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	var torso_x: float = num.call("TORSOX")
	var shell_x: float = num.call("SHELLX")
	check("the script ran to the end", heard.call("DONE"), true)
	check("the shell starts on the limb", num.call("OFFSET0") < 0.6, true)
	check("the character actually moved", torso_x > 8.0, true)
	check("and the shell went with it", num.call("APART") < 2.0, true)
	check("and stays at the limb's own height", absf(num.call("SHELLY") - num.call("TORSOY")) < 2.0, true)
	check("no script error", heard.call("ERROR"), false)
	lines.append("WELDLIMB   torso (x %.1f, y %.1f)  shell (x %.1f, y %.1f)  %.1f studs apart" % [torso_x, num.call("TORSOY"), shell_x, num.call("SHELLY"), num.call("APART")])
	for l in lines:
		print(l)
	print("WELDLIMB %d passed, %d failed" % [ok, bad])
	print("weld to limb: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
