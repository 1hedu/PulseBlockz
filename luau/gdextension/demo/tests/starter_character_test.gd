# StarterCharacter: godot --headless --path . -s res://tests/starter_character_test.gd
#
# StarterPlayer.StarterCharacter -- a Model holding a Humanoid and a HumanoidRootPart, the shape
# the engine insists on before it uses one -- is cloned for each spawn in place of the default R6
# rig, moved whole to the spawn with a fresh Animator, and StarterPlayer's settings reach its
# Humanoid.
extends SceneTree

var world: PulseBlockzWorld
var lines: Array[String] = []
var failed := 0
var t := 0.0
var phase := 0

func check(ok: bool, what: String) -> void:
	print(("  ok   " if ok else "  FAIL ") + what)
	if not ok:
		failed += 1

func _initialize() -> void:
	var main := Node.new()
	root.add_child(main)
	world = PulseBlockzWorld.new()
	world.mode = PulseBlockzWorld.MODE_SERVER
	world.auto_join = false
	world.default_camera = false
	world.default_controls = false
	main.add_child(world)
	world.script_print.connect(func(_n, s): lines.append(String(s)))
	world.script_error.connect(func(n, e): lines.append("ERROR %s: %s" % [n, e]))

func _said(prefix: String) -> String:
	for l in lines:
		if l.begins_with(prefix):
			return l
	return ""

func _process(delta: float) -> bool:
	t += delta
	match phase:
		0:
			if t > 0.5:
				world.run_chunk("floor", "local p = Instance.new('SpawnLocation') p.Anchored = true p.Position = Vector3.new(40, 10, 0) p.Size = Vector3.new(12, 1, 12) p.Duration = 0 p.Parent = workspace")
				world.add_player("Plain", 0)
				phase = 1
				t = 0.0
		1:
			if t > 0.5:
				world.run_chunk("plain", """
local c = workspace:WaitForChild("Plain", 5)
local h = c:FindFirstChildOfClass("Humanoid")
print("PLAIN", c["Left Arm"].Size.Y, c["Right Arm"].RightGripAttachment.Position.Y, tostring(h.UseJumpPower))
local copy = c:Clone()
copy.Name = "StarterCharacter"
copy["Left Arm"].Size = Vector3.new(1, 2.6, 1)
copy.Humanoid.Animator:Destroy()
copy:PivotTo(CFrame.new(0, 500, 0))
copy.Parent = game:GetService("StarterPlayer")
local sp = game:GetService("StarterPlayer")
sp.CharacterUseJumpPower = true
sp.CharacterJumpPower = 70
""")
				world.add_player("Shaped", 0)
				phase = 2
				t = 0.0
		2:
			if t > 0.5:
				world.run_chunk("shaped", """
local c = workspace:WaitForChild("Shaped", 5)
local h = c:FindFirstChildOfClass("Humanoid")
local root = c.HumanoidRootPart
print("SHAPED", c["Left Arm"].Size.Y, c["Right Arm"].Size.Y,
	math.floor((root.Position - Vector3.new(40, 10, 0)).Magnitude + 0.5),
	math.floor((c.Torso.Position - root.Position).Magnitude + 0.5),
	tostring(h.UseJumpPower), h.JumpPower, tostring(h:FindFirstChildOfClass("Animator") ~= nil),
	c.PrimaryPart == root, c.Torso["Left Shoulder"].Part1 == c["Left Arm"])
""")
				phase = 3
				t = 0.0
		3:
			if t > 0.5:
				check(_said("PLAIN") == "PLAIN\t2\t-1\tfalse", "with nothing set, Roblox's R6: 2-stud arms, the grip at the arm's end, UseJumpPower false: %s" % _said("PLAIN"))
				var shaped := _said("SHAPED").split("\t")
				check(shaped.size() == 10 and is_equal_approx(float(shaped[1]), 2.6) and is_equal_approx(float(shaped[2]), 2),
					"a StarterCharacter is the next body, the arm it lengthened still long: %s" % _said("SHAPED"))
				# 6 studs: slack over the 3.5 an R6 root stands above the SpawnLocation's Position
				check(shaped.size() == 10 and int(shaped[3]) <= 6 and shaped[4] == "0",
					"moved whole to the spawn, from where it was built 500 studs up: %s studs off, torso %s from the root" % [shaped[3] if shaped.size() > 3 else "?", shaped[4] if shaped.size() > 4 else "?"])
				check(shaped.size() == 10 and shaped[5] == "true" and shaped[6] == "70", "StarterPlayer's CharacterUseJumpPower and CharacterJumpPower are on its Humanoid")
				check(shaped.size() == 10 and shaped[7] == "true" and shaped[8] == "true" and shaped[9] == "true",
					"with an Animator of its own, its root as PrimaryPart and its joints pointing at its own limbs")
				check(not lines.any(func(l): return l.begins_with("ERROR")), "no script errors: %s" % [lines.filter(func(l): return l.begins_with("ERROR"))])
				print("starter character: " + ("PASS" if failed == 0 else "%d FAILED" % failed))
				quit(1 if failed > 0 else 0)
	return false
