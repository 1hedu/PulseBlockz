# The character family: Humanoid, HumanoidDescription, BodyColors and the other dressing,
# animation clips and markers, Player / Players / StarterPlayer settings, the Mouse's surface.
#
#   godot --headless --path . -s res://tests/character_parity_test.gd
#
# A Play Solo world with one joined player. The server script exercises what has behaviour
# (asserting the behaviour) and reads back what is recorded only; the client script covers
# the Mouse and the per-player camera settings.
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
	lines.append("CHARPAR %-58s %-10s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("StarterPlayer/StarterPlayerScripts/Mouse.client.luau", """
local Players = game:GetService("Players")
local p = Players.LocalPlayer
local char = p.Character or p.CharacterAdded:Wait()
task.wait(1.5)
-- a slab under the camera, which looks straight down at it; the pointer sits at the viewport's corner
local slab = Instance.new("Part")
slab.Name = "Slab"
slab.Anchored = true
slab.Size = Vector3.new(200, 1, 200)
slab.Position = Vector3.new(300, 0, 300)
slab.Material = Enum.Material.Slate
slab.Parent = workspace
local cam = workspace.CurrentCamera
cam.CameraType = Enum.CameraType.Scriptable
cam.CFrame = CFrame.lookAt(Vector3.new(300, 40, 300), Vector3.new(300, 0, 300))
task.wait(0.3)
local mouse = p:GetMouse()
print("MTARGET " .. tostring(mouse.Target and mouse.Target.Name))
print("MSURFACE " .. tostring(mouse.TargetSurface))
print("MSURFACETYPE " .. tostring(typeof(mouse.TargetSurface)))
-- the per-player camera settings arrived from StarterPlayer at join
print("PZOOM " .. tostring(p.CameraMinZoomDistance) .. " " .. tostring(p.CameraMaxZoomDistance) .. " " .. tostring(p.CameraMode))
print("PMOUSELOCK " .. tostring(p.DevEnableMouseLock))
print("CLIENTDONE")
""")

	world.load_file("ServerScriptService/Character.server.luau", """
local Players = game:GetService("Players")
local sp = game:GetService("StarterPlayer")
sp.EnableMouseLockOption = false   -- seeds Player.DevEnableMouseLock; set before the player joins
local floor = Instance.new("Part")
floor.Name = "Floor"
floor.Anchored = true
floor.Size = Vector3.new(60, 1, 60)
floor.Position = Vector3.new(0, 0, 0)
floor.Material = Enum.Material.Grass
floor.Parent = workspace
local p = Players:GetPlayers()[1] or Players.PlayerAdded:Wait()
local char = p.Character or p.CharacterAdded:Wait()
local hum = char:WaitForChild("Humanoid", 10)
local root = char:WaitForChild("HumanoidRootPart", 10)
task.wait(1.5)

-- Player: seeded from StarterPlayer, and the account facts
print("PSEED " .. tostring(p.DevEnableMouseLock) .. " " .. tostring(p.CameraMaxZoomDistance) .. " " .. tostring(p.NameDisplayDistance) .. " " .. tostring(p.HealthDisplayDistance))
print("PACCOUNT " .. tostring(p.AccountAge) .. " " .. tostring(p.HasVerifiedBadge) .. " " .. tostring(p:IsVerified()) .. " " .. p.LocaleId .. " " .. tostring(p.FollowUserId))
p:SetAccountAge(30)
print("PAGE " .. tostring(p.AccountAge))
print("PMODES " .. tostring(p.DevComputerCameraMode) .. " " .. tostring(p.DevTouchMovementMode) .. " " .. tostring(p.DevCameraOcclusionMode) .. " " .. tostring(p.AutoJumpEnabled))
p.DevComputerCameraMode = Enum.DevComputerCameraMovementMode.Orbital
p.ReplicationFocus = root
p:AddReplicationFocus(root)
p:RemoveReplicationFocus(root)
p:RequestStreamAroundAsync(Vector3.new(0, 0, 0))
print("PWRITE " .. tostring(p.DevComputerCameraMode) .. " " .. tostring(p.ReplicationFocus == root))
print("PSOCIAL " .. tostring(#p:GetFriendsOnlineAsync()) .. " " .. tostring(p:IsFriendsWithAsync(1)) .. " " .. tostring(p:IsInGroupAsync(1)) .. " " .. tostring(p:HasAppearanceLoaded()) .. " " .. tostring(p:GetNetworkPing()) .. " " .. tostring(next(p:GetJoinData())))
print("PSECURE " .. tostring(pcall(function() return p.AgeChecked end)) .. " " .. tostring(pcall(function() p.GameplayPaused = true end)))
print("PNAMES " .. Players:GetNameFromUserIdAsync(p.UserId) .. " " .. tostring(Players:GetUserIdFromNameAsync(p.Name) == p.UserId) .. " " .. tostring(pcall(function() Players:GetNameFromUserIdAsync(987654321) end)))
print("PLAYERS " .. tostring(Players.BanningEnabled) .. " " .. tostring(Players.ClassicChat) .. " " .. tostring(Players.BubbleChat) .. " " .. tostring(Players.UseStrafingAnimations) .. " " .. tostring(Players.PreferredPlayers))
Players:SetChatStyle(Enum.ChatStyle.ClassicAndBubble)
Players.UseStrafingAnimations = true
print("CHATSTYLE " .. tostring(Players.ClassicChat) .. " " .. tostring(Players.BubbleChat) .. " " .. tostring(Players.UseStrafingAnimations))
local plainEvents = 0
for _, e in ipairs({"CharacterAppearanceLoaded", "Idled", "OnTeleport"}) do if typeof(p[e]) == "RBXScriptSignal" then plainEvents += 1 end end
for _, e in ipairs({"PlayerMembershipChanged", "UserSubscriptionStatusChanged"}) do if typeof(Players[e]) == "RBXScriptSignal" then plainEvents += 1 end end
print("PEVENTS " .. plainEvents)

-- StarterPlayer: the new settings read back and take a write
print("SPREAD " .. tostring(sp.AutoJumpEnabled) .. " " .. tostring(sp.CameraMode) .. " " .. tostring(sp.DevComputerCameraMovementMode) .. " " .. tostring(sp.EnableDynamicHeads) .. " " .. tostring(sp.LoadCharacterLayeredClothing) .. " " .. tostring(sp.UserEmotesEnabled) .. " " .. tostring(sp.CharacterBreakJointsOnDeath))
sp.CameraMode = Enum.CameraMode.LockFirstPerson
sp.DevTouchMovementMode = Enum.DevTouchMovementMode.DynamicThumbstick
sp.EnableDynamicHeads = Enum.LoadDynamicHeads.Disabled
print("SPWRITE " .. tostring(sp.CameraMode) .. " " .. tostring(sp.DevTouchMovementMode) .. " " .. tostring(sp.EnableDynamicHeads) .. " " .. tostring(pcall(function() return sp.AllowCustomAnimations end)))

-- Humanoid: the floor's material, the limbs, the states
print("FLOOR " .. tostring(hum.FloorMaterial))
print("MAXSLOPE " .. tostring(hum.MaxSlopeAngle))
print("HREAD " .. tostring(hum.CameraOffset) .. " " .. tostring(hum.AutoJumpEnabled) .. " " .. tostring(hum.EvaluateStateMachine) .. " " .. tostring(hum.TargetPoint))
hum.CameraOffset = Vector3.new(0, 2, 0)
hum.MaxSlopeAngle = 45
hum.AutoJumpEnabled = false
print("HWRITE " .. tostring(hum.CameraOffset) .. " " .. tostring(hum.MaxSlopeAngle) .. " " .. tostring(hum.AutoJumpEnabled) .. " " .. tostring(pcall(function() hum.FloorMaterial = Enum.Material.Sand end)))
local arm = char:FindFirstChild("Left Arm") or char:FindFirstChild("LeftUpperArm")
print("LIMB " .. tostring(hum:GetLimb(arm)) .. " " .. tostring(hum:GetLimb(char.Head)) .. " " .. tostring(hum:GetLimb(floor)))
print("BODYPART " .. tostring(hum:GetBodyPartR15(root)) .. " " .. tostring(hum:GetBodyPartR15(floor)) .. " " .. tostring(Enum.BodyPartR15.LeftHand.Value))
print("MOVEVEL " .. tostring(typeof(hum:GetMoveVelocity())))
local stateChanges = {}
hum.StateEnabledChanged:Connect(function(state, enabled) table.insert(stateChanges, tostring(state) .. "=" .. tostring(enabled)) end)
hum:SetStateEnabled(Enum.HumanoidStateType.Jumping, false)
hum:SetStateEnabled(Enum.HumanoidStateType.Jumping, false)
print("STATE " .. tostring(hum:GetStateEnabled(Enum.HumanoidStateType.Jumping)) .. " " .. tostring(hum:GetStateEnabled(Enum.HumanoidStateType.Running)))
hum:SetStateEnabled(Enum.HumanoidStateType.Jumping, true)
task.wait(0.1)
print("STATEEV " .. table.concat(stateChanges, ","))
local humEvents = 0
for _, e in ipairs({"Climbing", "FallingDown", "FreeFalling", "GettingUp", "PlatformStanding", "Ragdoll", "Strafing", "Swimming", "ApplyDescriptionFinished"}) do if typeof(hum[e]) == "RBXScriptSignal" then humEvents += 1 end end
print("HEVENTS " .. humEvents)
print("EMOTE " .. tostring(hum:PlayEmoteAsync("wave")))
local fell = {}
hum.FreeFalling:Connect(function(active) table.insert(fell, tostring(active)) end)

-- BodyColors: parented into the character, the limbs take its colours
local bc = Instance.new("BodyColors")
print("BCDEFAULT " .. tostring(bc.HeadColor) .. " " .. tostring(bc.TorsoColor) .. " " .. tostring(bc.LeftLegColor) .. " " .. tostring(bc.HeadColor3 == BrickColor.new("Bright yellow").Color))
bc.HeadColor = BrickColor.new("Really red")
print("BCALIAS " .. tostring(bc.HeadColor3 == BrickColor.new("Really red").Color) .. " " .. tostring(bc.HeadColor))
bc.TorsoColor3 = Color3.fromRGB(0, 0, 255)
bc.Parent = char
task.wait(0.1)
local torso = char:FindFirstChild("Torso") or char:FindFirstChild("UpperTorso")
print("BCPAINT " .. tostring(char.Head.Color == BrickColor.new("Really red").Color) .. " " .. tostring(torso.Color == Color3.fromRGB(0, 0, 255)) .. " " .. tostring(arm.Color == BrickColor.new("Bright yellow").Color))
bc.LeftArmColor3 = Color3.fromRGB(0, 255, 0)
task.wait(0.1)
print("BCCHANGE " .. tostring(arm.Color == Color3.fromRGB(0, 255, 0)))
print("BCISA " .. tostring(bc:IsA("CharacterAppearance")) .. " " .. tostring(pcall(function() Instance.new("CharacterAppearance") end)))

-- The rest of the dressing: recorded, with their Content twins
local shirt = Instance.new("Shirt")
shirt.ShirtTemplate = "rbxassetid://1"
local pants = Instance.new("Pants")
pants.PantsTemplateContent = Content.fromUri("rbxassetid://2")
local sg = Instance.new("ShirtGraphic")
sg.Graphic = "rbxassetid://3"
local cm = Instance.new("CharacterMesh")
cm.BodyPart = Enum.BodyPart.LeftArm
cm.MeshId = "rbxassetid://4"
cm.BaseTextureId = "rbxassetid://5"
cm.OverlayTextureId = "rbxassetid://6"
print("CLOTHES " .. tostring(shirt.ShirtTemplateContent.Uri) .. " " .. pants.PantsTemplate .. " " .. tostring(sg.TextureContent.Uri) .. " " .. tostring(shirt.Color3) .. " " .. tostring(sg.Color3) .. " " .. tostring(shirt:IsA("Clothing")))
print("CMESH " .. tostring(cm.BodyPart) .. " " .. tostring(cm.MeshContent.Uri) .. " " .. cm.BaseTextureId .. " " .. tostring(cm.OverlayTextureContent.Uri))
shirt.Parent = char pants.Parent = char sg.Parent = char cm.Parent = char
local hat = Instance.new("Accessory") hat.Parent = char
p:ClearCharacterAppearance()
task.wait(0.1)
print("CLEARED " .. tostring(char:FindFirstChildOfClass("Shirt")) .. " " .. tostring(char:FindFirstChildOfClass("BodyColors")) .. " " .. tostring(char:FindFirstChildOfClass("Accessory")) .. " " .. tostring(char:FindFirstChildOfClass("Humanoid") ~= nil))

-- HumanoidDescription: recorded, its colours applied, its emotes kept
local hd = Instance.new("HumanoidDescription")
print("HDREAD " .. tostring(hd.Head) .. " " .. tostring(hd.HeadColor) .. " " .. tostring(hd.HeightScale) .. " " .. tostring(hd.BodyTypeScale) .. " " .. hd.AccessoryBlob .. " " .. hd.HatAccessory .. " " .. tostring(hd.UseAvatarSettings))
hd.Head = 12345 hd.HatAccessory = "1,2" hd.HeightScale = 1.2 hd.Shirt = 7 hd.IdleAnimation = 8
hd.HeadColor = Color3.fromRGB(255, 0, 255)
hd.TorsoColor = Color3.fromRGB(10, 20, 30)
print("HDWRITE " .. tostring(hd.Head) .. " " .. hd.HatAccessory .. " " .. tostring(hd.HeightScale) .. " " .. tostring(hd.Shirt) .. " " .. tostring(hd.IdleAnimation))
hd:SetAccessories({{AssetId = 99, AccessoryType = Enum.AccessoryType.Hat, Order = 1}}, true)
local acc = hd:GetAccessories(true)
print("HDACC " .. tostring(#acc) .. " " .. tostring(acc[1].AssetId) .. " " .. tostring(acc[1].AccessoryType) .. " " .. tostring(hd.AccessoryBlob:find('"AssetId":99', 1, true) ~= nil))
local emoteChanges = 0
hd.EmotesChanged:Connect(function() emoteChanges += 1 end)
hd:SetEmotes({Wave = {1}, Dance = {2, 3}})
hd:AddEmote("Salute", 4)
hd:RemoveEmote("Wave")
hd:RemoveEmote("Nope")
local emotes = hd:GetEmotes()
local names = {}
for n in pairs(emotes) do table.insert(names, n) end
table.sort(names)
print("HDEMOTES " .. table.concat(names, ",") .. " " .. tostring(#emotes.Dance) .. " " .. tostring(emotes.Salute[1]) .. " " .. tostring(emoteChanges))
hd:SetEquippedEmotes({"Dance", "Salute"})
local eq = hd:GetEquippedEmotes()
print("HDEQUIP " .. tostring(#eq) .. " " .. eq[1].Name .. " " .. tostring(eq[2].Slot))
local applied = nil
hum.ApplyDescriptionFinished:Connect(function(d) applied = d end)
hum:ApplyDescriptionAsync(hd)
task.wait(0.1)
local got = hum:GetAppliedDescription()
print("HDAPPLY " .. tostring(char.Head.Color == Color3.fromRGB(255, 0, 255)) .. " " .. tostring(torso.Color == Color3.fromRGB(10, 20, 30)) .. " " .. tostring(got.Head) .. " " .. got.HatAccessory .. " " .. tostring(got ~= hd) .. " " .. tostring(applied == hd))
print("HDFRESH " .. tostring(Instance.new("Humanoid"):GetAppliedDescription().Head))

-- Animation clips: the class, the children methods, the markers
local seq = Instance.new("KeyframeSequence")
print("CLIP " .. tostring(seq:IsA("AnimationClip")) .. " " .. tostring(seq.Length) .. " " .. tostring(seq.Loop) .. " " .. tostring(seq.Priority) .. " " .. tostring(pcall(function() Instance.new("AnimationClip") end)))
seq.Length = 2.5
local k0 = Instance.new("Keyframe") k0.Name = "Start" k0.Time = 0
local k1 = Instance.new("Keyframe") k1.Name = "Hit" k1.Time = 0.5
seq:AddKeyframe(k0) seq:AddKeyframe(k1)
local pose = Instance.new("Pose") pose.Name = "Torso"
k1:AddPose(pose)
local sub = Instance.new("Pose") sub.Name = "Head"
pose:AddSubPose(sub)
local marker = Instance.new("KeyframeMarker") marker.Name = "Swing" marker.Value = "hard"
k1:AddMarker(marker)
print("CLIPKIDS " .. tostring(#seq:GetKeyframes()) .. " " .. tostring(#k1:GetPoses()) .. " " .. tostring(#pose:GetSubPoses()) .. " " .. tostring(#k1:GetMarkers()) .. " " .. k1:GetMarkers()[1].Value .. " " .. tostring(marker.Parent == k1) .. " " .. tostring(seq.Length))
seq:RemoveKeyframe(k0) k1:RemovePose(pose) pose:RemoveSubPose(sub) k1:RemoveMarker(marker)
print("CLIPGONE " .. tostring(#seq:GetKeyframes()) .. " " .. tostring(#k1:GetPoses()) .. " " .. tostring(#pose:GetSubPoses()) .. " " .. tostring(#k1:GetMarkers()) .. " " .. tostring(marker.Parent))
seq:AddKeyframe(k0) k1:AddMarker(marker) k1:AddPose(pose)
seq.Name = "Swing"
seq.Parent = game:GetService("ReplicatedStorage")
local anim = Instance.new("Animation")
anim.AnimationId = "ReplicatedStorage.Swing"
local animator = hum:FindFirstChildOfClass("Animator")
print("ANIMATOR " .. tostring(animator.EvaluationThrottled) .. " " .. tostring(animator.PreferLodEnabled))
animator.PreferLodEnabled = false
animator:ApplyJointVelocities({})
local track = animator:LoadAnimation(anim)
local reached = {}
track:GetMarkerReachedSignal("Swing"):Connect(function(v) table.insert(reached, tostring(v)) end)
track:SetParameter("Blend", 0.7)
print("TRACKPARAM " .. tostring(track:GetParameter("Blend")) .. " " .. tostring(track:GetParameter("Nope")) .. " " .. tostring(next(track:GetParameterDefaults())) .. " " .. tostring(animator.PreferLodEnabled))
track:Play()
task.wait(1.5)
print("MARKER " .. table.concat(reached, ","))

-- Motor6D from RigAttachments: a two-part rig
local rig = Instance.new("Model") rig.Name = "Rig"
local rroot = Instance.new("Part") rroot.Name = "HumanoidRootPart" rroot.Anchored = true rroot.Position = Vector3.new(100, 10, 100) rroot.Parent = rig
local rhead = Instance.new("Part") rhead.Name = "Head" rhead.Position = Vector3.new(100, 12, 100) rhead.Parent = rig
local a0 = Instance.new("Attachment") a0.Name = "NeckRigAttachment" a0.Position = Vector3.new(0, 1, 0) a0.Parent = rroot
local a1 = Instance.new("Attachment") a1.Name = "NeckRigAttachment" a1.Position = Vector3.new(0, -0.5, 0) a1.Parent = rhead
local rhum = Instance.new("Humanoid") rhum.Parent = rig
rig.Parent = workspace
rhum:BuildRigFromAttachments()
rhum:BuildRigFromAttachments()
local neck = rhead:FindFirstChild("Neck")
print("RIG " .. tostring(neck and neck.ClassName) .. " " .. tostring(neck and neck.Part0 == rroot) .. " " .. tostring(neck and neck.Part1 == rhead) .. " " .. tostring(neck and neck.C0.Position) .. " " .. tostring(neck and neck.C1.Position) .. " " .. tostring(#rhead:GetChildren()))
local newHead = Instance.new("Part") newHead.Size = Vector3.new(1, 1, 1)
print("REPLACE " .. tostring(rhum:ReplaceBodyPartR15(Enum.BodyPartR15.Head, newHead)) .. " " .. tostring(rig:FindFirstChild("Head") == newHead) .. " " .. tostring(newHead:FindFirstChild("Neck") ~= nil) .. " " .. tostring(newHead.Neck.Part1 == newHead) .. " " .. tostring(rhead.Parent) .. " " .. tostring(rhum:ReplaceBodyPartR15(Enum.BodyPartR15.LeftHand, Instance.new("Part"))))

-- PlayerScripts' movement-mode registry: takes the calls
local ps = p:FindFirstChildOfClass("PlayerScripts")
ps:RegisterComputerMovementMode(Instance.new("ModuleScript")) ps:RegisterTouchCameraMovementMode(Instance.new("ModuleScript"))
ps:ClearComputerMovementModes() ps:ClearTouchMovementModes() ps:ClearComputerCameraMovementModes() ps:ClearTouchCameraMovementModes()
local pg = p:FindFirstChildOfClass("PlayerGui")
pg.ScreenOrientation = Enum.ScreenOrientation.Portrait
print("PGUI " .. tostring(pg.ScreenOrientation) .. " " .. tostring(pg.CurrentScreenOrientation) .. " " .. tostring(pg.SelectionImageObject))

-- Already computed, kept: the Accoutrement's point, the Motor's Transform, the Pose's CFrame, the Tool's Grip
local acc2 = Instance.new("Accessory")
acc2.AttachmentPoint = CFrame.new(1, 2, 3)
local pose2 = Instance.new("Pose") pose2.CFrame = CFrame.new(4, 5, 6)
local tool = Instance.new("Tool") tool.Grip = CFrame.new(7, 8, 9)
print("COMPUTED " .. tostring(acc2.AttachmentPoint.Position) .. " " .. tostring(pose2.CFrame.Position) .. " " .. tostring(tool.Grip.Position) .. " " .. tostring(neck.Transform.Position) .. " " .. tostring(hum.RootPart == root))

-- Freefall: the character is lifted and dropped, and FreeFalling reports the edges
root.CFrame = CFrame.new(0, 30, 0)
task.wait(2.5)
print("FELL " .. table.concat(fell, ","))
print("FLOOR2 " .. tostring(hum.FloorMaterial))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 14.0:
		return false
	var line := func(tag: String) -> String:
		for l in said:
			if l.begins_with(tag + " "):
				return l.substr(tag.length() + 1)
		return "<missing>"
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	check("the server script ran to the end", heard.call("DONE"), true)
	check("the client script ran to the end", heard.call("CLIENTDONE"), true)
	# Mouse
	check("Mouse.Target is the slab under the pointer", line.call("MTARGET"), "Slab")
	check("Mouse.TargetSurface is the face hit, an Enum.NormalId", line.call("MSURFACE") + " " + line.call("MSURFACETYPE"), "Enum.NormalId.Top EnumItem")
	# Player
	check("the client sees the seeded zoom range and camera mode", line.call("PZOOM"), "0.5 400 Enum.CameraMode.Classic")
	check("EnableMouseLockOption seeds Player.DevEnableMouseLock (client)", line.call("PMOUSELOCK"), "false")
	check("the seeds on the server: mouse lock, zoom, the two distances", line.call("PSEED"), "false 400 100 100")
	check("account facts read their defaults", line.call("PACCOUNT"), "0 false false en-us 0")
	check("SetAccountAge writes AccountAge", line.call("PAGE"), "30")
	check("the camera and touch modes read their defaults", line.call("PMODES"), "Enum.DevComputerCameraMovementMode.UserChoice Enum.DevTouchMovementMode.UserChoice Enum.DevCameraOcclusionMode.Zoom true")
	check("a mode and the ReplicationFocus take a write; the focus calls are taken", line.call("PWRITE"), "Enum.DevComputerCameraMovementMode.Orbital true")
	check("no friends, groups, ping or join data; the appearance is loaded", line.call("PSOCIAL"), "0 false false true 0 nil")
	check("AgeChecked cannot be read, GameplayPaused cannot be set", line.call("PSECURE"), "false false")
	check("name <-> user id by the players here; an unknown one errors", line.call("PNAMES").begins_with("Player 1 true false") or line.call("PNAMES").ends_with("true false"), true)
	check("Players' new settings read their defaults", line.call("PLAYERS"), "true true false false 0")
	check("SetChatStyle writes the two flags; UseStrafingAnimations takes a write", line.call("CHATSTYLE"), "true true true")
	check("the five new Player / Players events are signals", line.call("PEVENTS"), "5")
	# StarterPlayer
	check("StarterPlayer's new settings read their defaults", line.call("SPREAD"), "true Enum.CameraMode.Classic Enum.DevComputerCameraMovementMode.UserChoice Enum.LoadDynamicHeads.Default Enum.LoadCharacterLayeredClothing.Default true true")
	check("and take a write; AllowCustomAnimations is Roblox's own", line.call("SPWRITE"), "Enum.CameraMode.LockFirstPerson Enum.DevTouchMovementMode.DynamicThumbstick Enum.LoadDynamicHeads.Disabled false")
	# Humanoid
	check("FloorMaterial is the floor's material while standing", line.call("FLOOR"), "Enum.Material.Grass")
	check("MaxSlopeAngle is seeded from CharacterMaxSlopeAngle", line.call("MAXSLOPE"), "89")
	check("the new Humanoid settings read their defaults", line.call("HREAD"), "0, 0, 0 true true 0, 0, 0")
	check("and take a write; FloorMaterial is read-only", line.call("HWRITE"), "0, 2, 0 45 false false")
	check("GetLimb: an arm, the head, a part that is no limb", line.call("LIMB"), "Enum.Limb.LeftArm Enum.Limb.Head Enum.Limb.Unknown")
	check("GetBodyPartR15: the root, a stranger; the enum's values", line.call("BODYPART"), "Enum.BodyPartR15.RootPart Enum.BodyPartR15.Unknown 9")
	check("GetMoveVelocity is a Vector3", line.call("MOVEVEL"), "Vector3")
	check("SetStateEnabled records; GetStateEnabled reads it back", line.call("STATE"), "false true")
	check("StateEnabledChanged fires once per change, not per call", line.call("STATEEV"), "Enum.HumanoidStateType.Jumping=false,Enum.HumanoidStateType.Jumping=true")
	check("the nine new Humanoid events are signals", line.call("HEVENTS"), "9")
	check("PlayEmoteAsync plays nothing", line.call("EMOTE"), "false")
	check("FreeFalling fires on the way into and out of Freefall", line.call("FELL"), "true,false")
	check("FloorMaterial is the floor again after the fall", line.call("FLOOR2"), "Enum.Material.Grass")
	# BodyColors and the dressing
	check("BodyColors defaults: the classic palette, as BrickColors", line.call("BCDEFAULT"), "Bright yellow Bright blue Br. yellowish green true")
	check("HeadColor and HeadColor3 are one value", line.call("BCALIAS"), "true Really red")
	check("parented into the character, the limbs take its colours", line.call("BCPAINT"), "true true true")
	check("and a later change repaints", line.call("BCCHANGE"), "true")
	check("BodyColors is a CharacterAppearance; that base is not creatable", line.call("BCISA"), "true false")
	check("Shirt, Pants and ShirtGraphic record their templates through either twin", line.call("CLOTHES"), "rbxassetid://1 rbxassetid://2 rbxassetid://3 1, 1, 1 1, 1, 1 true")
	check("CharacterMesh records its part and three assets", line.call("CMESH"), "Enum.BodyPart.LeftArm rbxassetid://4 rbxassetid://5 rbxassetid://6")
	check("ClearCharacterAppearance takes the dressing off and leaves the Humanoid", line.call("CLEARED"), "nil nil nil true")
	# HumanoidDescription
	check("HumanoidDescription reads its defaults", line.call("HDREAD"), "0 0, 0, 0 1 0 []  false")
	check("and takes writes", line.call("HDWRITE"), "12345 1,2 1.2 7 8")
	check("SetAccessories / GetAccessories round-trip through AccessoryBlob", line.call("HDACC"), "1 99 Enum.AccessoryType.Hat true")
	check("emotes: set, add, remove; EmotesChanged per change that changed something", line.call("HDEMOTES"), "Dance,Salute 2 4 3")
	check("equipped emotes take names and give {Name, Slot}", line.call("HDEQUIP"), "2 Dance 2")
	check("ApplyDescriptionAsync paints, keeps a copy, fires ApplyDescriptionFinished", line.call("HDAPPLY"), "true true 12345 1,2 true true")
	check("a Humanoid nothing was applied to gives a fresh description", line.call("HDFRESH"), "0")
	# clips
	check("KeyframeSequence is an AnimationClip with Length; the base is not creatable", line.call("CLIP"), "true 0 true Enum.AnimationPriority.Action false")
	check("Add*/Get* on KeyframeSequence, Keyframe and Pose; a marker's Value", line.call("CLIPKIDS"), "2 1 1 1 hard true 2.5")
	check("Remove* takes them out", line.call("CLIPGONE"), "1 0 0 0 nil")
	check("Animator's two settings read their defaults", line.call("ANIMATOR"), "false true")
	check("SetParameter reads back; no defaults; PreferLodEnabled takes a write", line.call("TRACKPARAM"), "0.7 nil nil false")
	check("GetMarkerReachedSignal fires with the marker's Value at its keyframe", line.call("MARKER").begins_with("hard"), true)
	# rigs
	check("BuildRigFromAttachments joins two parts by their RigAttachment, once", line.call("RIG"), "Motor6D true true 0, 1, 0 0, -0.5, 0 2")
	check("ReplaceBodyPartR15 swaps the part and its motors; a missing part is false", line.call("REPLACE"), "true true true true nil false")
	# PlayerGui
	check("PlayerGui's orientation settings", line.call("PGUI"), "Enum.ScreenOrientation.Portrait Enum.ScreenOrientation.LandscapeLeft nil")
	check("the CFrame-valued members that were already computed still are", line.call("COMPUTED"), "1, 2, 3 4, 5, 6 7, 8, 9 0, 0, 0 true")
	check("no script error", heard.call("ERROR"), false)
	for l in lines:
		print(l)
	for l in said:
		if l.begins_with("ERROR"):
			print("CHARPAR " + l)
	print("CHARPAR %d passed, %d failed" % [ok, bad])
	print("character parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
