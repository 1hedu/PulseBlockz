# The camera never sits inside or behind the world.
#
#   godot --headless --path . -s res://tests/camera_pop_test.gd
#
# Roblox's popper: a ray from the subject to where the camera wants to be stops at the first
# thing in the way, and the camera comes in to just short of it. Physics runs headless, so the
# ray is real even though nothing is drawn.
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
	lines.append("CAMPOP %-52s %-9s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.default_camera = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	# A floor to stand on, and a wall four studs behind the spawn, right across the line the
	# default camera sits on (it hangs back along +Z at 12.5 studs).
	world.load_file("ServerScriptService/Room.server.luau", """
local floor = Instance.new("Part")
floor.Name = "Floor"; floor.Size = Vector3.new(200, 1, 200); floor.Position = Vector3.new(0, -0.5, 0)
floor.Anchored = true; floor.Parent = workspace
local wall = Instance.new("Part")
wall.Name = "Wall"; wall.Size = Vector3.new(60, 40, 1); wall.Position = Vector3.new(0, 20, 4)
wall.Anchored = true; wall.Parent = workspace
local Players = game:GetService("Players")
local p = Players:GetPlayers()[1] or Players.PlayerAdded:Wait()
local char = p.Character or p.CharacterAdded:Wait()
local rootPart = char:WaitForChild("HumanoidRootPart", 10)
task.wait(0.5)
rootPart.CFrame = CFrame.new(0, 3, 0)
task.wait(2)
print("ROOTZ " .. string.format("%.2f", rootPart.Position.Z))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 7.0:
		return false
	var cam: Camera3D = world.get_camera()
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
	var root_z: float = num.call("ROOTZ")
	var cam_z: float = cam.global_position.z if cam != null else 1e9
	var wall_near_face := 4.0 - 0.5
	check("the script ran to the end", heard.call("DONE"), true)
	check("there is a default camera", cam != null, true)
	check("it sits behind the character", cam_z > root_z + 1.0, true)
	check("but on the near side of the wall", cam_z < wall_near_face, true)
	check("and not inside the character's head", cam_z > root_z + 0.4, true)
	check("no script error", heard.call("ERROR"), false)
	lines.append("CAMPOP   root z %.2f, wall near face z %.2f, camera z %.2f (wanted 12.5 back without the wall)" % [root_z, wall_near_face, cam_z])
	for l in lines:
		print(l)
	print("CAMPOP %d passed, %d failed" % [ok, bad])
	print("camera pop: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
