# The rest family: the controller stack, IKControl, LineForce, the animation curves, the Input
# Action System, the descriptions, styling, video, captures, the engine settings, ReflectionService,
# EncodingService and the Studio's and Roblox's own services.
#
#   godot --headless --path . -s res://tests/rest_parity_test.gd
#
# A server script drives the members the server can reach; a LocalScript takes the keyboard for
# the Input Action System, with K pressed and released from here.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var ok := 0
var bad := 0
var said: Array[String] = []
var lines: Array[String] = []
var stage := 0
var stageAt := 0.0

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("RESTPAR %-62s %-10s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _key(code: Key, pressed: bool) -> void:
	var ev := InputEventKey.new()
	ev.physical_keycode = code
	ev.keycode = code
	ev.pressed = pressed
	Input.parse_input_event(ev)

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("StarterPlayer/StarterPlayerScripts/Ias.client.luau", """
local ctx = Instance.new("InputContext")
ctx.Parent = script
local jump = Instance.new("InputAction")
jump.Name = "Jump"
jump.Parent = ctx
local bind = Instance.new("InputBinding")
bind.KeyCode = Enum.KeyCode.K
bind.Parent = jump
local moves = {}
jump.Pressed:Connect(function() table.insert(moves, "pressed") end)
jump.Released:Connect(function() table.insert(moves, "released") end)
jump.StateChanged:Connect(function(v) table.insert(moves, "state=" .. tostring(v)) end)
print("IASSTART " .. tostring(jump:GetState()) .. " " .. tostring(jump.PreferredBinding == bind) .. " " .. tostring(ctx.Priority) .. " " .. tostring(ctx.Sink))
-- a Direction2D action fed by hand
local move = Instance.new("InputAction")
move.Type = Enum.InputActionType.Direction2D
move.Parent = ctx
local mb = Instance.new("InputBinding")
mb.Type = Enum.InputBindingType.Scriptable
mb.Parent = move
local moved
move.StateChanged:Connect(function(v) moved = v end)
mb:Fire(Vector2.new(0.5, -1))
print("IASFIRE " .. tostring(move:GetState()) .. " " .. tostring(moved))
-- a sinking context above it takes the same key first
local top = Instance.new("InputContext")
top.Priority = 2000
top.Sink = true
top.Parent = script
local topAction = Instance.new("InputAction")
topAction.Parent = top
local topBind = Instance.new("InputBinding")
topBind.KeyCode = Enum.KeyCode.L
topBind.Parent = topAction
local lowAction = Instance.new("InputAction")
lowAction.Parent = ctx
local lowBind = Instance.new("InputBinding")
lowBind.KeyCode = Enum.KeyCode.L
lowBind.Parent = lowAction
print("IASREADY")
task.wait(2.5)
print("IASHELD " .. tostring(jump:GetState()) .. " " .. table.concat(moves, ","))
print("IASSINK " .. tostring(topAction:GetState()) .. " " .. tostring(lowAction:GetState()))
task.wait(1.5)
print("IASDONE " .. tostring(jump:GetState()) .. " " .. table.concat(moves, ","))
""")

	world.load_file("ServerScriptService/Rest.server.luau", """
local function has(name, creatable)
	local ok, r = pcall(function() return Instance.new(name) end)
	if creatable then return ok and r ~= nil and r.ClassName == name end
	return not ok
end
local creatable = {"ControllerPartSensor", "BuoyancySensor", "AtmosphereSensor", "FluidForceSensor", "GroundController", "AirController", "ClimbController",
	"SwimController", "ControllerManager", "IKControl", "LineForce", "FloatCurve", "RotationCurve", "MarkerCurve", "EulerRotationCurve", "Vector3Curve",
	"ValueCurve", "CompositeValueCurve", "InputContext", "InputAction", "InputBinding", "InputActionLabel", "AccessoryDescription", "BodyPartDescription",
	"MakeupDescription", "HumanoidRigDescription", "DigitsRigDescription", "FaceControls", "HapticEffect", "GetTextBoundsParams", "StyleSheet", "StyleRule",
	"StyleDerive", "StyleLink", "StyleQuery", "VideoPlayer", "VideoDisplay", "AnimationNodeDefinition", "AdPortal", "SkateboardController", "CustomLog",
	"ExperienceInviteOptions", "GeneratedFolder", "ProceduralModel", "SurfaceSelection", "TextChannelWindow", "TextGenerator", "WorldModel", "WrapTextureTransfer"}
local abstract = {"SensorBase", "ControllerSensor", "ControllerBase", "PoseBase", "StyleBase", "BaseCoreGuiConfiguration", "Capture", "VideoCapture",
	"ConfigSnapshot", "Controller", "TextFilterResult", "TextFilterTranslatedResult", "PackageLink", "ReflectionMetadataItem", "MemStorageConnection",
	"NetworkPeer", "NetworkReplicator", "NetworkMarker", "File", "MLSession", "VideoSampler", "VirtualInput", "WebStreamClient", "ScreenshotHud",
	"StudioScreenshotCapture", "AssetPatchSettings", "SelectionLasso"}
local services = {"VideoService", "CaptureService", "StudioCaptureService", "CoreGuiConfiguration", "DebugSettings", "NetworkSettings", "PhysicsSettings",
	"RenderSettings", "TaskScheduler", "DraggerService", "ReflectionService", "EncodingService", "AnimationClipProvider", "AssetDeliveryProxy",
	"IncrementalPatchBuilder", "CommerceService", "ConfigService", "GamepadService", "GenerationService", "HeapProfilerService", "ScriptProfilerService",
	"ScriptDebuggerService", "InstanceFileSyncService", "MLService", "MatchmakingService", "ModerationService", "NetworkClient", "NetworkServer",
	"PlayerViewService", "RecommendationService", "RemoteCommandService", "SceneAnalysisService", "SerializationService", "StudioDeviceSimulatorService",
	"StudioTestService", "UniqueIdLookupService"}
local missing = {}
for _, n in ipairs(creatable) do if not has(n, true) then table.insert(missing, n) end end
for _, n in ipairs(abstract) do if not has(n, false) then table.insert(missing, "creatable:" .. n) end end
for _, n in ipairs(services) do
	local ok, r = pcall(function() return game:GetService(n) end)
	if not (ok and r and r.ClassName == n) then table.insert(missing, "service:" .. n) end
end
print("NEWCLASSES " .. (#missing == 0 and "ok" or table.concat(missing, ",")))

-- every declared-only member reads its default and takes a write
local fails = {}
local function rw(inst, prop, want, write)
	local got = tostring(inst[prop])
	if got ~= want then table.insert(fails, inst.ClassName .. "." .. prop .. "=" .. got .. "~=" .. want) end
	if write ~= nil then
		local ok, err = pcall(function() inst[prop] = write end)
		if not ok then table.insert(fails, inst.ClassName .. "." .. prop .. " write: " .. tostring(err))
		elseif tostring(inst[prop]) ~= tostring(write) then table.insert(fails, inst.ClassName .. "." .. prop .. " kept " .. tostring(inst[prop])) end
	end
end
local sensor = Instance.new("ControllerPartSensor")
rw(sensor, "UpdateType", "Enum.SensorUpdateType.OnRead", Enum.SensorUpdateType.Manual)
rw(sensor, "HitNormal", "0, 1, 0", Vector3.new(1, 0, 0))
rw(sensor, "LadderSearchHeight", "6", 2)
rw(sensor, "LadderSearchOffset", "5", 1)
rw(sensor, "SearchDistance", "0", 3)
rw(sensor, "SensedMaterial", "Enum.Material.Air", Enum.Material.Grass)
rw(sensor, "SensedPart", "nil", workspace)
rw(sensor, "SensorMode", "Enum.SensorMode.Floor", Enum.SensorMode.Ladder)
rw(sensor, "HitFrame", tostring(CFrame.new()), CFrame.new(1, 2, 3))
local buoy = Instance.new("BuoyancySensor")
rw(buoy, "FullySubmerged", "false", true)
rw(buoy, "TouchingSurface", "false", true)
local atm = Instance.new("AtmosphereSensor")
rw(atm, "AirDensity", "0")
rw(atm, "RelativeWindVelocity", "0, 0, 0")
local gc = Instance.new("GroundController")
rw(gc, "Active", "false")
rw(gc, "BalanceRigidityEnabled", "false", true)
rw(gc, "MoveSpeedFactor", "1", 2)
rw(gc, "AccelerationTime", "0", 0.2)
rw(gc, "BalanceMaxTorque", "10000", 5)
rw(gc, "BalanceSpeed", "100", 5)
rw(gc, "DecelerationTime", "0", 0.1)
rw(gc, "Friction", "2", 3)
rw(gc, "FrictionWeight", "1", 2)
rw(gc, "GroundOffset", "1", 2)
rw(gc, "TurnSpeedFactor", "1", 2)
local ac = Instance.new("AirController")
rw(ac, "MaintainAngularMomentum", "true", false)
rw(ac, "MaintainLinearMomentum", "true", false)
rw(ac, "MoveMaxForce", "1000", 5)
rw(ac, "TurnMaxTorque", "10000", 5)
rw(ac, "TurnSpeedFactor", "1", 5)
local cc = Instance.new("ClimbController")
rw(cc, "AccelerationTime", "0", 1)
rw(cc, "MoveMaxForce", "10000", 5)
local sc = Instance.new("SwimController")
rw(sc, "PitchMaxTorque", "10000", 5)
rw(sc, "PitchSpeedFactor", "1", 5)
rw(sc, "RollMaxTorque", "10000", 5)
rw(sc, "RollSpeedFactor", "1", 5)
local cm = Instance.new("ControllerManager")
rw(cm, "ActiveController", "nil", gc)
rw(cm, "BaseMoveSpeed", "16", 20)
rw(cm, "BaseTurnSpeed", "8", 9)
rw(cm, "ClimbSensor", "nil", sensor)
rw(cm, "FacingDirection", "0, 0, 1", Vector3.new(1, 0, 0))
rw(cm, "GroundSensor", "nil", sensor)
rw(cm, "MovingDirection", "0, 0, 0", Vector3.new(1, 0, 0))
rw(cm, "RootPart", "nil", workspace)
rw(cm, "UpDirection", "0, 1, 0", Vector3.new(0, 0, 1))
local ik = Instance.new("IKControl")
rw(ik, "Enabled", "true", false)
rw(ik, "Priority", "0", 3)
rw(ik, "SmoothTime", "0.05", 0.1)
rw(ik, "Weight", "1", 0.5)
rw(ik, "Type", "Enum.IKControlType.Transform", Enum.IKControlType.LookAt)
rw(ik, "Pole", "nil", workspace)
rw(ik, "EndEffectorOffset", tostring(CFrame.new()), CFrame.new(0, 1, 0))
local lf = Instance.new("LineForce")
rw(lf, "ApplyAtCenterOfMass", "false", true)
rw(lf, "InverseSquareLaw", "false", true)
rw(lf, "Magnitude", "1000", 5)
rw(lf, "MaxForce", "inf", 10)
rw(lf, "ReactionForceEnabled", "false", true)
local erc = Instance.new("EulerRotationCurve")
rw(erc, "RotationOrder", "Enum.RotationOrder.XYZ", Enum.RotationOrder.ZYX)
rw(Instance.new("ValueCurve"), "Length", "0")
rw(Instance.new("CompositeValueCurve"), "CurveType", "Enum.CompositeValueCurveType.NumberRange", Enum.CompositeValueCurveType.Vector3)
local ib = Instance.new("InputBinding")
rw(ib, "ClampMagnitudeToOne", "true", false)
rw(ib, "PressedThreshold", "0.5", 0.7)
rw(ib, "ReleasedThreshold", "0.2", 0.1)
rw(ib, "ResponseCurve", "1", 2)
rw(ib, "Vector2Scale", "1, 1", Vector2.new(2, 2))
rw(ib, "Vector3Scale", "1, 1, 1", Vector3.new(2, 2, 2))
rw(ib, "Up", "Enum.KeyCode.Unknown", Enum.KeyCode.W)
rw(ib, "DisplayName", "", "Jump")
rw(ib, "Scale", "1", 3)
rw(ib, "Type", "Enum.InputBindingType.Automatic", Enum.InputBindingType.Scriptable)
local ictx = Instance.new("InputContext")
rw(ictx, "Enabled", "true", false)
rw(ictx, "Priority", "1000", 5)
rw(ictx, "Sink", "false", true)
local ial = Instance.new("InputActionLabel")
rw(ial, "ImageColor3", "1, 1, 1", Color3.new(0, 0, 1))
rw(ial, "TextWrapped", "false", true)
rw(ial, "ResolvedText", "")
rw(ial, "InputAction", "nil", Instance.new("InputAction"))
local ad = Instance.new("AccessoryDescription")
rw(ad, "AccessoryType", "Enum.AccessoryType.Unknown", Enum.AccessoryType.Hat)
rw(ad, "AssetId", "0", 5)
rw(ad, "IsLayered", "false", true)
rw(ad, "Order", "0", 2)
rw(ad, "Position", "0, 0, 0", Vector3.new(1, 1, 1))
rw(ad, "Rotation", "0, 0, 0", Vector3.new(1, 1, 1))
rw(ad, "Scale", "1, 1, 1", Vector3.new(2, 2, 2))
local bd = Instance.new("BodyPartDescription")
rw(bd, "BodyPart", "Enum.BodyPart.Head", Enum.BodyPart.Torso)
rw(bd, "Color", "0, 0, 0", Color3.new(1, 0, 0))
rw(bd, "HeadShape", "", "Round")
local md = Instance.new("MakeupDescription")
rw(md, "MakeupType", "Enum.MakeupType.Face", Enum.MakeupType.Lip)
rw(md, "Order", "0", 1)
local rig = Instance.new("HumanoidRigDescription")
rw(rig, "Neck", "nil", workspace)
rw(rig, "NeckRangeMax", "0, 0, 0", Vector3.new(1, 2, 3))
rw(rig, "NeckSize", "0", 2)
rw(rig, "NeckTposeAdjustment", tostring(CFrame.new()), CFrame.new(1, 2, 3))
local dg = Instance.new("DigitsRigDescription")
rw(dg, "Side", "Enum.DigitsRigDescriptionSide.None", Enum.DigitsRigDescriptionSide.Left)
rw(dg, "Thumb1", "nil", workspace)
rw(dg, "ThumbRange", "0, 0, 0", Vector3.new(1, 1, 1))
rw(dg, "Thumb1TposeAdjustment", tostring(CFrame.new()), CFrame.new(1, 2, 3))
local fc = Instance.new("FaceControls")
rw(fc, "JawDrop", "0", 0.5)
rw(fc, "LeftEyeClosed", "0", 1)
local he = Instance.new("HapticEffect")
rw(he, "Looped", "false", true)
rw(he, "Position", "0, 0, 0", Vector3.new(1, 1, 1))
rw(he, "Radius", "3", 1)
rw(he, "Type", "Enum.HapticEffectType.UIClick", Enum.HapticEffectType.Custom)
local tb = Instance.new("GetTextBoundsParams")
rw(tb, "RichText", "false", true)
rw(tb, "Width", "0", 100)
rw(tb, "Text", "", "hello")
rw(tb, "Size", "0", 14)
local sr = Instance.new("StyleRule")
rw(sr, "Priority", "0", 2)
rw(sr, "Selector", "", "Frame")
rw(sr, "SelectorError", "")
rw(Instance.new("StyleDerive"), "StyleSheet", "nil", Instance.new("StyleSheet"))
rw(Instance.new("StyleLink"), "StyleSheet", "nil", Instance.new("StyleSheet"))
rw(Instance.new("StyleQuery"), "IsActive", "false")
local vp = Instance.new("VideoPlayer")
rw(vp, "IsLoaded", "false")
rw(vp, "IsPlaying", "false")
rw(vp, "Looping", "false", true)
rw(vp, "PlaybackSpeed", "1", 2)
rw(vp, "TimePosition", "0", 3)
rw(vp, "Volume", "1", 0.5)
rw(vp, "TimeLength", "0")
rw(vp, "Resolution", "0, 0")
local vd = Instance.new("VideoDisplay")
rw(vd, "ResampleMode", "Enum.ResamplerMode.Default", Enum.ResamplerMode.Pixelated)
rw(vd, "ScaleType", "Enum.ScaleType.Stretch", Enum.ScaleType.Fit)
rw(vd, "TileSize", "{1, 0}, {1, 0}", UDim2.new(0, 5, 0, 5))
rw(vd, "VideoColor3", "1, 1, 1", Color3.new(1, 0, 0))
rw(vd, "VideoTransparency", "0", 0.5)
local an = Instance.new("AnimationNodeDefinition")
rw(an, "NodeType", "Enum.AnimationNodeType.InvalidNode", Enum.AnimationNodeType.ClipNode)
rw(Instance.new("AdPortal"), "Status", "Enum.AdUnitStatus.Inactive")
local sk = Instance.new("SkateboardController")
rw(sk, "Steer", "0")
rw(sk, "Throttle", "0")
local eio = Instance.new("ExperienceInviteOptions")
rw(eio, "InviteUser", "0", 5)
rw(eio, "PromptMessage", "", "Join")
local pm = Instance.new("ProceduralModel")
rw(pm, "Size", "12, 12, 12", Vector3.new(1, 1, 1))
rw(pm, "GenerationError", "")
rw(Instance.new("SurfaceSelection"), "TargetSurface", "Enum.NormalId.Right", Enum.NormalId.Top)
rw(Instance.new("TextChannelWindow"), "Target", "nil", workspace)
local tg = Instance.new("TextGenerator")
rw(tg, "Temperature", "0.7", 0.5)
rw(tg, "TopP", "0.9", 0.5)
rw(Instance.new("WorldModel"), "UseWorkspaceCollisionGroups", "false", true)
local wt = Instance.new("WrapTextureTransfer")
rw(wt, "UVMaxBound", "-inf, -inf", Vector2.new(1, 1))
rw(wt, "UVMinBound", "inf, inf", Vector2.new(0, 0))
rw(game:GetService("DebugSettings"), "JobCount", "1")
rw(game:GetService("RenderSettings"), "QualityLevel", "Enum.QualityLevel.Automatic")
rw(game:GetService("TaskScheduler"), "ThreadPoolConfig", "Enum.ThreadPoolConfig.Auto", Enum.ThreadPoolConfig.Threads4)
rw(game:GetService("TaskScheduler"), "ThreadPoolSize", "1")
rw(game:GetService("PhysicsSettings"), "PhysicsEnvironmentalThrottle", "Enum.EnviromentalPhysicsThrottle.DefaultAuto")
rw(game:GetService("NetworkSettings"), "IncomingReplicationLag", "0", 1)
rw(game:GetService("DraggerService"), "DraggerMovementMode", "Enum.DraggerMovementMode.Geometric", Enum.DraggerMovementMode.Physical)
rw(game:GetService("StudioTestService"), "EditModeActive", "false")
rw(game:GetService("CoreGuiConfiguration"), "PlayerListConfiguration", "nil")
rw(game:GetService("AssetDeliveryProxy"), "Port", "0", 8080)
print("DEFAULTS " .. (#fails == 0 and "ok" or table.concat(fails, " | ")))
print("POSEBASE " .. tostring(Instance.new("Pose"):IsA("PoseBase")) .. " " .. tostring(Instance.new("NumberPose"):IsA("PoseBase")) .. " " .. tostring(Instance.new("Pose").EasingStyle))
print("WORLDMODEL " .. tostring(Instance.new("WorldModel"):IsA("WorldRoot")) .. " " .. tostring(Instance.new("GroundController"):IsA("ControllerBase")) .. " " .. tostring(Instance.new("BuoyancySensor"):IsA("SensorBase")))

-- FloatCurve: keys, sampling, indices
local curve = Instance.new("FloatCurve")
local k0 = FloatCurveKey.new(0, 0, Enum.KeyInterpolationMode.Linear)
local k1 = FloatCurveKey.new(1, 10, Enum.KeyInterpolationMode.Constant)
local k2 = FloatCurveKey.new(2, 20, Enum.KeyInterpolationMode.Cubic)
local added, at = table.unpack(curve:InsertKey(k2))
local added0, at0 = table.unpack(curve:InsertKey(k0))
curve:InsertKey(k1)
local replaced = curve:InsertKey(FloatCurveKey.new(1, 10, Enum.KeyInterpolationMode.Constant))
print("FCURVE " .. curve.Length .. " " .. tostring(added) .. " " .. at .. " " .. tostring(added0) .. " " .. at0 .. " " .. tostring(replaced[1]) .. " " .. replaced[2])
print("FSAMPLE " .. curve:GetValueAtTime(0.5) .. " " .. curve:GetValueAtTime(1.5) .. " " .. curve:GetValueAtTime(5) .. " " .. curve:GetValueAtTime(-1) .. " " .. tostring(Instance.new("FloatCurve"):GetValueAtTime(0)))
local idx = curve:GetKeyIndicesAtTime(1.5)
local idx2 = curve:GetKeyIndicesAtTime(1)
print("FINDICES " .. idx[1] .. " " .. idx[2] .. " " .. idx2[1] .. " " .. idx2[2] .. " " .. #curve:GetKeys() .. " " .. tostring(curve:GetKeyAtIndex(2).Value) .. " " .. tostring(curve:GetKeyAtIndex(2).Interpolation))
local cubic = Instance.new("FloatCurve")
local a = FloatCurveKey.new(0, 0, Enum.KeyInterpolationMode.Cubic)
a.RightTangent = 0
local b = FloatCurveKey.new(2, 4, Enum.KeyInterpolationMode.Cubic)
b.LeftTangent = 0
cubic:SetKeys({b, a})
print("FCUBIC " .. cubic:GetValueAtTime(1) .. " " .. tostring(a.RightTangent) .. " " .. tostring(k0.LeftTangent) .. " " .. tostring(pcall(function() k0.RightTangent = 1 end)) .. " " .. tostring(typeof(a)))
print("FREMOVE " .. curve:RemoveKeyAtIndex(1, 2) .. " " .. curve.Length .. " " .. curve:GetKeyAtIndex(1).Time)
-- RotationCurve
local rot = Instance.new("RotationCurve")
rot:InsertKey(RotationCurveKey.new(0, CFrame.new(0, 0, 0), Enum.KeyInterpolationMode.Linear))
rot:InsertKey(RotationCurveKey.new(1, CFrame.new(10, 0, 0) * CFrame.Angles(0, math.rad(90), 0), Enum.KeyInterpolationMode.Linear))
local mid = rot:GetValueAtTime(0.5)
local _, ry, _ = mid:ToEulerAnglesYXZ()
print("RCURVE " .. rot.Length .. " " .. tostring(mid.Position) .. " " .. string.format("%.1f", math.deg(ry)) .. " " .. tostring(typeof(rot:GetKeyAtIndex(1))) .. " " .. tostring(rot:GetKeyAtIndex(2).Value.Position))
-- MarkerCurve
local mk = Instance.new("MarkerCurve")
mk:InsertMarkerAtTime(2, "late")
local mr = mk:InsertMarkerAtTime(1, "early")
print("MCURVE " .. mk.Length .. " " .. mr[2] .. " " .. mk:GetMarkerAtIndex(1).Value .. " " .. mk:GetMarkerAtIndex(1).Time .. " " .. #mk:GetMarkers() .. " " .. mk:RemoveMarkerAtIndex(1) .. " " .. mk:GetMarkers()[1].Value)
-- EulerRotationCurve / Vector3Curve: channels are child FloatCurves
local e = Instance.new("EulerRotationCurve")
e:Y():InsertKey(FloatCurveKey.new(0, 0, Enum.KeyInterpolationMode.Linear))
e:Y():InsertKey(FloatCurveKey.new(1, math.rad(90), Enum.KeyInterpolationMode.Linear))
local angles = e:GetAnglesAtTime(0.5)
local _, ey, _ = e:GetRotationAtTime(1):ToEulerAnglesYXZ()
print("ECURVE " .. tostring(e:Y() == e:FindFirstChild("Y")) .. " " .. #e:GetChildren() .. " " .. tostring(angles[1]) .. " " .. string.format("%.2f", angles[2]) .. " " .. string.format("%.1f", math.deg(ey)))
local v3 = Instance.new("Vector3Curve")
v3:X():InsertKey(FloatCurveKey.new(0, 5, Enum.KeyInterpolationMode.Constant))
local vv = v3:GetValueAtTime(3)
print("V3CURVE " .. tostring(vv[1]) .. " " .. tostring(vv[2]) .. " " .. v3:X().Name)

-- IKControl over a Motor6D chain
local rigModel = Instance.new("Model")
rigModel.Parent = workspace
local function part(name, x)
	local p = Instance.new("Part")
	p.Name = name
	p.Anchored = true
	p.Position = Vector3.new(x, 100, 0)
	p.Parent = rigModel
	return p
end
local root, mid1, tip = part("Root", 0), part("Mid", 3), part("Tip", 7)
local function motor(p0, p1)
	local m = Instance.new("Motor6D")
	m.Part0, m.Part1 = p0, p1
	m.Parent = p0
end
motor(root, mid1)
motor(mid1, tip)
local target = Instance.new("Part")
target.Anchored = true
target.Position = Vector3.new(20, 100, 0)
target.Parent = workspace
local ikc = Instance.new("IKControl")
ikc.ChainRoot = root
ikc.EndEffector = tip
ikc.Target = target
ikc.Offset = CFrame.new(0, 1, 0)
ikc.Parent = rigModel
print("IK " .. ikc:GetChainCount() .. " " .. ikc:GetChainLength() .. " " .. tostring(ikc:GetNodeWorldCFrame(1).Position) .. " " .. tostring(ikc:GetNodeLocalCFrame(1).Position) .. " " .. tostring(ikc:GetRawFinalTarget().Position) .. " " .. tostring(ikc:GetSmoothedFinalTarget().Position))

-- ReflectionService reads the registry
local RS = game:GetService("ReflectionService")
local partClass = RS:GetClass("Part")
local props = RS:GetPropertiesOfClass("Part")
local sizeOwner, ownProps = "?", 0
for _, p in ipairs(props) do if p.Name == "Size" then sizeOwner = p.Owner .. "/" .. p.Type.Name .. "/" .. tostring(p.Serialized) end end
for _, p in ipairs(RS:GetPropertiesOfClass("Part", {ExcludeInherited = true})) do ownProps += 1 end
ownProps = ownProps >= 1 and ownProps < 10
local methods, hasChildren = RS:GetMethodsOfClass("Part"), "?"
for _, m in ipairs(methods) do if m.Name == "GetChildren" then hasChildren = m.Owner .. "/" .. tostring(m.CanYield) end end
local events, touched = RS:GetEventsOfClass("Part"), "?"
for _, ev in ipairs(events) do if ev.Name == "Touched" then touched = ev.Owner end end
local subs = 0
for _, c in ipairs(RS:GetClasses({IsA = "BasePart"})) do subs += 1 end
print("REFLECT " .. partClass.Name .. " " .. partClass.Superclass .. " " .. tostring(#partClass.Subclasses) .. " " .. tostring(#RS:GetClasses() > 300) .. " " .. sizeOwner .. " " .. tostring(ownProps) .. " " .. hasChildren .. " " .. touched .. " " .. tostring(subs > 10) .. " " .. tostring(RS:GetClass("Nope")))

-- EncodingService base64
local ES = game:GetService("EncodingService")
local enc = ES:Base64Encode(buffer.fromstring("hello"))
local dec = ES:Base64Decode(enc)
print("BASE64 " .. typeof(enc) .. " " .. buffer.tostring(enc) .. " " .. buffer.tostring(dec) .. " " .. buffer.tostring(ES:Base64Encode("hi")) .. " " .. tostring(pcall(function() ES:Base64Decode("***") end)))

-- TextService's filter result: the text as it came
local fr = game:GetService("TextService"):FilterStringAsync("hello there", 1)
print("FILTER " .. fr.ClassName .. " " .. fr:GetNonChatStringForBroadcastAsync() .. "/" .. fr:GetNonChatStringForUserAsync(1) .. "/" .. fr:GetChatForUserAsync(1))

-- styling: rules and properties are kept, nothing applies them
local sheet = Instance.new("StyleSheet")
local rule = Instance.new("StyleRule")
rule.Selector = "Frame"
local changed = 0
sheet.StyleRulesChanged:Connect(function() changed += 1 end)
sheet:InsertStyleRule(rule, 3)
rule:SetProperty("BackgroundColor3", Color3.new(1, 0, 0))
rule:SetProperties({Size = UDim2.new(0, 10, 0, 10), Visible = false})
rule:SetPropertyTransition("Size", {Duration = 0.5})
rule:SetDefaultPropertyTransition({Duration = 1})
local q = Instance.new("StyleQuery")
q:SetCondition("Hover", true)
q:SetConditions({Dark = true})
local derive = Instance.new("StyleDerive")
sheet:SetDerives({derive})
task.wait()
print("STYLE " .. #sheet:GetStyleRules() .. " " .. rule.Priority .. " " .. changed .. " " .. tostring(rule:GetProperty("BackgroundColor3")) .. " " .. tostring(rule:GetProperties().Size) .. " " .. tostring(rule:GetProperties().Visible) .. " " .. tostring(rule:GetProperty("Nope")) .. " " .. tostring(rule:GetPropertyTransitions().Size.Duration) .. " " .. tostring(rule:GetDefaultPropertyTransition().Duration) .. " " .. tostring(q:GetCondition("Hover")) .. " " .. tostring(q:GetConditions().Dark) .. " " .. #sheet:GetDerives())
sheet:SetStyleRules({})
print("STYLE2 " .. #sheet:GetStyleRules() .. " " .. tostring(rule.Parent))

-- the rigs
rig.Neck = root
rig.LeftHip = mid1
local jn = rig:GetJointNames()
dg:SetFingerTip(2, Vector3.new(1, 2, 3))
dg:SetFingerControl(4, Vector3.new(4, 5, 6))
print("RIG " .. #rig:GetR15JointNames() .. " " .. #rig:GetR6JointNames() .. " " .. #jn .. " " .. jn[1] .. "," .. jn[2] .. " " .. tostring(rig:GetJointFromName("Neck") == root) .. " " .. tostring(rig:GetJointFromName("Nope")) .. " " .. tostring(dg:GetFingerTip(2)) .. " " .. tostring(dg:GetFingerControl(4)) .. " " .. tostring(dg:GetFingerTip(0)))
-- descriptions, pins, configs, attributes, controllers, captures, video, sensors, haptics
ad.Instance = root
local pins = Instance.new("AnimationNodeDefinition")
local pinsChanged = 0
pins.InputPinsChanged:Connect(function() pinsChanged += 1 end)
pins:AddInputPin("A")
pins:AddInputPin("B")
pins:AddInputPin("A")
pins:SetOrderedInputPinNames({"B", "A", "C"})
pins:RemoveInputPin("A")
task.wait()
print("DESC " .. tostring(ad:GetAppliedInstance() == root) .. " " .. tostring(Instance.new("MakeupDescription"):GetAppliedInstance()) .. " " .. table.concat(pins:GetOrderedInputPinNames(), ",") .. " " .. pinsChanged)
local CS = game:GetService("ConfigService")
CS:SetTestingValue("speed", 7)
CS:SetTestingValue("gone", 1)
CS:ClearTestingValue("gone")
local snap = CS:GetConfigAsync()
print("CONFIG " .. tostring(snap:GetValue("speed")) .. " " .. tostring(snap:GetValue("gone")) .. " " .. tostring(snap.Outdated) .. " " .. tostring(snap.Error) .. " " .. typeof(snap:GetValueChangedSignal("speed")) .. " " .. typeof(snap))
local MM = game:GetService("MatchmakingService")
print("MATCH " .. tostring(MM:SetServerAttribute("Region", "eu")) .. " " .. tostring(MM:GetServerAttribute("Region")) .. " " .. tostring(MM:GetServerAttribute("Nope")))
sk:BindButton(Enum.Button.Jump, "jump")
print("CTRL " .. tostring(sk:GetButton(Enum.Button.Jump)))
local Cap = game:GetService("CaptureService")
local saved
Cap:PromptSaveCapturesToGallery({}, function(r) saved = #r end)
local denied = false
Cap:PromptShareCapture(Content.none, "", function() end, function() denied = true end)
print("CAPTURE " .. tostring(saved) .. " " .. tostring(denied) .. " " .. tostring(Cap:PromptCaptureGalleryPermissionAsync(0)) .. " " .. #Cap:ReadCapturesFromGalleryAsync() .. " " .. tostring(game:GetService("StudioCaptureService"):CanCaptureScreenshot()))
local failedWith
vp.PlayFailed:Connect(function(err) failedWith = err end)
local status = vp:LoadAsync()
vp:Play()
task.wait()
print("VIDEO " .. tostring(status) .. " " .. tostring(failedWith) .. " " .. tostring(vp.IsPlaying) .. " " .. vp:GetOutputPins()[1] .. " " .. vd:GetInputPins()[1] .. " " .. #vp:GetConnectedWires("Output"))
local fs = Instance.new("FluidForceSensor")
local cp, ff, tq = fs:EvaluateAsync(Vector3.new(1, 0, 0), Vector3.new(), CFrame.new())
he:SetWaveformKeys({FloatCurveKey.new(0, 0, Enum.KeyInterpolationMode.Linear), FloatCurveKey.new(100, 1, Enum.KeyInterpolationMode.Linear)})
he:Play()
he:Stop()
print("SENSOR " .. tostring(cp) .. " " .. tostring(ff) .. " " .. tostring(tq) .. " " .. typeof(game:GetService("PlayerViewService"):GetDeviceCameraCFrame()) .. " " .. game:GetService("RenderSettings"):GetMaxQualityLevel())
local DS = game:GetService("DebugSettings")
print("DEBUG " .. tostring(DS.InstanceCount > 10) .. " " .. DS.PlayerCount .. " " .. DS.JobCount .. " " .. tostring(DS.DataModel))
local gf = Instance.new("GeneratedFolder")
gf:SetPrimaryPart(root)
game:GetService("GamepadService"):DisableGamepadCursor()
print("MISC " .. tostring(pm:ForceGeneration()) .. " " .. tostring(pm:WaitForGenerationAsync()) .. " " .. tostring(game:GetService("CommerceService"):UserEligibleForRealWorldCommerceAsync()) .. " " .. tostring(gf:IsA("Folder")))

-- LineForce pulls a free part toward an anchored one
local free = Instance.new("Part")
free.Position = Vector3.new(0, 50, 0)
free.Parent = workspace
local anchor = Instance.new("Part")
anchor.Anchored = true
anchor.Position = Vector3.new(30, 50, 0)
anchor.Parent = workspace
local a0 = Instance.new("Attachment")
a0.Parent = free
local a1 = Instance.new("Attachment")
a1.Parent = anchor
local force = Instance.new("LineForce")
force.Attachment0, force.Attachment1 = a0, a1
force.Magnitude = 200
force.Parent = free
task.wait(0.8)
print("LINEFORCE " .. tostring(free.Position.X > 3) .. " " .. tostring(free.Position.X < 30))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	if stage == 0 and heard.call("IASREADY"):
		stage = 1
		stageAt = t
	elif stage == 1 and t - stageAt > 0.4:
		_key(KEY_K, true)
		_key(KEY_L, true)
		stage = 2
	elif stage == 2 and t - stageAt > 2.9:
		_key(KEY_K, false)
		_key(KEY_L, false)
		stage = 3
	if t < 30.0 and not (heard.call("DONE") and heard.call("IASDONE")):
		return false
	check("the server script ran to the end", heard.call("DONE"), true)
	check("the LocalScript ran to the end", heard.call("IASDONE"), true)
	check("the creatable, abstract and service classes are declared", heard.call("NEWCLASSES ok"), true)
	check("every declared member reads its default and takes a write", heard.call("DEFAULTS ok"), true)
	check("PoseBase sits under Pose and NumberPose, with the easings", heard.call("POSEBASE true true Enum.PoseEasingStyle.Linear"), true)
	check("WorldModel is a WorldRoot, the controllers and sensors their bases", heard.call("WORLDMODEL true true true"), true)
	check("FloatCurve keeps sorted keys, one a time, with Length", heard.call("FCURVE 3 true 1 true 1 false 2"), true)
	check("FloatCurve samples linear, constant and the ends", heard.call("FSAMPLE 5 10 20 0 nil"), true)
	check("GetKeyIndicesAtTime brackets a time; keys read back", heard.call("FINDICES 2 3 2 2 3 10 Enum.KeyInterpolationMode.Constant"), true)
	check("cubic keys ease between their tangents; RightTangent needs Cubic", heard.call("FCUBIC 2 0 nil false FloatCurveKey"), true)
	check("RemoveKeyAtIndex takes a range", heard.call("FREMOVE 2 1 2"), true)
	check("RotationCurve blends two frames", heard.call("RCURVE 2 5, 0, 0 45.0 RotationCurveKey 10, 0, 0"), true)
	check("MarkerCurve keeps markers by time", heard.call("MCURVE 2 1 early 1 2 1 late"), true)
	check("EulerRotationCurve's channels are child FloatCurves; angles and rotation", heard.call("ECURVE true 1 nil 0.79 90.0"), true)
	check("Vector3Curve samples its channels", heard.call("V3CURVE 5 nil X"), true)
	check("IKControl walks the Motor6D chain; the target is Target times Offset", heard.call("IK 3 7 3, 100, 0 3, 0, 0 20, 101, 0 20, 101, 0"), true)
	check("ReflectionService reads classes, properties, methods and events", heard.call("REFLECT Part BasePart 2 true BasePart/Vector3/true true Instance/false BasePart true nil"), true)
	check("EncodingService encodes and decodes base64 as buffers", heard.call("BASE64 buffer aGVsbG8= hello aGk= false"), true)
	check("FilterStringAsync gives a TextFilterResult that reads the text back", heard.call("FILTER TextFilterResult hello there/hello there/hello there"), true)
	check("a StyleSheet keeps its rules, derives, properties, transitions; SetConditions replaces", heard.call("STYLE 1 3 1 1, 0, 0 {0, 10}, {0, 10} false nil 0.5 1 nil true 1"), true)
	check("SetStyleRules replaces them", heard.call("STYLE2 0 nil"), true)
	check("the rig descriptions name their joints and keep the finger points", heard.call("RIG 22 6 2 Neck,LeftHip true nil 1, 2, 3 4, 5, 6 0, 0, 0"), true)
	check("descriptions answer their Instance; a node's pins are ordered and fire", heard.call("DESC true nil B,C 4"), true)
	check("ConfigService snapshots the testing values", heard.call("CONFIG 7 nil false Enum.ConfigSnapshotErrorState.None RBXScriptSignal Object"), true)
	check("MatchmakingService keeps the server attributes", heard.call("MATCH true eu nil"), true)
	check("a Controller's buttons never press here", heard.call("CTRL false"), true)
	check("the captures decline", heard.call("CAPTURE 0 true false 0 false"), true)
	check("a VideoPlayer fails to load and never plays; its pins", heard.call("VIDEO Enum.AssetFetchStatus.Failure Enum.AssetFetchStatus.Failure false Output Input 0"), true)
	check("the sensor reads zero; the device camera and the quality cap", heard.call("SENSOR 0, 0, 0 0, 0, 0 0, 0, 0 CFrame 21"), true)
	check("DebugSettings reads the tree", heard.call("DEBUG true 1 1 0"), true)
	check("the generators and prompts decline", heard.call("MISC false false false true"), true)
	check("LineForce pulls the free part toward the anchored one", heard.call("LINEFORCE true true"), true)
	check("an InputAction starts false with its first binding preferred", heard.call("IASSTART false true 1000 false"), true)
	check("Fire hands a Direction2D action its state", heard.call("IASFIRE 0.5, -1 0.5, -1"), true)
	check("K held: the Bool action is pressed and its events fired", heard.call("IASHELD true state=true,pressed"), true)
	check("a sinking context takes the key from the one below", heard.call("IASSINK true false"), true)
	check("K released: the action is released", heard.call("IASDONE false state=true,pressed,state=false,released"), true)
	check("no script error", heard.call("ERROR"), false)
	for l in lines:
		print(l)
	for l in said:
		if bad > 0 or l.find("ERROR") != -1 or l.find("DEFAULTS") != -1 or l.find("NEWCLASSES") != -1:
			print(l)
	print("RESTPAR %d passed, %d failed" % [ok, bad])
	print("rest parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
