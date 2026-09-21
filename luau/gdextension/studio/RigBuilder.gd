# Roblox Studio's Avatar > Rig Builder: the Luau that builds an R6 or R15 block rig,
# run in the edit world and recorded as one insert for Ctrl+Z. Sizes, offsets, Motor6D
# C0 / C1 and which part each joint hangs in are Roblox's; rbx_runtime.cpp uses the same numbers.
extends RefCounted

const R6_LIMBS := [
	["Head", Vector3(2, 1, 1), Vector3(0, 1.5, 0)], ["Torso", Vector3(2, 2, 1), Vector3(0, 0, 0)],
	["Left Arm", Vector3(1, 2, 1), Vector3(-1.5, 0, 0)], ["Right Arm", Vector3(1, 2, 1), Vector3(1.5, 0, 0)],
	["Left Leg", Vector3(1, 2, 1), Vector3(-0.5, -2, 0)], ["Right Leg", Vector3(1, 2, 1), Vector3(0.5, -2, 0)],
]
const R15_LIMBS := [
	["Head", Vector3(2, 1, 1), Vector3(0, 2.3, 0)],
	["UpperTorso", Vector3(2, 1.6, 1), Vector3(0, 1, 0)], ["LowerTorso", Vector3(2, 0.4, 1), Vector3(0, 0, 0)],
	["LeftUpperArm", Vector3(1, 1.2, 1), Vector3(-1.5, 1, 0)], ["LeftLowerArm", Vector3(1, 1.2, 1), Vector3(-1.5, -0.2, 0)],
	["LeftHand", Vector3(1, 0.4, 1), Vector3(-1.5, -1, 0)],
	["RightUpperArm", Vector3(1, 1.2, 1), Vector3(1.5, 1, 0)], ["RightLowerArm", Vector3(1, 1.2, 1), Vector3(1.5, -0.2, 0)],
	["RightHand", Vector3(1, 0.4, 1), Vector3(1.5, -1, 0)],
	["LeftUpperLeg", Vector3(1, 1.4, 1), Vector3(-0.5, -0.9, 0)], ["LeftLowerLeg", Vector3(1, 1.4, 1), Vector3(-0.5, -2.3, 0)],
	["LeftFoot", Vector3(1, 0.2, 1), Vector3(-0.5, -3.1, 0)],
	["RightUpperLeg", Vector3(1, 1.4, 1), Vector3(0.5, -0.9, 0)], ["RightLowerLeg", Vector3(1, 1.4, 1), Vector3(0.5, -2.3, 0)],
	["RightFoot", Vector3(1, 0.2, 1), Vector3(0.5, -3.1, 0)],
]

# name, part the joint lives in, Part0, Part1, C0, C1 (Luau CFrame expressions)
# Column 2 is Roblox's own parenting, not a free choice: scripts reach character.Torso["Right Shoulder"]
const R6_JOINTS := [
	["RootJoint", "HumanoidRootPart", "HumanoidRootPart", "Torso",
	 "CFrame.new(0, 0, 0) * CFrame.Angles(-math.pi / 2, 0, math.pi)", "CFrame.new(0, 0, 0) * CFrame.Angles(-math.pi / 2, 0, math.pi)"],
	["Neck", "Torso", "Torso", "Head",
	 "CFrame.new(0, 1, 0) * CFrame.Angles(-math.pi / 2, 0, math.pi)", "CFrame.new(0, -0.5, 0) * CFrame.Angles(-math.pi / 2, 0, math.pi)"],
	["Left Shoulder", "Torso", "Torso", "Left Arm",
	 "CFrame.new(-1, 0.5, 0) * CFrame.Angles(0, -math.pi / 2, 0)", "CFrame.new(0.5, 0.5, 0) * CFrame.Angles(0, -math.pi / 2, 0)"],
	["Right Shoulder", "Torso", "Torso", "Right Arm",
	 "CFrame.new(1, 0.5, 0) * CFrame.Angles(0, math.pi / 2, 0)", "CFrame.new(-0.5, 0.5, 0) * CFrame.Angles(0, math.pi / 2, 0)"],
	["Left Hip", "Torso", "Torso", "Left Leg",
	 "CFrame.new(-1, -1, 0) * CFrame.Angles(0, -math.pi / 2, 0)", "CFrame.new(-0.5, 1, 0) * CFrame.Angles(0, -math.pi / 2, 0)"],
	["Right Hip", "Torso", "Torso", "Right Leg",
	 "CFrame.new(1, -1, 0) * CFrame.Angles(0, math.pi / 2, 0)", "CFrame.new(0.5, 1, 0) * CFrame.Angles(0, math.pi / 2, 0)"],
]
const R15_JOINTS := [
	["Root", "LowerTorso", "HumanoidRootPart", "LowerTorso", "CFrame.new(0, 0, 0)", "CFrame.new(0, 0, 0)"],
	["Waist", "UpperTorso", "LowerTorso", "UpperTorso", "CFrame.new(0, 0.2, 0)", "CFrame.new(0, -0.8, 0)"],
	["Neck", "Head", "UpperTorso", "Head", "CFrame.new(0, 0.8, 0)", "CFrame.new(0, -0.5, 0)"],
	["LeftShoulder", "LeftUpperArm", "UpperTorso", "LeftUpperArm", "CFrame.new(-1.5, 0.6, 0)", "CFrame.new(0, 0.6, 0)"],
	["LeftElbow", "LeftLowerArm", "LeftUpperArm", "LeftLowerArm", "CFrame.new(0, -0.6, 0)", "CFrame.new(0, 0.6, 0)"],
	["LeftWrist", "LeftHand", "LeftLowerArm", "LeftHand", "CFrame.new(0, -0.6, 0)", "CFrame.new(0, 0.2, 0)"],
	["RightShoulder", "RightUpperArm", "UpperTorso", "RightUpperArm", "CFrame.new(1.5, 0.6, 0)", "CFrame.new(0, 0.6, 0)"],
	["RightElbow", "RightLowerArm", "RightUpperArm", "RightLowerArm", "CFrame.new(0, -0.6, 0)", "CFrame.new(0, 0.6, 0)"],
	["RightWrist", "RightHand", "RightLowerArm", "RightHand", "CFrame.new(0, -0.6, 0)", "CFrame.new(0, 0.2, 0)"],
	["LeftHip", "LeftUpperLeg", "LowerTorso", "LeftUpperLeg", "CFrame.new(-0.5, -0.2, 0)", "CFrame.new(0, 0.7, 0)"],
	["LeftKnee", "LeftLowerLeg", "LeftUpperLeg", "LeftLowerLeg", "CFrame.new(0, -0.7, 0)", "CFrame.new(0, 0.7, 0)"],
	["LeftAnkle", "LeftFoot", "LeftLowerLeg", "LeftFoot", "CFrame.new(0, -0.7, 0)", "CFrame.new(0, 0.1, 0)"],
	["RightHip", "RightUpperLeg", "LowerTorso", "RightUpperLeg", "CFrame.new(0.5, -0.2, 0)", "CFrame.new(0, 0.7, 0)"],
	["RightKnee", "RightLowerLeg", "RightUpperLeg", "RightLowerLeg", "CFrame.new(0, -0.7, 0)", "CFrame.new(0, 0.7, 0)"],
	["RightAnkle", "RightFoot", "RightLowerLeg", "RightFoot", "CFrame.new(0, -0.7, 0)", "CFrame.new(0, 0.1, 0)"],
]

# limb, attachment name, position
const R6_POINTS := [
	["Head", "HatAttachment", Vector3(0, 1.24, 0)], ["Head", "HairAttachment", Vector3(0, 1.24, 0)],
	["Head", "FaceCenterAttachment", Vector3(0, 0, 0)], ["Head", "FaceFrontAttachment", Vector3(0, 0, -0.5)],
	["Head", "NeckAttachment", Vector3(0, -0.5, 0)],
	["Torso", "NeckAttachment", Vector3(0, 1, 0)], ["Torso", "BodyFrontAttachment", Vector3(0, 0, -0.5)],
	["Torso", "BodyBackAttachment", Vector3(0, 0, 0.5)], ["Torso", "LeftCollarAttachment", Vector3(-1, 1, 0)],
	["Torso", "RightCollarAttachment", Vector3(1, 1, 0)], ["Torso", "WaistCenterAttachment", Vector3(0, -1, 0)],
	["Torso", "WaistFrontAttachment", Vector3(0, -1, -0.5)], ["Torso", "WaistBackAttachment", Vector3(0, -1, 0.5)],
	["Left Arm", "LeftShoulderAttachment", Vector3(0, 1, 0)], ["Left Arm", "LeftGripAttachment", Vector3(0, -1, 0)],
	["Right Arm", "RightShoulderAttachment", Vector3(0, 1, 0)], ["Right Arm", "RightGripAttachment", Vector3(0, -1, 0)],
	["Left Leg", "LeftFootAttachment", Vector3(0, -1, 0)], ["Right Leg", "RightFootAttachment", Vector3(0, -1, 0)],
]
const R15_POINTS := [
	["Head", "HatAttachment", Vector3(0, 1.24, 0)], ["Head", "HairAttachment", Vector3(0, 1.24, 0)],
	["Head", "FaceCenterAttachment", Vector3(0, 0, 0)], ["Head", "FaceFrontAttachment", Vector3(0, 0, -0.5)],
	["Head", "NeckRigAttachment", Vector3(0, -0.5, 0)],
	["UpperTorso", "NeckRigAttachment", Vector3(0, 0.8, 0)], ["UpperTorso", "BodyFrontAttachment", Vector3(0, 0, -0.5)],
	["UpperTorso", "BodyBackAttachment", Vector3(0, 0, 0.5)], ["UpperTorso", "LeftCollarAttachment", Vector3(-1, 0.8, 0)],
	["UpperTorso", "RightCollarAttachment", Vector3(1, 0.8, 0)], ["UpperTorso", "WaistRigAttachment", Vector3(0, -0.8, 0)],
	["LowerTorso", "WaistRigAttachment", Vector3(0, 0.2, 0)], ["LowerTorso", "WaistCenterAttachment", Vector3(0, 0.2, 0)],
	["LowerTorso", "WaistFrontAttachment", Vector3(0, 0.2, -0.5)], ["LowerTorso", "WaistBackAttachment", Vector3(0, 0.2, 0.5)],
	["LeftUpperArm", "LeftShoulderAttachment", Vector3(0, 0.6, 0)], ["LeftHand", "LeftGripAttachment", Vector3(0, -0.2, 0)],
	["RightUpperArm", "RightShoulderAttachment", Vector3(0, 0.6, 0)], ["RightHand", "RightGripAttachment", Vector3(0, -0.2, 0)],
	["LeftFoot", "LeftFootAttachment", Vector3(0, -0.1, 0)], ["RightFoot", "RightFootAttachment", Vector3(0, -0.1, 0)],
]

static func _v(v: Vector3) -> String:
	return "Vector3.new(%s, %s, %s)" % [v.x, v.y, v.z]

# `at` is where the HumanoidRootPart goes. The Model is parented last, so the tree
# sees one finished rig rather than a part at a time.
static func chunk(r15: bool, at: Vector3) -> String:
	var limbs: Array = R15_LIMBS if r15 else R6_LIMBS
	var joints: Array = R15_JOINTS if r15 else R6_JOINTS
	var points: Array = R15_POINTS if r15 else R6_POINTS
	var lines: PackedStringArray = []
	lines.append("local at = %s" % _v(at))
	lines.append('local rig = Instance.new("Model"); rig.Name = "Rig"')
	lines.append("local P = {}")
	lines.append('local function limb(name, size, offset) local p = Instance.new("Part"); p.Name = name; p.Size = size; p.Position = at + offset; p.TopSurface = Enum.SurfaceType.Smooth; p.BottomSurface = Enum.SurfaceType.Smooth; p.Parent = rig; P[name] = p; return p end')
	lines.append('local root = limb("HumanoidRootPart", Vector3.new(2, 2, 1), Vector3.new(0, 0, 0)); root.Transparency = 1; root.CanCollide = false')
	for l in limbs:
		lines.append("limb(%s, %s, %s)" % [JSON.stringify(l[0]), _v(l[1]), _v(l[2])])
	# Roblox draws the head as a SpecialMesh of MeshType Head over the 2 x 1 x 1 box; here a hexagon
	# Scale makes that prism regular at the torso's 2-stud width; Offset lifts it onto the torso
	lines.append('do local m = Instance.new("SpecialMesh"); m.Name = "Mesh"; m.MeshType = Enum.MeshType.Head; m.Scale = Vector3.new(1, 2, 1); m.Offset = Vector3.new(0, 0.37, 0); m.Parent = P["Head"] end')
	for j in joints:
		lines.append('do local m = Instance.new("Motor6D"); m.Name = %s; m.Part0 = P[%s]; m.Part1 = P[%s]; m.C0 = %s; m.C1 = %s; m.Parent = P[%s] end'
			% [JSON.stringify(j[0]), JSON.stringify(j[2]), JSON.stringify(j[3]), j[4], j[5], JSON.stringify(j[1])])
	for pt in points:
		lines.append('do local a = Instance.new("Attachment"); a.Name = %s; a.Position = %s; a.Parent = P[%s] end'
			% [JSON.stringify(pt[1]), _v(pt[2]), JSON.stringify(pt[0])])
	# Humanoid.RigType is read-only to a script; the Studio sets it for R15 after this runs
	lines.append('local hum = Instance.new("Humanoid"); hum.Parent = rig')
	lines.append("rig.PrimaryPart = root")
	lines.append("rig.Parent = workspace")
	return "\n".join(lines)
