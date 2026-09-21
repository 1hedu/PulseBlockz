# The parts family: PVInstance and WorldRoot in the class chain, a part's PivotOffset and a
# model's WorldPivot, the BasePart queries, Bone, the wrap origins, the camera's other fields
# of view, the Terrain voxel API over Region3, the legacy Motor, and the recorded-only settings.
#
#   godot --headless --path . -s res://tests/parts_parity_test.gd
#
# A Play Solo world with one joined player (for the persistent-player list). What has behaviour
# is asserted on the behaviour; what is recorded only reads its default and takes a write.
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
	lines.append("PARTS %-60s %-10s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("ServerScriptService/Parts.server.luau", """
local Players = game:GetService("Players")
local function near(a, b, eps)
	eps = eps or 0.01
	if typeof(a) == "Vector3" then return (a - b).Magnitude < eps end
	if typeof(a) == "CFrame" then return (a.Position - b.Position).Magnitude < eps and (a.LookVector - b.LookVector).Magnitude < eps and (a.UpVector - b.UpVector).Magnitude < eps end
	return math.abs(a - b) < eps
end
local function part(name, pos, size)
	local p = Instance.new("Part")
	p.Name = name
	p.Anchored = true
	p.Size = size or Vector3.new(4, 4, 4)
	p.Position = pos
	p.Parent = workspace
	return p
end

-- the class chain
print("ISA " .. tostring(Instance.new("Part"):IsA("PVInstance")) .. " " .. tostring(Instance.new("Model"):IsA("PVInstance")) .. " " .. tostring(workspace:IsA("WorldRoot")) .. " " .. tostring(workspace:IsA("Model")) .. " " .. tostring(Instance.new("Folder"):IsA("PVInstance")))
print("NEWCLASSES " .. tostring(Instance.new("TrussPart").ClassName) .. " " .. tostring(Instance.new("Bone"):IsA("Attachment")) .. " " .. tostring(Instance.new("Motor"):IsA("JointInstance")) .. " " .. tostring(Instance.new("DragDetector"):IsA("ClickDetector")) .. " " .. tostring(Instance.new("WrapDeformer"):IsA("BaseWrap")) .. " " .. tostring(Instance.new("TorsionSpringConstraint"):IsA("Constraint")) .. " " .. tostring(Instance.new("UniversalConstraint"):IsA("Constraint")) .. " " .. tostring(Instance.new("AnimationConstraint"):IsA("Constraint")) .. " " .. tostring(Instance.new("VelocityMotor"):IsA("JointInstance")))

-- Region3
local r = Region3.new(Vector3.new(-3, 1, 9), Vector3.new(5, -1, 1))
print("REGION3 " .. typeof(r) .. " " .. tostring(near(r.CFrame.Position, Vector3.new(1, 0, 5))) .. " " .. tostring(r.Size) .. " " .. tostring(r:ExpandToGrid(4).Size) .. " " .. tostring(near(r:ExpandToGrid(4).CFrame.Position, Vector3.new(2, 0, 6))))

-- a part's PivotOffset
local a = part("A", Vector3.new(0, 10, 0))
print("PIVOTOFF0 " .. tostring(a.PivotOffset == CFrame.new()) .. " " .. tostring(a:GetPivot() == a.CFrame))
a.PivotOffset = CFrame.new(0, 2, 0)
print("PIVOTOFF1 " .. tostring(near(a:GetPivot().Position, Vector3.new(0, 12, 0))))
a:PivotTo(CFrame.new(10, 20, 30))
print("PIVOTOFF2 " .. tostring(near(a.Position, Vector3.new(10, 18, 30))) .. " " .. tostring(near(a:GetPivot().Position, Vector3.new(10, 20, 30))))
a.PivotOffset = CFrame.new()

-- a model's WorldPivot
local m = Instance.new("Model")
m.Name = "M"
m.Parent = workspace
local m1 = part("M1", Vector3.new(0, 30, 0)); m1.Parent = m
local m2 = part("M2", Vector3.new(8, 30, 0)); m2.Parent = m
print("WPIVOT0 " .. tostring(near(m.WorldPivot.Position, Vector3.new(0, 30, 0))) .. " " .. tostring(m:GetPivot() == m.WorldPivot))
m.WorldPivot = CFrame.new(4, 30, 0)   -- no PrimaryPart: held on the model
print("WPIVOT1 " .. tostring(near(m:GetPivot().Position, Vector3.new(4, 30, 0))) .. " " .. tostring(near(m1.Position, Vector3.new(0, 30, 0))))
m:PivotTo(CFrame.new(4, 40, 0))
print("WPIVOT2 " .. tostring(near(m1.Position, Vector3.new(0, 40, 0))) .. " " .. tostring(near(m2.Position, Vector3.new(8, 40, 0))) .. " " .. tostring(near(m:GetPivot().Position, Vector3.new(4, 40, 0))))
m.PrimaryPart = m2
m.WorldPivot = CFrame.new(9, 40, 0)   -- with one: the primary part's PivotOffset
print("WPIVOT3 " .. tostring(near(m2.PivotOffset.Position, Vector3.new(1, 0, 0))) .. " " .. tostring(near(m:GetPivot().Position, Vector3.new(9, 40, 0))))
m:TranslateBy(Vector3.new(0, 0, 5))
print("TRANSLATE " .. tostring(near(m1.Position, Vector3.new(0, 40, 5))) .. " " .. tostring(near(m2.Position, Vector3.new(8, 40, 5))))
print("EXTENTS " .. tostring(m:GetExtentsSize()))
print("MODELREC " .. tostring(m.LevelOfDetail) .. " " .. tostring(m.ModelStreamingMode))
m.ModelStreamingMode = Enum.ModelStreamingMode.Persistent
print("MODELRECW " .. tostring(m.ModelStreamingMode) .. " " .. tostring(pcall(function() m.LevelOfDetail = Enum.ModelLevelOfDetail.Disabled end)))
local pl = Players:GetPlayers()[1] or Players.PlayerAdded:Wait()
m:AddPersistentPlayer(pl)
m:AddPersistentPlayer(pl)
print("PERSIST " .. #m:GetPersistentPlayers() .. " " .. tostring(m:GetPersistentPlayers()[1] == pl))
m:RemovePersistentPlayer(pl)
print("PERSIST2 " .. #m:GetPersistentPlayers())

-- BasePart: the physics figures and the queries
local b = part("B", Vector3.new(50, 10, 0), Vector3.new(4, 2, 6))
b.Material = Enum.Material.Metal
local cpp = b.CurrentPhysicalProperties
print("CURPHYS " .. typeof(cpp) .. " " .. tostring(near(cpp.Density, 7.85)) .. " " .. tostring(b.ExtentsCFrame == b.CFrame) .. " " .. tostring(pcall(function() b.ExtentsCFrame = CFrame.new() end)))
print("BPREC " .. tostring(b.EnableFluidForces) .. " " .. tostring(b.LocalTransparencyModifier) .. " " .. tostring(b.ReceiveAge) .. " " .. tostring(b.ResizeIncrement))
b.EnableFluidForces = false
b.LocalTransparencyModifier = 0.5
print("BPRECW " .. tostring(b.EnableFluidForces) .. " " .. tostring(b.LocalTransparencyModifier) .. " " .. tostring(pcall(function() b.ReceiveAge = 1 end)))
b.LocalTransparencyModifier = 0
print("CLOSEST " .. tostring(b:GetClosestPointOnSurface(Vector3.new(60, 10, 0))) .. " " .. tostring(b:GetClosestPointOnSurface(Vector3.new(50, 10.5, 0))))
b.Anchored = false
b.AssemblyLinearVelocity = Vector3.new(1, 0, 0)
b.AssemblyAngularVelocity = Vector3.new(0, 1, 0)
print("VELAT " .. tostring(b:GetVelocityAtPosition(b.Position + Vector3.new(0, 0, 2))))
b.AssemblyLinearVelocity = Vector3.new(0, 0, 0)
b.AssemblyAngularVelocity = Vector3.new(0, 0, 0)
b.Anchored = true
local c = part("C", Vector3.new(50, 13, 0), Vector3.new(4, 4, 4))
c.Anchored = false
local w = Instance.new("WeldConstraint"); w.Part0 = b; w.Part1 = c; w.Parent = b
local ncc = Instance.new("NoCollisionConstraint"); ncc.Part0 = b; ncc.Part1 = c; ncc.Parent = b
local att = Instance.new("Attachment"); att.Parent = b
local att2 = Instance.new("Attachment"); att2.Parent = c
local hinge = Instance.new("HingeConstraint"); hinge.Attachment0 = att; hinge.Attachment1 = att2; hinge.Parent = b
task.wait(0.2)
local joints = b:GetJoints()
print("JOINTS " .. #joints .. " " .. #c:GetJoints() .. " " .. #b:GetNoCollisionConstraints() .. " " .. tostring(b:GetNoCollisionConstraints()[1] == ncc))
print("CONSTRAINTS " .. #att:GetConstraints() .. " " .. tostring(att:GetConstraints()[1] == hinge) .. " " .. #Instance.new("Attachment"):GetConstraints())
print("CANCOLLIDE " .. tostring(b:CanCollideWith(c)) .. " " .. tostring(b:CanCollideWith(a)))
print("GROUNDED " .. tostring(c:IsGrounded()) .. " " .. tostring(a:IsGrounded()))
local free = part("Free", Vector3.new(50, 30, 30)); free.Anchored = false
print("GROUNDED2 " .. tostring(free:IsGrounded()))
print("NETOWN " .. tostring(c:GetNetworkOwner()) .. " " .. tostring(c:GetNetworkOwnershipAuto()) .. " " .. tostring(c:CanSetNetworkOwnership()) .. " " .. tostring(b:CanSetNetworkOwnership()))
c:SetNetworkOwnershipAuto()
-- a lone 2x4x6 Plastic box: I = m/12 * (b^2 + c^2) about each axis, m = 0.7 * 48
local lone = part("Lone", Vector3.new(80, 10, 0), Vector3.new(2, 4, 6)); lone.Anchored = false
local torque = lone:AngularAccelerationToTorque(Vector3.new(1, 0, 0))
local wantX = 0.7 * 48 / 12 * (16 + 36)
local wantY = 0.7 * 48 / 12 * (4 + 36)
print("TORQUE " .. tostring(near(torque, Vector3.new(wantX, 0, 0), 0.05)) .. " " .. tostring(near(lone:TorqueToAngularAcceleration(torque), Vector3.new(1, 0, 0), 0.001)) .. " " .. tostring(near(lone:TorqueToAngularAcceleration(Vector3.new(0, wantY, 0), Enum.ActuatorRelativeTo.World), Vector3.new(0, 1, 0), 0.001)))
local rz = part("Rz", Vector3.new(90, 10, 0), Vector3.new(4, 2, 2))
print("RESIZE " .. tostring(rz:Resize(Enum.NormalId.Right, 2)) .. " " .. tostring(rz.Size) .. " " .. tostring(rz.Position) .. " " .. tostring(rz:Resize(Enum.NormalId.Bottom, -1)) .. " " .. tostring(rz.Size) .. " " .. tostring(rz.Position) .. " " .. tostring(rz:Resize(Enum.NormalId.Top, -5)) .. " " .. tostring(rz.Size))

-- Bone
local bone = Instance.new("Bone")
bone.Position = Vector3.new(0, 1, 0)
bone.Parent = b
bone.Transform = CFrame.new(0, 0, 3)
local child = Instance.new("Bone"); child.Position = Vector3.new(1, 0, 0); child.Parent = bone
print("BONE " .. tostring(near(bone.TransformedCFrame.Position, Vector3.new(0, 1, 3))) .. " " .. tostring(near(bone.TransformedWorldCFrame.Position, Vector3.new(50, 11, 3))) .. " " .. tostring(near(child.TransformedWorldCFrame.Position, Vector3.new(51, 11, 3))) .. " " .. tostring(near(bone.WorldPosition, Vector3.new(50, 11, 0))) .. " " .. tostring(pcall(function() bone.TransformedCFrame = CFrame.new() end)))

-- the wraps: the origins are a plugin's to write, and their World twins follow the part
local mp = Instance.new("MeshPart"); mp.Position = Vector3.new(0, 50, 0); mp.Anchored = true; mp.Parent = workspace
local wl = Instance.new("WrapLayer"); wl.Parent = mp
local wt = Instance.new("WrapTarget"); wt.Parent = mp
print("WRAP " .. tostring(wl.CageOrigin == CFrame.new()) .. " " .. tostring(near(wl.CageOriginWorld.Position, Vector3.new(0, 50, 0))) .. " " .. tostring(near(wt.ImportOriginWorld.Position, Vector3.new(0, 50, 0))) .. " " .. tostring(near(wl.ReferenceOriginWorld.Position, Vector3.new(0, 50, 0))) .. " " .. tostring(wl.BindOffset == CFrame.new()) .. " " .. tostring(wl.AutoSkin))
print("WRAPSEC " .. tostring(pcall(function() wl.CageOrigin = CFrame.new(1, 0, 0) end)) .. " " .. tostring(pcall(function() wl.ReferenceOriginWorld = CFrame.new() end)))
wl.AutoSkin = Enum.WrapLayerAutoSkin.EnabledPreserve
print("WRAPW " .. tostring(wl.AutoSkin))

-- Camera: the other fields of view follow FieldOfView on the viewport's aspect, and back
local cam = Instance.new("Camera")   -- the server has no CurrentCamera; a fresh one has the default viewport
cam.Parent = workspace
cam.FieldOfView = 70
local aspect = cam.ViewportSize.X / cam.ViewportSize.Y
local diag = math.deg(2 * math.atan(math.tan(math.rad(35)) * math.sqrt(1 + aspect * aspect)))
local maxAxis = math.deg(2 * math.atan(math.tan(math.rad(35)) * math.max(1, aspect)))
print("FOV " .. tostring(near(cam.DiagonalFieldOfView, diag)) .. " " .. tostring(near(cam.MaxAxisFieldOfView, maxAxis)) .. " " .. tostring(cam.FieldOfViewMode))
cam.DiagonalFieldOfView = diag
print("FOVW1 " .. tostring(near(cam.FieldOfView, 70)))
cam.MaxAxisFieldOfView = 90
print("FOVW2 " .. tostring(near(cam.MaxAxisFieldOfView, 90)) .. " " .. tostring(cam.FieldOfView <= 90))
cam.FieldOfView = 70
print("CAMREC " .. tostring(cam.Focus == CFrame.new()) .. " " .. tostring(cam.HeadLocked) .. " " .. tostring(cam.HeadScale) .. " " .. tostring(cam.VRTiltAndRollEnabled) .. " " .. tostring(cam.NearPlaneZ))
cam.Focus = CFrame.new(1, 2, 3); cam.HeadLocked = false; cam.HeadScale = 2; cam.VRTiltAndRollEnabled = true; cam.FieldOfViewMode = Enum.FieldOfViewMode.Diagonal
print("CAMRECW " .. tostring(cam.Focus.Position) .. " " .. tostring(cam.HeadLocked) .. " " .. tostring(cam.HeadScale) .. " " .. tostring(cam.VRTiltAndRollEnabled) .. " " .. tostring(cam.FieldOfViewMode) .. " " .. tostring(pcall(function() cam.NearPlaneZ = 1 end)))
cam.CameraType = Enum.CameraType.Scriptable
cam.CFrame = CFrame.lookAt(Vector3.new(200, 10, 0), Vector3.new(230, 10, 0))
local wall1 = part("Wall1", Vector3.new(210, 10, 0), Vector3.new(1, 10, 10))
local wall2 = part("Wall2", Vector3.new(220, 10, 0), Vector3.new(1, 10, 10))
task.wait(0.1)
local obscuring = cam:GetPartsObscuringTarget({Vector3.new(230, 10, 0)}, {})
local obscuring2 = cam:GetPartsObscuringTarget({Vector3.new(230, 10, 0)}, {wall1})
print("OBSCURE " .. #obscuring .. " " .. tostring(obscuring[1] == wall1) .. " " .. tostring(obscuring[2] == wall2) .. " " .. #obscuring2 .. " " .. tostring(obscuring2[1] == wall2) .. " " .. #cam:GetPartsObscuringTarget({Vector3.new(205, 10, 0)}, {}))

-- CylindricalConstraint.WorldRotationAxis is Attachment0's WorldAxis
local cyl = Instance.new("CylindricalConstraint")
local ca = Instance.new("Attachment"); ca.Parent = rz; ca.Orientation = Vector3.new(0, 90, 0)
cyl.Attachment0 = ca; cyl.Parent = rz
print("CYLAXIS " .. tostring(near(cyl.WorldRotationAxis, ca.WorldAxis)) .. " " .. tostring(near(cyl.WorldRotationAxis, Vector3.new(0, 0, -1))) .. " " .. tostring(near(Instance.new("CylindricalConstraint").WorldRotationAxis, Vector3.new(1, 0, 0))) .. " " .. tostring(cyl.AngularResponsiveness))
cyl.AngularResponsiveness = 20
print("CONSREC " .. tostring(cyl.AngularResponsiveness) .. " " .. tostring(hinge.AngularResponsiveness) .. " " .. tostring(Instance.new("PrismaticConstraint").LinearResponsiveness))
hinge.AngularResponsiveness = 30
print("CONSRECW " .. tostring(hinge.AngularResponsiveness))
local lv = Instance.new("LinearVelocity")
print("LINVEL " .. tostring(lv.ForceLimitsEnabled) .. " " .. tostring(lv.MaxPlanarAxesForce) .. " " .. tostring(lv.ReactionForceEnabled))
lv.ForceLimitsEnabled = true; lv.MaxPlanarAxesForce = Vector2.new(1, 2); lv.ReactionForceEnabled = true
print("LINVELW " .. tostring(lv.ForceLimitsEnabled) .. " " .. tostring(lv.MaxPlanarAxesForce) .. " " .. tostring(lv.ReactionForceEnabled))
local ao = Instance.new("AlignOrientation")
print("ALIGNO " .. tostring(ao.AlignType) .. " " .. tostring(ao.LookAtPosition) .. " " .. tostring(ao.PrimaryAxis) .. " " .. tostring(ao.SecondaryAxis) .. " " .. tostring(ao.CFrame == CFrame.new()))
ao.AlignType = Enum.AlignType.PrimaryAxisLookAt; ao.LookAtPosition = Vector3.new(1, 2, 3); ao.PrimaryAxis = Vector3.new(0, 1, 0); ao.SecondaryAxis = Vector3.new(0, 0, 1)
print("ALIGNOW " .. tostring(ao.AlignType) .. " " .. tostring(ao.LookAtPosition) .. " " .. tostring(ao.PrimaryAxis) .. " " .. tostring(ao.SecondaryAxis))
local ts = Instance.new("TorsionSpringConstraint")
print("TORSION " .. tostring(ts.Coils) .. " " .. tostring(ts.CurrentAngle) .. " " .. tostring(ts.Damping) .. " " .. tostring(ts.LimitsEnabled) .. " " .. tostring(ts.MaxAngle) .. " " .. tostring(ts.MaxTorque) .. " " .. tostring(ts.Radius) .. " " .. tostring(ts.Restitution) .. " " .. tostring(ts.Stiffness))
ts.Coils = 5; ts.Damping = 1; ts.LimitsEnabled = true; ts.MaxAngle = 90; ts.MaxTorque = 10; ts.Radius = 1; ts.Restitution = 0.5; ts.Stiffness = 50
print("TORSIONW " .. tostring(ts.Coils) .. " " .. tostring(ts.Damping) .. " " .. tostring(ts.LimitsEnabled) .. " " .. tostring(ts.MaxAngle) .. " " .. tostring(ts.MaxTorque) .. " " .. tostring(ts.Radius) .. " " .. tostring(ts.Restitution) .. " " .. tostring(ts.Stiffness) .. " " .. tostring(pcall(function() ts.CurrentAngle = 1 end)))
local uc = Instance.new("UniversalConstraint")
print("UNIVERSAL " .. tostring(uc.LimitsEnabled) .. " " .. tostring(uc.MaxAngle) .. " " .. tostring(uc.Radius) .. " " .. tostring(uc.Restitution))
uc.LimitsEnabled = true; uc.MaxAngle = 10; uc.Radius = 1; uc.Restitution = 1
print("UNIVERSALW " .. tostring(uc.LimitsEnabled) .. " " .. tostring(uc.MaxAngle) .. " " .. tostring(uc.Radius) .. " " .. tostring(uc.Restitution))
local ac = Instance.new("AnimationConstraint")
print("ANIMC " .. tostring(ac.AngularDamping) .. " " .. tostring(ac.AngularStrength) .. " " .. tostring(ac.IsKinematic) .. " " .. tostring(ac.LinearDamping) .. " " .. tostring(ac.LinearStrength) .. " " .. tostring(ac.MaxForce) .. " " .. tostring(ac.MaxTorque) .. " " .. tostring(ac.Transform == CFrame.new()))
ac.AngularDamping = 1; ac.AngularStrength = 2; ac.IsKinematic = true; ac.LinearDamping = 3; ac.LinearStrength = 4; ac.MaxForce = 5; ac.MaxTorque = 6; ac.Transform = CFrame.new(1, 0, 0)
print("ANIMCW " .. tostring(ac.AngularDamping) .. " " .. tostring(ac.AngularStrength) .. " " .. tostring(ac.IsKinematic) .. " " .. tostring(ac.LinearDamping) .. " " .. tostring(ac.LinearStrength) .. " " .. tostring(ac.MaxForce) .. " " .. tostring(ac.MaxTorque) .. " " .. tostring(ac.Transform.Position))

-- the legacy Motor turns like a Motor6D, and SetDesiredAngle sets the goal
local mA = part("MA", Vector3.new(120, 10, 0)); local mB = part("MB", Vector3.new(120, 14, 0)); mB.Anchored = false
local motor = Instance.new("Motor"); motor.Part0 = mA; motor.Part1 = mB; motor.MaxVelocity = 0.5; motor.Parent = mA
print("MOTOR0 " .. tostring(motor.CurrentAngle) .. " " .. tostring(motor.DesiredAngle) .. " " .. tostring(motor.MaxVelocity))
motor:SetDesiredAngle(1)
task.wait(0.5)
print("MOTOR1 " .. tostring(motor.DesiredAngle) .. " " .. tostring(motor.CurrentAngle))
local m6 = Instance.new("Motor6D"); m6.Part0 = mA; m6.Parent = mA
m6:SetDesiredAngle(2)
print("MOTOR6D " .. tostring(m6.DesiredAngle))
local vm = Instance.new("VelocityMotor")
print("VMOTOR " .. tostring(vm.CurrentAngle) .. " " .. tostring(vm.DesiredAngle) .. " " .. tostring(vm.MaxVelocity) .. " " .. tostring(vm.Hole))
vm.CurrentAngle = 1; vm.DesiredAngle = 2; vm.MaxVelocity = 3; vm.Hole = mA
print("VMOTORW " .. tostring(vm.CurrentAngle) .. " " .. tostring(vm.DesiredAngle) .. " " .. tostring(vm.MaxVelocity) .. " " .. tostring(vm.Hole == mA))

-- Workspace: Terrain, the bulk move, touching, the physics figures, and the recorded switches
print("WSTERRAIN " .. tostring(workspace.Terrain ~= nil and workspace.Terrain.ClassName) .. " " .. tostring(pcall(function() workspace.Terrain = nil end)))
local b1 = part("B1", Vector3.new(0, 100, 0)); local b2 = part("B2", Vector3.new(0, 110, 0))
workspace:BulkMoveTo({b1, b2}, {CFrame.new(1, 100, 0), CFrame.new(2, 110, 0)}, Enum.BulkMoveMode.FireCFrameChanged)
print("BULKMOVE " .. tostring(b1.Position) .. " " .. tostring(b2.Position) .. " " .. tostring(pcall(function() workspace:BulkMoveTo({b1}, {}) end)))
local t1 = part("T1", Vector3.new(0, 200, 0)); local t2 = part("T2", Vector3.new(3, 200, 0)); local t3 = part("T3", Vector3.new(50, 200, 0))
task.wait(0.1)
print("TOUCHING " .. tostring(workspace:ArePartsTouchingOthers({t1})) .. " " .. tostring(workspace:ArePartsTouchingOthers({t1, t2})) .. " " .. tostring(workspace:ArePartsTouchingOthers({t3})))
print("PHYSFIG " .. tostring(workspace:GetNumAwakeParts() >= 4) .. " " .. tostring(workspace:GetPhysicsThrottling()) .. " " .. tostring(workspace:GetRealPhysicsFPS()) .. " " .. tostring(workspace:PGSIsEnabled()))
workspace:JoinToOutsiders({t1})
workspace:UnjoinFromOutsiders({t1})
print("WSREC " .. tostring(workspace.AirDensity) .. " " .. tostring(workspace.AirTurbulenceIntensity) .. " " .. tostring(workspace.FluidForces) .. " " .. tostring(workspace.GlobalWind) .. " " .. tostring(workspace.AllowThirdPartySales) .. " " .. tostring(workspace.InsertPoint) .. " " .. tostring(workspace.StreamingMinRadius) .. " " .. tostring(workspace.StreamingTargetRadius) .. " " .. tostring(workspace.TouchesUseCollisionGroups))
print("WSENUMS " .. tostring(workspace.StreamingIntegrityMode) .. " " .. tostring(workspace.StreamOutBehavior) .. " " .. tostring(workspace.ModelStreamingBehavior) .. " " .. tostring(workspace.PhysicsSteppingMethod) .. " " .. tostring(workspace.SignalBehavior) .. " " .. tostring(workspace.AvatarUnificationMode) .. " " .. tostring(workspace.ClientAnimatorThrottling) .. " " .. tostring(workspace.MeshPartHeadsAndAccessories) .. " " .. tostring(workspace.RejectCharacterDeletions) .. " " .. tostring(workspace.ReplicateInstanceDestroySetting) .. " " .. tostring(workspace.SandboxedInstanceMode) .. " " .. tostring(workspace.PlayerCharacterDestroyBehavior) .. " " .. tostring(workspace.IKControlConstraintSupport) .. " " .. tostring(workspace.PrimalPhysicsSolver) .. " " .. tostring(workspace.RenderingCacheOptimizations) .. " " .. tostring(workspace.PathfindingUseImprovedSearch) .. " " .. tostring(workspace.Retargeting))
workspace.AirDensity = 2; workspace.AirTurbulenceIntensity = 1; workspace.FluidForces = Enum.FluidForces.Experimental; workspace.GlobalWind = Vector3.new(1, 0, 0); workspace.AllowThirdPartySales = true
workspace.InsertPoint = Vector3.new(0, 5, 0); workspace.StreamingMinRadius = 128; workspace.StreamingTargetRadius = 2048; workspace.TouchesUseCollisionGroups = true
workspace.StreamingIntegrityMode = Enum.StreamingIntegrityMode.PauseOutsideLoadedArea; workspace.StreamOutBehavior = Enum.StreamOutBehavior.LowMemory; workspace.ModelStreamingBehavior = Enum.ModelStreamingBehavior.Improved
workspace.PhysicsSteppingMethod = Enum.PhysicsSteppingMethod.Fixed; workspace.SignalBehavior = Enum.SignalBehavior.Deferred; workspace.AvatarUnificationMode = Enum.AvatarUnificationMode.Enabled
workspace.ClientAnimatorThrottling = Enum.ClientAnimatorThrottlingMode.Disabled; workspace.MeshPartHeadsAndAccessories = Enum.MeshPartHeadsAndAccessories.Enabled; workspace.RejectCharacterDeletions = Enum.RejectCharacterDeletions.Enabled
workspace.ReplicateInstanceDestroySetting = Enum.ReplicateInstanceDestroySetting.Enabled; workspace.SandboxedInstanceMode = Enum.SandboxedInstanceMode.Experimental; workspace.PlayerCharacterDestroyBehavior = Enum.PlayerCharacterDestroyBehavior.Enabled
workspace.IKControlConstraintSupport = Enum.IKControlConstraintSupport.Enabled; workspace.PrimalPhysicsSolver = Enum.PrimalPhysicsSolver.Experimental; workspace.RenderingCacheOptimizations = Enum.RenderingCacheOptimizationMode.Enabled
workspace.PathfindingUseImprovedSearch = Enum.PathfindingUseImprovedSearch.Enabled; workspace.Retargeting = Enum.AnimatorRetargetingMode.Enabled
print("WSRECW " .. tostring(workspace.AirDensity) .. " " .. tostring(workspace.FluidForces) .. " " .. tostring(workspace.GlobalWind) .. " " .. tostring(workspace.StreamingTargetRadius) .. " " .. tostring(workspace.SignalBehavior) .. " " .. tostring(workspace.PrimalPhysicsSolver) .. " " .. tostring(workspace.Retargeting) .. " " .. tostring(workspace.InsertPoint))
print("WSSEC " .. tostring(pcall(function() workspace.FallHeightEnabled = true end)) .. " " .. tostring(workspace.FallHeightEnabled) .. " " .. tostring(typeof(workspace.PersistentLoaded)))

-- Terrain: the cell arithmetic, the fills and the voxels
local terrain = workspace.Terrain
print("CELLS " .. tostring(terrain:CellCenterToWorld(1, 2, 3)) .. " " .. tostring(terrain:CellCornerToWorld(1, 2, 3)) .. " " .. tostring(terrain:WorldToCell(Vector3.new(5, -1, 8))) .. " " .. tostring(terrain:WorldToCellPreferEmpty(Vector3.new(5, -1, 8))) .. " " .. tostring(terrain:WorldToCellPreferSolid(Vector3.new(5, -1, 8))))
print("TERRREC " .. tostring(terrain.Decoration) .. " " .. tostring(terrain.GrassLength))
terrain.Decoration = true; terrain.GrassLength = 0.5
print("TERRRECW " .. tostring(terrain.Decoration) .. " " .. tostring(terrain.GrassLength))
local cells0 = terrain:CountCells()
terrain:FillRegion(Region3.new(Vector3.new(0, 0, 0), Vector3.new(16, 8, 16)), 4, Enum.Material.Grass)
task.wait(0.5)
local mats, occ = terrain:ReadVoxels(Region3.new(Vector3.new(0, 0, 0), Vector3.new(16, 8, 16)), 4)
print("FILLREGION " .. tostring(terrain:CountCells() > cells0) .. " " .. tostring(mats.Size) .. " " .. tostring(occ.Size) .. " " .. tostring(mats[1][1][1]) .. " " .. tostring(mats[4][2][4]) .. " " .. tostring(occ[2][2][2]) .. " " .. tostring(pcall(function() terrain:ReadVoxels(Region3.new(Vector3.new(), Vector3.new(4, 4, 4)), 2) end)))
local farMats = terrain:ReadVoxels(Region3.new(Vector3.new(100, 100, 100), Vector3.new(104, 104, 104)), 4)
print("READAIR " .. tostring(farMats[1][1][1]) .. " " .. tostring(farMats.Size))
terrain:ReplaceMaterial(Region3.new(Vector3.new(0, 0, 0), Vector3.new(16, 8, 16)), 4, Enum.Material.Grass, Enum.Material.Sand)
task.wait(0.5)
local mats2 = terrain:ReadVoxels(Region3.new(Vector3.new(0, 0, 0), Vector3.new(16, 8, 16)), 4)
print("REPLACE " .. tostring(mats2[1][1][1]) .. " " .. tostring(mats2[4][2][4]))
terrain:FillCylinder(CFrame.new(100, 20, 100), 8, 6, Enum.Material.Rock)
terrain:FillWedge(CFrame.new(100, 40, 100), Vector3.new(16, 16, 16), Enum.Material.Snow)
task.wait(0.5)
local cylM, cylO = terrain:ReadVoxels(Region3.new(Vector3.new(100, 20, 100), Vector3.new(104, 24, 104)), 4)
local farCyl = terrain:ReadVoxels(Region3.new(Vector3.new(108, 20, 108), Vector3.new(112, 24, 112)), 4)
local wedgeBack, wedgeBackO = terrain:ReadVoxels(Region3.new(Vector3.new(100, 36, 104), Vector3.new(104, 40, 108)), 4)
local wedgeFront, wedgeFrontO = terrain:ReadVoxels(Region3.new(Vector3.new(100, 36, 96), Vector3.new(104, 40, 100)), 4)
local wedgeAbove = terrain:ReadVoxels(Region3.new(Vector3.new(100, 44, 96), Vector3.new(104, 48, 100)), 4)
print("CYLWEDGE " .. tostring(cylM[1][1][1]) .. " " .. tostring(cylO[1][1][1] > 0.9) .. " " .. tostring(farCyl[1][1][1]) .. " " .. tostring(wedgeBack[1][1][1]) .. " " .. tostring(wedgeBackO[1][1][1] > 0.9) .. " " .. tostring(wedgeFrontO[1][1][1] < wedgeBackO[1][1][1]) .. " " .. tostring(wedgeAbove[1][1][1]))
local wm = {{{Enum.Material.Mud, Enum.Material.Air}}}
local wo = {{{1, 0}}}
wm.Size = Vector3.new(1, 1, 2); wo.Size = Vector3.new(1, 1, 2)
terrain:WriteVoxels(Region3.new(Vector3.new(200, 0, 200), Vector3.new(204, 4, 208)), 4, wm, wo)
task.wait(0.5)
local rm, ro = terrain:ReadVoxels(Region3.new(Vector3.new(200, 0, 200), Vector3.new(204, 4, 208)), 4)
print("WRITEVOX " .. tostring(rm[1][1][1]) .. " " .. tostring(ro[1][1][1]) .. " " .. tostring(rm[1][1][2]) .. " " .. tostring(ro[1][1][2]))
local ch = terrain:ReadVoxelChannels(Region3.new(Vector3.new(200, 0, 200), Vector3.new(204, 4, 208)), 4, {"SolidMaterial", "SolidOccupancy", "LiquidOccupancy"})
print("CHANNELS " .. tostring(ch.SolidMaterial[1][1][1]) .. " " .. tostring(ch.SolidOccupancy[1][1][1]) .. " " .. tostring(ch.LiquidOccupancy[1][1][1]) .. " " .. tostring(ch.SolidMaterial.Size))
terrain:WriteVoxelChannels(Region3.new(Vector3.new(200, 0, 200), Vector3.new(204, 4, 204)), 4, {LiquidOccupancy = {{{1}}}})
task.wait(0.5)
local ch2 = terrain:ReadVoxelChannels(Region3.new(Vector3.new(200, 0, 200), Vector3.new(204, 4, 204)), 4, {"SolidOccupancy", "LiquidOccupancy"})
print("CHANNELSW " .. tostring(ch2.LiquidOccupancy[1][1][1]) .. " " .. tostring(ch2.SolidOccupancy[1][1][1]))
local grass0 = terrain:GetMaterialColor(Enum.Material.Grass)
terrain:SetMaterialColor(Enum.Material.Grass, Color3.fromRGB(10, 20, 30))
local grass1 = terrain:GetMaterialColor(Enum.Material.Grass)
print("MATCOLOR " .. typeof(grass0) .. " " .. tostring(grass0 ~= grass1) .. " " .. tostring(grass1 == Color3.fromRGB(10, 20, 30)) .. " " .. tostring(terrain:GetMaterialColor(Enum.Material.Slate) ~= grass1) .. " " .. tostring(#terrain.MaterialColors > 0) .. " " .. tostring(pcall(function() terrain:GetMaterialColor(Enum.Material.Water) end)))

-- the recorded-only rest: ProximityPrompt, DragDetector, TrussPart, MeshPart, VehicleSeat, Explosion, AssetService
local pp = Instance.new("ProximityPrompt")
print("PROMPT " .. tostring(pp.AutoLocalize) .. " " .. tostring(pp.GamepadKeyCode) .. " " .. tostring(pp.RootLocalizationTable) .. " " .. typeof(pp.IndicatorShown) .. " " .. typeof(pp.IndicatorHidden))
pp.AutoLocalize = false; pp.GamepadKeyCode = Enum.KeyCode.ButtonA; pp.RootLocalizationTable = Instance.new("LocalizationTable")
print("PROMPTW " .. tostring(pp.AutoLocalize) .. " " .. tostring(pp.GamepadKeyCode) .. " " .. tostring(pp.RootLocalizationTable ~= nil) .. " " .. tostring(Enum.KeyCode.ButtonX.Value) .. " " .. tostring(Enum.KeyCode.DPadDown.Value))
local dd = Instance.new("DragDetector")
print("DRAG " .. tostring(dd.ActivatedCursorIcon) .. " " .. tostring(dd.ApplyAtCenterOfMass) .. " " .. tostring(dd.Axis) .. " " .. tostring(dd.DragFrame == CFrame.new()) .. " " .. tostring(dd.DragStyle) .. " " .. tostring(dd.GamepadModeSwitchKeyCode) .. " " .. tostring(dd.KeyboardModeSwitchKeyCode) .. " " .. tostring(dd.VRSwitchKeyCode) .. " " .. tostring(dd.MaxDragAngle) .. " " .. tostring(dd.MaxDragTranslation) .. " " .. tostring(dd.MaxForce) .. " " .. tostring(dd.MaxTorque) .. " " .. tostring(dd.MinDragAngle) .. " " .. tostring(dd.MinDragTranslation) .. " " .. tostring(dd.PermissionPolicy) .. " " .. tostring(dd.ReferenceInstance) .. " " .. tostring(dd.ResponseStyle) .. " " .. tostring(dd.Responsiveness) .. " " .. tostring(dd.RunLocally) .. " " .. tostring(dd.SecondaryAxis) .. " " .. tostring(dd.TrackballRadialPullFactor) .. " " .. tostring(dd.TrackballRollFactor) .. " " .. tostring(dd.MaxActivationDistance))
dd.ActivatedCursorIcon = "x"; dd.ApplyAtCenterOfMass = true; dd.Axis = Vector3.new(0, 1, 0); dd.DragFrame = CFrame.new(1, 0, 0); dd.DragStyle = Enum.DragDetectorDragStyle.RotateAxis
dd.GamepadModeSwitchKeyCode = Enum.KeyCode.ButtonL3; dd.KeyboardModeSwitchKeyCode = Enum.KeyCode.LeftShift; dd.VRSwitchKeyCode = Enum.KeyCode.ButtonL3; dd.MaxDragAngle = 90; dd.MaxDragTranslation = Vector3.new(1, 1, 1)
dd.MaxForce = 1; dd.MaxTorque = 2; dd.MinDragAngle = -90; dd.MinDragTranslation = Vector3.new(-1, -1, -1); dd.PermissionPolicy = Enum.DragDetectorPermissionPolicy.Nobody; dd.ReferenceInstance = a
dd.ResponseStyle = Enum.DragDetectorResponseStyle.Physical; dd.Responsiveness = 5; dd.RunLocally = true; dd.SecondaryAxis = Vector3.new(0, 0, 1); dd.TrackballRadialPullFactor = 2; dd.TrackballRollFactor = 3
print("DRAGW " .. tostring(dd.ActivatedCursorIcon) .. " " .. tostring(dd.ApplyAtCenterOfMass) .. " " .. tostring(dd.Axis) .. " " .. tostring(dd.DragFrame.Position) .. " " .. tostring(dd.DragStyle) .. " " .. tostring(dd.GamepadModeSwitchKeyCode) .. " " .. tostring(dd.KeyboardModeSwitchKeyCode) .. " " .. tostring(dd.MaxDragAngle) .. " " .. tostring(dd.MaxForce) .. " " .. tostring(dd.PermissionPolicy) .. " " .. tostring(dd.ReferenceInstance == a) .. " " .. tostring(dd.ResponseStyle) .. " " .. tostring(dd.Responsiveness) .. " " .. tostring(dd.RunLocally) .. " " .. tostring(dd.TrackballRollFactor) .. " " .. typeof(dd.DragStart) .. " " .. typeof(dd.DragContinue) .. " " .. typeof(dd.DragEnd))
local truss = Instance.new("TrussPart")
print("TRUSS " .. tostring(truss.Style) .. " " .. tostring(truss:IsA("BasePart")))
truss.Style = Enum.Style.NoSupports
print("TRUSSW " .. tostring(truss.Style))
print("MESHSKIN " .. tostring(mp.HasSkinnedMesh) .. " " .. tostring(pcall(function() mp.HasSkinnedMesh = true end)))
local vs = Instance.new("VehicleSeat")
print("VSEAT " .. tostring(vs.AreHingesDetected) .. " " .. tostring(pcall(function() vs.AreHingesDetected = 1 end)))
local ex = Instance.new("Explosion")
print("EXPL " .. tostring(ex.LocalTransparencyModifier))
ex.LocalTransparencyModifier = 0.5
print("EXPLW " .. tostring(ex.LocalTransparencyModifier))
print("ASSETSVC " .. tostring(pcall(function() return game:GetService("AssetService").AllowInsertFreeAssets end)))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 14.0:
		return false
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("PVInstance and WorldRoot sit in the chain", heard.call("ISA true true true true false"), true)
	check("the absent classes are declared", heard.call("NEWCLASSES TrussPart true true true true true true true true"), true)
	check("Region3: corners, CFrame, Size, ExpandToGrid", heard.call("REGION3 Region3 true 8, 2, 8 12, 8, 12 true"), true)
	check("PivotOffset starts at identity", heard.call("PIVOTOFF0 true true"), true)
	check("GetPivot is CFrame * PivotOffset", heard.call("PIVOTOFF1 true"), true)
	check("PivotTo lands the pivot, not the part", heard.call("PIVOTOFF2 true true"), true)
	check("WorldPivot reads as GetPivot", heard.call("WPIVOT0 true true"), true)
	check("WorldPivot written with no PrimaryPart is held", heard.call("WPIVOT1 true true"), true)
	check("PivotTo moves the parts by the held pivot", heard.call("WPIVOT2 true true true"), true)
	check("WorldPivot written with a PrimaryPart is its PivotOffset", heard.call("WPIVOT3 true true"), true)
	check("TranslateBy moves every part", heard.call("TRANSLATE true true"), true)
	check("GetExtentsSize is the parts' box", heard.call("EXTENTS 12, 4, 4"), true)
	check("Model's recorded settings read their defaults", heard.call("MODELREC Enum.ModelLevelOfDetail.Automatic Enum.ModelStreamingMode.Default"), true)
	check("ModelStreamingMode takes a write, LevelOfDetail is a plugin's", heard.call("MODELRECW Enum.ModelStreamingMode.Persistent false"), true)
	check("persistent players: added once", heard.call("PERSIST 1 true"), true)
	check("persistent players: removed", heard.call("PERSIST2 0"), true)
	check("CurrentPhysicalProperties is the material's; ExtentsCFrame the part's", heard.call("CURPHYS PhysicalProperties true true false"), true)
	check("BasePart's recorded settings read their defaults", heard.call("BPREC true 0 0 1"), true)
	check("and take a write; ReceiveAge is read only", heard.call("BPRECW false 0.5 false"), true)
	check("GetClosestPointOnSurface clamps and pushes out", heard.call("CLOSEST 52, 10, 0 50, 11, 0"), true)
	check("GetVelocityAtPosition adds the spin", heard.call("VELAT 3, 0, 0"), true)
	check("GetJoints: the weld, the no-collision and the hinge", heard.call("JOINTS 3 3 1 true"), true)
	check("Attachment:GetConstraints", heard.call("CONSTRAINTS 1 true 0"), true)
	check("CanCollideWith minds the NoCollisionConstraint", heard.call("CANCOLLIDE false true"), true)
	check("IsGrounded: welded to an anchor, or anchored", heard.call("GROUNDED true true"), true)
	check("IsGrounded: a free part is not", heard.call("GROUNDED2 false"), true)
	check("network ownership is the server's", heard.call("NETOWN nil true true false"), true)
	check("the inertia of a box, there and back", heard.call("TORQUE true true true"), true)
	check("Resize grows a face and refuses a vanishing side", heard.call("RESIZE true 6, 2, 2 91, 10, 0 true 6, 1, 2 91, 10.5, 0 false 6, 1, 2"), true)
	check("Bone: transformed frames, own and through a parent", heard.call("BONE true true true true false"), true)
	check("the wrap origins and their world twins", heard.call("WRAP true true true true true Enum.WrapLayerAutoSkin.Disabled"), true)
	check("CageOrigin is a plugin's to write; the world twin is read only", heard.call("WRAPSEC false false"), true)
	check("AutoSkin takes a write", heard.call("WRAPW Enum.WrapLayerAutoSkin.EnabledPreserve"), true)
	check("the diagonal and max-axis fields of view", heard.call("FOV true true Enum.FieldOfViewMode.Vertical"), true)
	check("DiagonalFieldOfView written sets FieldOfView", heard.call("FOVW1 true"), true)
	check("MaxAxisFieldOfView written sets FieldOfView", heard.call("FOVW2 true true"), true)
	check("Camera's recorded settings read their defaults", heard.call("CAMREC true true 1 false -0.1"), true)
	check("and take a write; NearPlaneZ is read only", heard.call("CAMRECW 1, 2, 3 false 2 true Enum.FieldOfViewMode.Diagonal false"), true)
	check("GetPartsObscuringTarget walks every wall, minus the ignored", heard.call("OBSCURE 2 true true 1 true 0"), true)
	check("WorldRotationAxis is Attachment0's WorldAxis", heard.call("CYLAXIS true true true 45"), true)
	check("the responsiveness figures read their defaults", heard.call("CONSREC 20 45 45"), true)
	check("and take a write", heard.call("CONSRECW 30"), true)
	check("LinearVelocity's new flags", heard.call("LINVEL false 0, 0 false"), true)
	check("and take a write", heard.call("LINVELW true 1, 2 true"), true)
	check("AlignOrientation's new fields", heard.call("ALIGNO Enum.AlignType.AllAxes 0, 0, 0 1, 0, 0 0, 1, 0 true"), true)
	check("and take a write", heard.call("ALIGNOW Enum.AlignType.PrimaryAxisLookAt 1, 2, 3 0, 1, 0 0, 0, 1"), true)
	check("TorsionSpringConstraint declared", heard.call("TORSION 3 0 0 false 45 0 0 0 100"), true)
	check("and takes writes; CurrentAngle read only", heard.call("TORSIONW 5 1 true 90 10 1 0.5 50 false"), true)
	check("UniversalConstraint declared", heard.call("UNIVERSAL false 45 0.15 0"), true)
	check("and takes writes", heard.call("UNIVERSALW true 10 1 1"), true)
	check("AnimationConstraint declared", heard.call("ANIMC 0 0 false 0 0 0 0 true"), true)
	check("and takes writes", heard.call("ANIMCW 1 2 true 3 4 5 6 1, 0, 0"), true)
	check("Motor starts still", heard.call("MOTOR0 0 0 0.5"), true)
	check("Motor:SetDesiredAngle, and CurrentAngle chases it", heard.call("MOTOR1 1 1"), true)
	check("Motor6D:SetDesiredAngle", heard.call("MOTOR6D 2"), true)
	check("VelocityMotor declared", heard.call("VMOTOR 0 0 0 nil"), true)
	check("and takes writes", heard.call("VMOTORW 1 2 3 true"), true)
	check("workspace.Terrain is the Terrain, read only", heard.call("WSTERRAIN Terrain false"), true)
	check("BulkMoveTo moves each to its CFrame", heard.call("BULKMOVE 1, 100, 0 2, 110, 0 false"), true)
	check("ArePartsTouchingOthers", heard.call("TOUCHING true false false"), true)
	check("the physics figures", heard.call("PHYSFIG true 100 60 true"), true)
	check("Workspace's recorded numbers read their defaults", heard.call("WSREC 1.2 0 Enum.FluidForces.Default 0, 0, 0 false 0, 0, 0 64 1024 false"), true)
	check("Workspace's recorded switches read Default", heard.call("WSENUMS Enum.StreamingIntegrityMode.Default Enum.StreamOutBehavior.Default Enum.ModelStreamingBehavior.Default Enum.PhysicsSteppingMethod.Default Enum.SignalBehavior.Default Enum.AvatarUnificationMode.Default Enum.ClientAnimatorThrottlingMode.Default Enum.MeshPartHeadsAndAccessories.Default Enum.RejectCharacterDeletions.Default Enum.ReplicateInstanceDestroySetting.Default Enum.SandboxedInstanceMode.Default Enum.PlayerCharacterDestroyBehavior.Default Enum.IKControlConstraintSupport.Default Enum.PrimalPhysicsSolver.Default Enum.RenderingCacheOptimizationMode.Default Enum.PathfindingUseImprovedSearch.Default Enum.AnimatorRetargetingMode.Default"), true)
	check("and take writes", heard.call("WSRECW 2 Enum.FluidForces.Experimental 1, 0, 0 2048 Enum.SignalBehavior.Deferred Enum.PrimalPhysicsSolver.Experimental Enum.AnimatorRetargetingMode.Enabled 0, 5, 0"), true)
	check("FallHeightEnabled is a plugin's; PersistentLoaded is a signal", heard.call("WSSEC false false RBXScriptSignal"), true)
	check("the cell arithmetic", heard.call("CELLS 6, 10, 14 4, 8, 12 1, -1, 2 1, -1, 2 1, -1, 2"), true)
	check("Terrain's recorded settings read their defaults", heard.call("TERRREC false 1"), true)
	check("and take a write", heard.call("TERRRECW true 0.5"), true)
	check("FillRegion fills, ReadVoxels reads it back, a resolution but 4 is refused", heard.call("FILLREGION true 4, 2, 4 4, 2, 4 Enum.Material.Grass Enum.Material.Grass 1 false"), true)
	check("ReadVoxels off the field is Air", heard.call("READAIR Enum.Material.Air 1, 1, 1"), true)
	check("ReplaceMaterial swaps the material", heard.call("REPLACE Enum.Material.Sand Enum.Material.Sand"), true)
	check("FillCylinder and FillWedge lay their shapes", heard.call("CYLWEDGE Enum.Material.Rock true Enum.Material.Air Enum.Material.Snow true true Enum.Material.Air"), true)
	check("WriteVoxels lands and reads back", heard.call("WRITEVOX Enum.Material.Mud 1 Enum.Material.Air 0"), true)
	check("ReadVoxelChannels splits ground and water", heard.call("CHANNELS Enum.Material.Mud 1 0 1, 1, 2"), true)
	check("WriteVoxelChannels lays water", heard.call("CHANNELSW 1 0"), true)
	check("Get/SetMaterialColor through the MaterialColors blob", heard.call("MATCOLOR Color3 true true true true false"), true)
	check("ProximityPrompt's new fields read their defaults", heard.call("PROMPT true Enum.KeyCode.ButtonX nil RBXScriptSignal RBXScriptSignal"), true)
	check("and take a write; the gamepad KeyCodes resolve", heard.call("PROMPTW false Enum.KeyCode.ButtonA true 1000 1015"), true)
	check("DragDetector declared with its defaults", heard.call("DRAG  false 1, 0, 0 true Enum.DragDetectorDragStyle.TranslateViewPlane Enum.KeyCode.ButtonR3 Enum.KeyCode.LeftControl Enum.KeyCode.ButtonR3 0 0, 0, 0 10000000 10000000 0 0, 0, 0 Enum.DragDetectorPermissionPolicy.Everybody nil Enum.DragDetectorResponseStyle.Geometric 10 false 0, 1, 0 1 1 32"), true)
	check("and takes writes; its events are signals", heard.call("DRAGW x true 0, 1, 0 1, 0, 0 Enum.DragDetectorDragStyle.RotateAxis Enum.KeyCode.ButtonL3 Enum.KeyCode.LeftShift 90 1 Enum.DragDetectorPermissionPolicy.Nobody true Enum.DragDetectorResponseStyle.Physical 5 true 3 RBXScriptSignal RBXScriptSignal RBXScriptSignal"), true)
	check("TrussPart declared", heard.call("TRUSS Enum.Style.AlternatingSupports true"), true)
	check("and takes a write", heard.call("TRUSSW Enum.Style.NoSupports"), true)
	check("MeshPart.HasSkinnedMesh is false and read only", heard.call("MESHSKIN false false"), true)
	check("VehicleSeat.AreHingesDetected is 0 and read only", heard.call("VSEAT 0 false"), true)
	check("Explosion.LocalTransparencyModifier", heard.call("EXPL 0"), true)
	check("and takes a write", heard.call("EXPLW 0.5"), true)
	check("AssetService.AllowInsertFreeAssets is the engine's own", heard.call("ASSETSVC false"), true)
	check("no script error", heard.call("ERROR"), false)
	for l in lines:
		print(l)
	for l in said:
		if l.find("ERROR") != -1:
			print(l)
	print("PARTS %d passed, %d failed" % [ok, bad])
	print("parts parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
