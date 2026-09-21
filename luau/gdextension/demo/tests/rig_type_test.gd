# Humanoid.RigType is writable, and a StarterCharacter's own rig wins.
#
#   godot --headless --path . -s res://tests/rig_type_test.gd
#
# A StarterCharacter is cloned as it was built, so the limb set added to the clone has to be the
# one the model already is. StarterPlayer.CharacterRigType decides only when nothing else does.
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
	lines.append("RIGTYPE %-52s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	# An R15 StarterCharacter under a StarterPlayer that says R6: the model must win, or the
	# clone gets a Torso bolted on beside its UpperTorso.
	world.load_file("ServerScriptService/Rig.server.luau", """
local players = game:GetService("Players")
local sp = game:GetService("StarterPlayer")
sp.CharacterRigType = Enum.HumanoidRigType.R6

local sc = Instance.new("Model")
sc.Name = "StarterCharacter"
local root = Instance.new("Part")
root.Name = "HumanoidRootPart"
root.Size = Vector3.new(2, 2, 1)
root.Parent = sc
local upper = Instance.new("Part")
upper.Name = "UpperTorso"
upper.Size = Vector3.new(2, 1.6, 1)
upper.Parent = sc
local hum = Instance.new("Humanoid")
local wrote = pcall(function() hum.RigType = Enum.HumanoidRigType.R15 end)
print("WROTE " .. tostring(wrote) .. " " .. tostring(hum.RigType))
hum.Parent = sc
sc.Parent = sp

local p = players:GetPlayers()[1] or players.PlayerAdded:Wait()
p:LoadCharacter()
task.wait(1.5)
local ch = p.Character
print("HASUPPER " .. tostring(ch and ch:FindFirstChild("UpperTorso") ~= nil))
print("HASTORSO " .. tostring(ch and ch:FindFirstChild("Torso") ~= nil))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 9.0:
		return false
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("a script may set RigType", heard.call("WROTE true"), true)
	check("and it reads back as what was set", heard.call("WROTE true Enum.HumanoidRigType.R15"), true)
	check("the R15 model keeps its UpperTorso", heard.call("HASUPPER true"), true)
	check("and gets no R6 Torso bolted on", heard.call("HASTORSO false"), true)
	check("no script error", heard.call("ERROR"), false)
	for l in lines:
		print(l)
	print("RIGTYPE %d passed, %d failed" % [ok, bad])
	print("rig type: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
