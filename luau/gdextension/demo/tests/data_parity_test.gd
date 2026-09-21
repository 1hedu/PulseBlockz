# The data family: an Actor's messages, ValueBase and the three value classes, the Instance
# members (GetActor, IsPropertyModified, ResetPropertyToDefault, GetStyled, Sandboxed), the
# ServiceProvider base and ServiceAdded, the DataModel's creator and ids, RunContext on BaseScript,
# ChangeHistoryService's recording state, Dialog and DialogChoice, Feature, the plugin objects
# (PluginMenu, PluginAction's fields, the toolbar button's class, the dock's members), the
# StudioService, UserGameSettings' members and methods, UserSettings:Reset, and the recorded-only
# members.
#
#   godot --headless --path . -s res://tests/data_parity_test.gd
#
# A Play Solo world with one joined player; a server script and a plugin do the work.
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
	lines.append("DATAPAR %-62s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	# A Feature is not creatable; a place file carries one.
	world.load_file("Workspace/Feat.model.json", '{"ClassName": "Feature"}')
	world.load_file("StarterPlayer/StarterPlayerScripts/Loc.client.luau", "print('LOCAL ' .. tostring(script.RunContext))")
	world.load_file("ServerScriptService/Data.server.luau", """
local Players = game:GetService("Players")
local p = Players:GetPlayers()[1] or Players.PlayerAdded:Wait()
task.wait(0.5)
local function b(v) return v and "t" or "f" end

-- the chain: ServiceProvider, GenericSettings, ValueBase; the new classes are declared
print("CHAIN " .. b(game:IsA("ServiceProvider")) .. b(game:IsA("Instance")) .. b(UserSettings():IsA("GenericSettings")) .. b(UserSettings():IsA("ServiceProvider"))
	.. " " .. b(Instance.new("NumberValue"):IsA("ValueBase")) .. b(Instance.new("CFrameValue"):IsA("ValueBase")) .. b(Instance.new("StringValue"):IsA("ValueBase"))
	.. " " .. b(not pcall(Instance.new, "ValueBase")) .. b(not pcall(Instance.new, "ServiceProvider")) .. b(not pcall(Instance.new, "Feature")))
local made = ""
for _, n in ipairs({"Actor", "BinaryStringValue", "BrickColorValue", "RayValue", "Dialog", "DialogChoice"}) do
	local okc, i = pcall(Instance.new, n)
	made = made .. b(okc and i.ClassName == n)
end
print("CLASSES " .. made .. " " .. b(Instance.new("Actor"):IsA("Model")) .. " " .. b(game:GetService("StudioService").ClassName == "StudioService"))

-- an Actor's messages land on the next deferred pass; GetActor finds the nearest Actor
local actor = Instance.new("Actor"); actor.Name = "Worker"; actor.Parent = workspace
local part = Instance.new("Part"); part.Anchored = true; part.Parent = actor
local heard = {}
local conn = actor:BindToMessage("job", function(a, c) table.insert(heard, tostring(a) .. ":" .. tostring(c)) end)
local par = actor:BindToMessageParallel("job", function(a) table.insert(heard, "par" .. tostring(a)) end)
actor:SendMessage("job", "one", 2)
local before = #heard
task.wait(0.2)
local after = #heard
conn:Disconnect(); par:Disconnect()
actor:SendMessage("job", "two", 3)
task.wait(0.2)
print("ACTOR " .. typeof(conn) .. " " .. before .. " " .. after .. " " .. table.concat(heard, ",") .. " " .. #heard
	.. " " .. tostring(part:GetActor() == actor) .. " " .. tostring(actor:GetActor() == actor) .. " " .. tostring(workspace:GetActor()))

-- BrickColorValue: Medium stone grey, read and written as a BrickColor, Changed with one
local bc = Instance.new("BrickColorValue")
local bcHeard = ""
bc.Changed:Connect(function(v) bcHeard = typeof(v) .. ":" .. v.Name end)
local bcDefault = bc.Value
bc.Value = BrickColor.new("Bright red")
task.wait(0.05)
print("BRICKVAL " .. typeof(bcDefault) .. " " .. bcDefault.Name .. " " .. bc.Value.Name .. " " .. bcHeard)

-- RayValue: a Ray over its pair, Changed once with the whole Ray, the Value signal once
local rv = Instance.new("RayValue")
local rayFires, propFires, lastRay = 0, 0, nil
rv.Changed:Connect(function(v) rayFires += 1; lastRay = v end)
rv:GetPropertyChangedSignal("Value"):Connect(function() propFires += 1 end)
local rvDefault = rv.Value
rv.Value = Ray.new(Vector3.new(1, 2, 3), Vector3.new(0, 1, 0))
task.wait(0.05)
print("RAYVAL " .. typeof(rvDefault) .. " " .. tostring(rvDefault.Origin) .. " " .. tostring(rv.Value.Origin) .. " " .. tostring(rv.Value.Direction)
	.. " " .. rayFires .. " " .. propFires .. " " .. tostring(lastRay and lastRay.Direction) .. " " .. b(not pcall(function() rv.Value = 5 end)))
local bsv = Instance.new("BinaryStringValue")
print("BINVAL " .. bsv.ClassName .. " " .. typeof(bsv.Changed))

-- Instance: Sandboxed and IsInSandbox, UniqueId hidden, GetStyled, IsPropertyModified and the reset
local folder = Instance.new("Folder"); folder.Parent = workspace
local child = Instance.new("Part"); child.Anchored = true; child.Parent = folder
local sandDefault = folder.Sandboxed
folder.Sandboxed = true
print("SANDBOX " .. tostring(sandDefault) .. " " .. tostring(folder.Sandboxed) .. " " .. tostring(child.IsInSandbox) .. " " .. tostring(workspace.IsInSandbox)
	.. " " .. b(not pcall(function() child.IsInSandbox = false end)) .. " " .. b(not pcall(function() return folder.UniqueId end)))
child.Transparency = 0.5
local modified = child:IsPropertyModified("Transparency")
local nameMod = child:IsPropertyModified("Name")
child:ResetPropertyToDefault("Transparency")
print("PROPMOD " .. tostring(modified) .. " " .. tostring(nameMod) .. " " .. tostring(child:IsPropertyModified("Transparency")) .. " " .. tostring(child.Transparency)
	.. " " .. b(not pcall(function() child:IsPropertyModified("NoSuch") end)) .. " " .. b(not pcall(function() child:ResetPropertyToDefault("Mass") end))
	.. " " .. tostring(child:GetStyled("Anchored")) .. " " .. typeof(child:GetStyledPropertyChangedSignal("Size")) .. " " .. typeof(child.StyledPropertiesChanged))

-- DataModel: the creator, the two services as properties, the ids a game script may not set
local ssId = ""
local svcConn = game.ServiceAdded:Connect(function(s) ssId = s.ClassName end)
local ss = game:GetService("StudioService")
game:GetService("SharedTableRegistry")
task.wait(0.05)
svcConn:Disconnect()
print("DATAMODEL " .. tostring(game.CreatorId) .. " " .. tostring(game.CreatorType) .. " " .. tostring(game.Workspace == workspace) .. " " .. tostring(game.RunService == game:GetService("RunService"))
	.. " " .. b(not pcall(function() game.Workspace = nil end)) .. " " .. b(not pcall(function() game.CreatorId = 1 end))
	.. " " .. b(not pcall(function() game:SetPlaceId(5) end)) .. " " .. b(not pcall(function() game:GetJobsInfo() end))
	.. " " .. ssId .. " " .. typeof(game.Loaded) .. " " .. typeof(game.GraphicsQualityChangeRequest) .. " " .. typeof(game.ServerRestartScheduled) .. " " .. typeof(game.ServiceRemoving) .. " " .. typeof(game.Close))

-- RunContext sits on BaseScript, PluginSecurity to write
print("RUNCTX " .. tostring(script.RunContext) .. " " .. b(not pcall(function() script.RunContext = Enum.RunContext.Server end)) .. " " .. typeof(script:GetPropertyChangedSignal("RunContext")))

-- ChangeHistoryService: one recording at a time
local CHS = game:GetService("ChangeHistoryService")
local was = CHS:IsRecordingInProgress()
local id = CHS:TryBeginRecording("edit")
local second = CHS:TryBeginRecording("again")
local during = CHS:IsRecordingInProgress()
CHS:FinishRecording(id)
CHS:ResetWaypoints()
print("HISTORY " .. tostring(was) .. " " .. tostring(id) .. " " .. tostring(second) .. " " .. tostring(during) .. " " .. tostring(CHS:IsRecordingInProgress()))

-- Dialog and DialogChoice read their defaults and take a write
local d = Instance.new("Dialog")
local dc = Instance.new("DialogChoice")
print("DIALOG " .. tostring(d.BehaviorType) .. " " .. d.ConversationDistance .. " " .. tostring(d.GoodbyeChoiceActive) .. " [" .. d.GoodbyeDialog .. d.InitialPrompt .. "] " .. tostring(d.InUse)
	.. " " .. tostring(d.Purpose) .. " " .. tostring(d.Tone) .. " " .. d.TriggerDistance .. " " .. tostring(d.TriggerOffset) .. " " .. #d:GetCurrentPlayers() .. " " .. typeof(d.DialogChoiceSelected)
	.. " " .. tostring(dc.GoodbyeChoiceActive) .. " [" .. dc.GoodbyeDialog .. dc.ResponseDialog .. dc.UserDialog .. "]")
d.BehaviorType = Enum.DialogBehaviorType.MultiplePlayers; d.ConversationDistance = 10; d.GoodbyeChoiceActive = false; d.GoodbyeDialog = "bye"; d.InitialPrompt = "hi"
d.InUse = true; d.Purpose = Enum.DialogPurpose.Shop; d.Tone = Enum.DialogTone.Enemy; d.TriggerDistance = 4; d.TriggerOffset = Vector3.new(0, 1, 0)
dc.GoodbyeChoiceActive = false; dc.GoodbyeDialog = "a"; dc.ResponseDialog = "b"; dc.UserDialog = "c"
print("DIALOGW " .. tostring(d.BehaviorType) .. " " .. d.ConversationDistance .. " " .. tostring(d.GoodbyeChoiceActive) .. " " .. d.GoodbyeDialog .. d.InitialPrompt .. " " .. tostring(d.InUse)
	.. " " .. tostring(d.Purpose) .. " " .. tostring(d.Tone) .. " " .. d.TriggerDistance .. " " .. tostring(d.TriggerOffset) .. " " .. tostring(dc.GoodbyeChoiceActive) .. " " .. dc.GoodbyeDialog .. dc.ResponseDialog .. dc.UserDialog)

-- Feature, from the place
local feat = workspace:WaitForChild("Feat")
print("FEATURE " .. tostring(feat.FaceId) .. " " .. tostring(feat.InOut) .. " " .. tostring(feat.LeftRight) .. " " .. tostring(feat.TopBottom))
feat.FaceId = Enum.NormalId.Top; feat.InOut = Enum.InOut.Edge; feat.LeftRight = Enum.LeftRight.Left; feat.TopBottom = Enum.TopBottom.Bottom
print("FEATUREW " .. tostring(feat.FaceId) .. " " .. tostring(feat.InOut) .. " " .. tostring(feat.LeftRight) .. " " .. tostring(feat.TopBottom))

-- StudioService reads its defaults and takes a write
print("STUDIOSVC " .. tostring(ss.ActiveScript) .. " " .. tostring(ss.DraggerSolveConstraints) .. " " .. ss.GridSize .. " " .. ss.RotateIncrement .. " " .. tostring(ss.ShowConstraintDetails)
	.. " " .. ss.StudioLocaleId .. " " .. tostring(ss.UseLocalSpace) .. " " .. b(not pcall(function() return ss.ShowWeldDetails end)) .. " " .. ss:GetUserId()
	.. " [" .. ss:GetClassIcon("Part").Image .. "] " .. tostring(ss:GetClassIcon("Part").ImageRectSize) .. " " .. tostring(ss:PromptImportFileAsync()) .. " " .. #ss:PromptImportFilesAsync())
ss.DraggerSolveConstraints = true; ss.GridSize = 4; ss.RotateIncrement = 15; ss.ShowConstraintDetails = true; ss.StudioLocaleId = "fr-fr"; ss.UseLocalSpace = true
print("STUDIOSVCW " .. tostring(ss.DraggerSolveConstraints) .. " " .. ss.GridSize .. " " .. ss.RotateIncrement .. " " .. tostring(ss.ShowConstraintDetails) .. " " .. ss.StudioLocaleId .. " " .. tostring(ss.UseLocalSpace))

-- UserGameSettings: the readable members, the engine's own refused, the methods; UserSettings:Reset
local UGS = UserSettings():GetService("UserGameSettings")
print("UGS " .. UGS.MouseSensitivity .. " " .. UGS.GamepadCameraSensitivity .. " " .. tostring(UGS.ComputerCameraMovementMode) .. " " .. tostring(UGS.ComputerMovementMode)
	.. " " .. tostring(UGS.TouchCameraMovementMode) .. " " .. tostring(UGS.TouchMovementMode) .. " " .. UGS.RCCProfilerRecordFrameRate .. " " .. UGS.RCCProfilerRecordTimeFrame
	.. " " .. b(not pcall(function() return UGS.MasterVolume end)) .. b(not pcall(function() return UGS.Fullscreen end)) .. b(not pcall(function() UGS.VREnabled = true end))
	.. " " .. UGS:GetCameraYInvertValue() .. " " .. tostring(UGS:GetOnboardingCompleted("intro")) .. " " .. tostring(UGS:InFullScreen()) .. " " .. typeof(UGS:InStudioMode())
	.. " " .. typeof(UGS.FullscreenChanged) .. " " .. typeof(UGS.StudioModeChanged))
UGS:SetOnboardingCompleted("intro"); UGS:SetCameraYInvertVisible(true); UGS:SetGamepadCameraSensitivityVisible(true)
UGS.MouseSensitivity = 3; UGS.GamepadCameraSensitivity = 0.5; UGS.ComputerCameraMovementMode = Enum.ComputerCameraMovementMode.Orbital
UGS.ComputerMovementMode = Enum.ComputerMovementMode.ClickToMove; UGS.TouchCameraMovementMode = Enum.TouchCameraMovementMode.Follow
UGS.TouchMovementMode = Enum.TouchMovementMode.DynamicThumbstick; UGS.RCCProfilerRecordFrameRate = 30; UGS.RCCProfilerRecordTimeFrame = 5
print("UGSW " .. UGS.MouseSensitivity .. " " .. UGS.GamepadCameraSensitivity .. " " .. tostring(UGS.ComputerCameraMovementMode) .. " " .. tostring(UGS.ComputerMovementMode)
	.. " " .. tostring(UGS.TouchCameraMovementMode) .. " " .. tostring(UGS.TouchMovementMode) .. " " .. UGS.RCCProfilerRecordFrameRate .. " " .. UGS.RCCProfilerRecordTimeFrame
	.. " " .. tostring(UGS:GetOnboardingCompleted("intro")) .. " " .. b(not pcall(function() return UserSettings():IsUserFeatureEnabled("UserThing") end)))
UserSettings():Reset()
print("UGSRESET " .. UGS.MouseSensitivity .. " " .. tostring(UGS.ComputerMovementMode) .. " " .. UGS.RCCProfilerRecordFrameRate)

-- the enums
print("ENUMS " .. Enum.CreatorType.Group.Value .. " " .. Enum.RibbonTool.None.Value .. " " .. Enum.JointCreationMode.Surface.Value .. " " .. Enum.ComputerCameraMovementMode.CameraToggle.Value
	.. " " .. Enum.TouchMovementMode.DynamicThumbstick.Value .. " " .. Enum.CustomCameraMode.Follow.Value)
print("DONE")
""")
	# The plugin side: menus, actions, the ribbon tool, the prompts, the dock, the selection's thickness,
	# and the plugin-only DataModel setters.
	world.plugin_add("DataTools", """
local function b(v) return v and "t" or "f" end
local menu = plugin:CreatePluginMenu("m1", "Tools", "rbxasset://icon.png")
local act = plugin:CreatePluginAction("act1", "Do it", "Does the thing", "rbxasset://a.png", false)
menu:AddAction(act)
local added = menu:AddNewAction("act2", "Second")
local sep = menu:AddSeparator()
local sub = plugin:CreatePluginMenu("m2", "Sub")
menu:AddMenu(sub)
local n = #menu:GetChildren()
local shown = menu:ShowAsync()
menu:Clear()
print("PLUGMENU " .. menu.ClassName .. " " .. menu.Title .. " " .. menu.Icon .. " " .. n .. " " .. added.ClassName .. " " .. added.Text .. " " .. added.ActionId .. " " .. sep.ClassName .. " " .. tostring(shown) .. " " .. #menu:GetChildren()
	.. " " .. b(not pcall(function() return menu.Visible end)))
print("PLUGACTION " .. act.ActionId .. " " .. act.Text .. " " .. act.StatusTip .. " " .. tostring(act.AllowBinding) .. " " .. b(not pcall(function() act.Text = "x" end)) .. " " .. b(not pcall(function() act.ActionId = "y" end)))
local toolbar = plugin:CreateToolbar("Data Tools")
local button = toolbar:CreateButton("data", "tip", "rbxasset://b.png", "Data")
print("PLUGBUTTON " .. button.ClassName .. " " .. button.Icon .. " " .. tostring(button.Enabled) .. " " .. tostring(button.ClickableWhenViewportHidden) .. " " .. typeof(button.IconContent) .. " " .. typeof(button.Click))
local mode, tool0 = plugin:GetJoinMode(), plugin:GetSelectedRibbonTool()
plugin:SelectRibbonTool(Enum.RibbonTool.Move, UDim2.new())
plugin:OpenWikiPage("Part"); plugin:SaveSelectedToRoblox(); plugin:StartDrag({Sender = "x", MimeType = "text/plain", Data = "d"})
print("PLUGMISC " .. tostring(mode) .. " " .. tostring(tool0) .. " " .. tostring(plugin:GetSelectedRibbonTool()) .. " " .. plugin:PromptForExistingAssetId("Model") .. " " .. plugin:PromptForExistingAssetIdAsync("Model")
	.. " " .. tostring(plugin:PromptSaveSelectionAsync()) .. " " .. b(not pcall(function() plugin:ImportFbxRigAsync() end)) .. " " .. b(not pcall(function() return plugin.IsDebuggable end)) .. " " .. typeof(plugin:GetMouse().DragEnter))
local info = DockWidgetPluginGuiInfo.new(Enum.InitialDockState.Float, true, false, 240, 120, 100, 60)
local dock = plugin:CreateDockWidgetPluginGuiAsync("DataDock", info)
dock:BindToClose(function() dock.Enabled = false end)
print("PLUGDOCK " .. dock.ClassName .. " " .. tostring(dock.HostWidgetWasRestored) .. " " .. b(not pcall(function() dock.HostWidgetWasRestored = true end)) .. " " .. typeof(dock:GetRelativeMousePosition())
	.. " " .. typeof(dock.PluginDragEntered) .. " " .. typeof(dock.PluginDragLeft) .. " " .. typeof(dock.PluginDragMoved))
local Selection = game:GetService("Selection")
local thick = Selection.SelectionThickness
Selection.SelectionThickness = 2
game:SetPlaceId(4242); game:SetUniverseId(77)
local jobs = game:GetJobsInfo()
local s = Instance.new("Script"); s.RunContext = Enum.RunContext.Client
print("PLUGDATA " .. thick .. " " .. Selection.SelectionThickness .. " " .. game.PlaceId .. " " .. game.GameId .. " " .. #jobs .. " " .. jobs[1].name .. " " .. tostring(s.RunContext))
""")

func _process(delta: float) -> bool:
	t += delta
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	if t < 25.0 and not (heard.call("DONE") and heard.call("PLUGDATA")):
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("ServiceProvider, GenericSettings and ValueBase sit in the chains, none creatable", heard.call("CHAIN tttt ttt ttt"), true)
	check("the new classes are declared; an Actor is a Model; StudioService answers", heard.call("CLASSES tttttt t t"), true)
	check("an Actor's message lands on the next pass, both bindings, none after a disconnect; GetActor", heard.call("ACTOR RBXScriptConnection 0 2 one:2,parone 2 true true nil"), true)
	check("BrickColorValue is Medium stone grey, takes a BrickColor, Changed carries one", heard.call("BRICKVAL BrickColor Medium stone grey Bright red BrickColor:Bright red"), true)
	check("RayValue is a zero Ray, takes a Ray, Changed and the Value signal fire once", heard.call("RAYVAL Ray 0, 0, 0 1, 2, 3 0, 1, 0 1 1 0, 1, 0 t"), true)
	check("BinaryStringValue is declared with its Changed", heard.call("BINVAL BinaryStringValue RBXScriptSignal"), true)
	check("Sandboxed reads false, takes a write; IsInSandbox reads the ancestors, read only; UniqueId is hidden", heard.call("SANDBOX false true true false t t"), true)
	check("IsPropertyModified against the default, ResetPropertyToDefault, GetStyled and the styled signal", heard.call("PROPMOD true false false 0 t t true RBXScriptSignal RBXScriptSignal"), true)
	check("the DataModel's creator, Workspace and RunService, the plugin-only setters refused, ServiceAdded", heard.call("DATAMODEL 0 Enum.CreatorType.User true true t t t t SharedTableRegistry RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal RBXScriptSignal"), true)
	check("RunContext sits on BaseScript: a LocalScript has it, a game script cannot set it", heard.call("RUNCTX Enum.RunContext.Legacy t RBXScriptSignal") and heard.call("LOCAL Enum.RunContext.Legacy"), true)
	check("one recording at a time; IsRecordingInProgress follows it", heard.call("HISTORY false edit nil true false"), true)
	check("Dialog and DialogChoice read their defaults", heard.call("DIALOG Enum.DialogBehaviorType.SinglePlayer 25 true [] false Enum.DialogPurpose.Help Enum.DialogTone.Neutral 0 0, 0, 0 0 RBXScriptSignal true []"), true)
	check("and take a write", heard.call("DIALOGW Enum.DialogBehaviorType.MultiplePlayers 10 false byehi true Enum.DialogPurpose.Shop Enum.DialogTone.Enemy 4 0, 1, 0 false abc"), true)
	check("a Feature from the place reads its defaults", heard.call("FEATURE Enum.NormalId.Right Enum.InOut.Center Enum.LeftRight.Center Enum.TopBottom.Center"), true)
	check("and takes a write", heard.call("FEATUREW Enum.NormalId.Top Enum.InOut.Edge Enum.LeftRight.Left Enum.TopBottom.Bottom"), true)
	check("StudioService reads its defaults; no user, no icon, the prompts decline", heard.call("STUDIOSVC nil false 1 0 false en-us false t 0 [] 0, 0 nil 0"), true)
	check("and takes a write", heard.call("STUDIOSVCW true 4 15 true fr-fr true"), true)
	check("UserGameSettings' readable members read their defaults, the engine's own refuse, the methods answer", heard.call("UGS 1 1 Enum.ComputerCameraMovementMode.Default Enum.ComputerMovementMode.Default Enum.TouchCameraMovementMode.Default Enum.TouchMovementMode.Default 0 0 ttt 1 false false boolean RBXScriptSignal RBXScriptSignal"), true)
	check("and take a write; an onboarding completes; no user feature exists", heard.call("UGSW 3 0.5 Enum.ComputerCameraMovementMode.Orbital Enum.ComputerMovementMode.ClickToMove Enum.TouchCameraMovementMode.Follow Enum.TouchMovementMode.DynamicThumbstick 30 5 true t"), true)
	check("UserSettings:Reset restores the defaults", heard.call("UGSRESET 1 Enum.ComputerMovementMode.Default 0"), true)
	check("the enums resolve", heard.call("ENUMS 1 9 1 4 5 2"), true)
	check("a PluginMenu holds its actions and menus, ShowAsync answers nil, Clear empties it", heard.call("PLUGMENU PluginMenu Tools rbxasset://icon.png 4 PluginAction Second act2 PluginAction nil 0 t"), true)
	check("CreatePluginAction fills the action; its fields are not a plugin's to write", heard.call("PLUGACTION act1 Do it Does the thing false t t"), true)
	check("the toolbar button is a PluginToolbarButton with an IconContent", heard.call("PLUGBUTTON PluginToolbarButton rbxasset://b.png true false Content RBXScriptSignal"), true)
	check("no joints, the ribbon tool is kept, the prompts decline, no FBX, IsDebuggable is the engine's", heard.call("PLUGMISC Enum.JointCreationMode.None Enum.RibbonTool.None Enum.RibbonTool.Move -1 -1 false t t RBXScriptSignal"), true)
	check("the dock's members: not restored, read only, a relative position, the drag events", heard.call("PLUGDOCK DockWidgetPluginGui false t Vector2 RBXScriptSignal RBXScriptSignal RBXScriptSignal"), true)
	check("SelectionThickness, the place and universe ids, the jobs header, RunContext for a plugin", heard.call("PLUGDATA 0 2 4242 77 1 name Enum.RunContext.Client"), true)
	var stray := 0
	for l in said:
		if l.find("ERROR") != -1:
			stray += 1
	check("no script error", stray, 0)
	for l in lines:
		print(l)
	if bad > 0:
		for l in said:
			print("DATAPAR said: " + l)
	print("DATAPAR %d passed, %d failed" % [ok, bad])
	print("data parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
