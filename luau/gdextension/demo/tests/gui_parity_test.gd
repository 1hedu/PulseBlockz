# The gui family: GuiService's selection, UserInputService's gamepad and motion answers,
# ContextActionService's tool events and button settings, UIPageLayout's paging, VideoFrame's
# Play / Pause, a WireframeHandleAdornment's lines, BillboardGui.CurrentDistance, ScreenInsets,
# GuiState and the pointer's secondary click and wheel, and the recorded-only members.
#
#   godot --headless --path . -s res://tests/gui_parity_test.gd
#
# A Play Solo world with one joined player. The client script does the work; this side drives
# the pointer onto its button through the world's own GUI input entry points.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var ok := 0
var bad := 0
var said: Array[String] = []
var lines: Array[String] = []
var stage := 0
var stageAt := 0.0
var btn := 0

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("GUIPAR %-58s %-10s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = true
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("StarterPlayer/StarterPlayerScripts/Gui.client.luau", """
local Players = game:GetService("Players")
local GuiService = game:GetService("GuiService")
local UIS = game:GetService("UserInputService")
local CAS = game:GetService("ContextActionService")
local p = Players.LocalPlayer
local char = p.Character or p.CharacterAdded:Wait()
task.wait(1.0)
local pg = p:WaitForChild("PlayerGui")

-- the class chain
local sg = Instance.new("ScreenGui")
sg.Name = "ParityGui"
sg.Parent = pg
local btn = Instance.new("TextButton")
btn.Name = "ParityBtn"
btn.Position = UDim2.new(0, 50, 0, 50)
btn.Size = UDim2.new(0, 200, 0, 100)
btn.Text = "Parity"
btn.Parent = sg
local list = Instance.new("UIListLayout")
print("ISA " .. tostring(sg:IsA("GuiBase")) .. " " .. tostring(pg:IsA("BasePlayerGui")) .. " " .. tostring(game:GetService("StarterGui"):IsA("BasePlayerGui"))
	.. " " .. tostring(list:IsA("UIGridStyleLayout")) .. " " .. tostring(list:IsA("UILayout")) .. " " .. tostring(list:IsA("UIComponent")))
local made = {}
for _, c in {"CanvasGroup", "VideoFrame", "UIFlexItem", "UIDragDetector", "UIShadow", "UIPageLayout", "UITableLayout", "BoxHandleAdornment",
             "ConeHandleAdornment", "CylinderHandleAdornment", "SphereHandleAdornment", "LineHandleAdornment", "ImageHandleAdornment",
             "PyramidHandleAdornment", "WireframeHandleAdornment", "Handles", "ArcHandles", "SelectionBox", "SelectionSphere", "Path2D"} do
	local okc, inst = pcall(Instance.new, c)
	table.insert(made, okc and inst.ClassName == c and "t" or "f")
end
print("NEWCLASSES " .. table.concat(made))
local box = Instance.new("SelectionBox")
print("ADORNISA " .. tostring(box:IsA("InstanceAdornment")) .. " " .. tostring(box:IsA("GuiBase3d")) .. " " .. tostring(box:IsA("GuiBase"))
	.. " " .. tostring(Instance.new("BoxHandleAdornment"):IsA("PVAdornment")) .. " " .. tostring(Instance.new("Handles"):IsA("PartAdornment")))
print("TEXTSERVICE " .. tostring(game:GetService("TextService").ClassName))

-- recorded only: defaults, then a write
local f = Instance.new("Frame")
print("FRAMESTYLE " .. tostring(f.Style) .. " " .. tostring(btn.Style) .. " " .. tostring(btn.HoverHapticEffect))
f.Style = Enum.FrameStyle.DropShadow
btn.Style = Enum.ButtonStyle.RobloxRoundButton
print("FRAMESTYLEW " .. tostring(f.Style) .. " " .. tostring(btn.Style))
print("GB2D " .. tostring(sg.AutoLocalize) .. " " .. tostring(sg.RootLocalizationTable) .. " " .. tostring(sg.SelectionBehaviorDown) .. " " .. tostring(sg.SelectionGroup))
sg.AutoLocalize = false
sg.SelectionBehaviorUp = Enum.SelectionBehavior.Stop
sg.SelectionGroup = true
print("GB2DW " .. tostring(sg.AutoLocalize) .. " " .. tostring(sg.SelectionBehaviorUp) .. " " .. tostring(sg.SelectionGroup))
print("GOREC " .. tostring(btn.GuiState) .. " " .. tostring(btn.SelectionOrder) .. " " .. tostring(btn.NextSelectionDown) .. " " .. tostring(btn.SelectionImageObject) .. " " .. tostring(btn.BorderMode))
btn.SelectionOrder = 3
btn.NextSelectionDown = f
btn.BorderMode = Enum.BorderMode.Inset
print("GORECW " .. tostring(btn.SelectionOrder) .. " " .. tostring(btn.NextSelectionDown == f) .. " " .. tostring(btn.BorderMode) .. " " .. tostring(pcall(function() btn.GuiState = Enum.GuiState.Press end)))
print("SCREENGUI " .. tostring(sg.ScreenInsets) .. " " .. tostring(sg.ClipToDeviceSafeArea) .. " " .. tostring(sg.SafeAreaCompatibility) .. " " .. tostring(sg.IgnoreGuiInset))
sg.ScreenInsets = Enum.ScreenInsets.None
print("SCREENGUIW " .. tostring(sg.ScreenInsets) .. " " .. tostring(sg.IgnoreGuiInset))
sg.ScreenInsets = Enum.ScreenInsets.DeviceSafeInsets
print("SCREENGUIW2 " .. tostring(sg.ScreenInsets) .. " " .. tostring(sg.IgnoreGuiInset))
sg.IgnoreGuiInset = true
print("SCREENGUIW3 " .. tostring(sg.ScreenInsets))
sg.IgnoreGuiInset = false
local tl = Instance.new("TextLabel")
tl.Text = "hello"
print("TEXTREC " .. tostring(tl.LocalizedText) .. " " .. tostring(tl.MaxVisibleGraphemes) .. " [" .. tostring(tl.OpenTypeFeatures) .. "] [" .. tostring(tl.OpenTypeFeaturesError) .. "] " .. tostring(tl.TextDirection))
tl.MaxVisibleGraphemes = 2
tl.OpenTypeFeatures = "smcp"
tl.TextDirection = Enum.TextDirection.RightToLeft
local tbox = Instance.new("TextBox")
print("TEXTRECW " .. tostring(tl.MaxVisibleGraphemes) .. " " .. tostring(tl.OpenTypeFeatures) .. " " .. tostring(tl.TextDirection) .. " " .. tostring(pcall(function() tl.LocalizedText = "x" end)) .. " " .. tostring(tbox.ShowNativeInput))
local il = Instance.new("ImageLabel")
il.SliceScale = 2
print("SLICE " .. tostring(Instance.new("ImageButton").SliceScale) .. " " .. tostring(il.SliceScale))
list.HorizontalFlex = Enum.UIFlexAlignment.SpaceBetween
list.Wraps = true
print("LIST " .. tostring(list.HorizontalFlex) .. " " .. tostring(list.VerticalFlex) .. " " .. tostring(list.ItemLineAlignment) .. " " .. tostring(list.Wraps))
local stroke = Instance.new("UIStroke")
stroke.ApplyStrokeMode = Enum.ApplyStrokeMode.Border
print("STROKE " .. tostring(stroke.ApplyStrokeMode) .. " " .. tostring(stroke.LineJoinMode) .. " " .. tostring(stroke.ZIndex))
local flex = Instance.new("UIFlexItem")
flex.FlexMode = Enum.UIFlexMode.Grow
flex.GrowRatio = 2
print("FLEX " .. tostring(flex.FlexMode) .. " " .. tostring(flex.GrowRatio) .. " " .. tostring(flex.ShrinkRatio) .. " " .. tostring(flex.ItemLineAlignment))
local drag = Instance.new("UIDragDetector")
print("DRAG " .. tostring(drag.DragStyle) .. " " .. tostring(drag.DragRelativity) .. " " .. tostring(drag.DragSpace) .. " " .. tostring(drag.ResponseStyle) .. " " .. tostring(drag.Enabled) .. " " .. tostring(drag.DragAxis) .. " " .. tostring(drag.BoundingBehavior) .. " " .. tostring(drag.CursorIconContent))
drag.DragStyle = Enum.UIDragDetectorDragStyle.Rotate
drag.Enabled = false
drag.MaxDragTranslation = UDim2.new(0, 10, 0, 20)
drag.CursorIcon = "rbxassetid://5"
print("DRAGW " .. tostring(drag.DragStyle) .. " " .. tostring(drag.Enabled) .. " " .. tostring(drag.MaxDragTranslation) .. " " .. tostring(drag.CursorIconContent))
local shadow = Instance.new("UIShadow")
shadow.Color = Color3.new(1, 0, 0)
shadow.Transparency = 0.5
print("SHADOW " .. tostring(shadow.Enabled) .. " " .. tostring(shadow.Color) .. " " .. tostring(shadow.Transparency) .. " " .. tostring(shadow.BlurRadius) .. " " .. tostring(shadow.ZIndex))
local tab = Instance.new("UITableLayout")
tab.MajorAxis = Enum.TableMajorAxis.ColumnMajor
print("TABLE " .. tostring(tab.FillEmptySpaceColumns) .. " " .. tostring(tab.MajorAxis) .. " " .. tostring(tab.Padding) .. " " .. tostring(tab.SortOrder) .. " " .. tostring(tab.FillDirection))
local cg = Instance.new("CanvasGroup")
cg.GroupTransparency = 0.25
print("CANVAS " .. tostring(cg.GroupColor3) .. " " .. tostring(cg.GroupTransparency) .. " " .. tostring(cg:IsA("GuiObject")))
local sgui = game:GetService("StarterGui")
print("STARTERGUI " .. tostring(sgui.ScreenOrientation) .. " " .. tostring(sgui.VirtualCursorMode) .. " " .. tostring(sgui.RtlTextSupport) .. " " .. tostring(pcall(function() sgui.ProcessUserInput = true end)))
sgui.VirtualCursorMode = Enum.VirtualCursorMode.Enabled
print("STARTERGUIW " .. tostring(sgui.VirtualCursorMode))
local ha = Instance.new("BoxHandleAdornment")
print("HANDLE " .. tostring(ha.Color3) .. " " .. tostring(ha.Transparency) .. " " .. tostring(ha.Visible) .. " " .. tostring(ha.Adornee) .. " " .. tostring(ha.AlwaysOnTop) .. " " .. tostring(ha.ZIndex) .. " " .. tostring(ha.Size) .. " " .. tostring(ha.AdornCullingMode) .. " " .. tostring(ha.SizeRelativeOffset))
ha.CFrame = CFrame.new(1, 2, 3)
ha.Adornee = char
ha.Size = Vector3.new(2, 3, 4)
ha.Visible = false
print("HANDLEW " .. tostring(ha.CFrame.Position) .. " " .. tostring(ha.Adornee == char) .. " " .. tostring(ha.Size) .. " " .. tostring(ha.Visible))
local cyl = Instance.new("CylinderHandleAdornment")
local cone = Instance.new("ConeHandleAdornment")
local line = Instance.new("LineHandleAdornment")
local img = Instance.new("ImageHandleAdornment")
img.Image = "rbxassetid://7"
print("SHAPES " .. tostring(cyl.Angle) .. " " .. tostring(cyl.Height) .. " " .. tostring(cyl.InnerRadius) .. " " .. tostring(cyl.Radius) .. " " .. tostring(cone.Height) .. " " .. tostring(cone.Radius)
	.. " " .. tostring(Instance.new("SphereHandleAdornment").Radius) .. " " .. tostring(line.Length) .. " " .. tostring(line.Thickness) .. " " .. tostring(img.Size) .. " " .. tostring(img.ImageContent))
local handles = Instance.new("Handles")
handles.Style = Enum.HandlesStyle.Movement
print("HANDLES " .. tostring(handles.Style) .. " " .. tostring(handles.Adornee) .. " " .. tostring(typeof(handles.MouseDrag)) .. " " .. tostring(typeof(ha.MouseEnter)))
print("SELBOX " .. tostring(box.LineThickness) .. " " .. tostring(box.SurfaceColor3) .. " " .. tostring(box.SurfaceTransparency) .. " " .. tostring(Instance.new("SelectionSphere").SurfaceTransparency))
local path = Instance.new("Path2D")
path.Closed = true
print("PATH2D " .. tostring(path.Closed) .. " " .. tostring(path.Thickness) .. " " .. tostring(path.Visible) .. " " .. tostring(path.ZIndex) .. " " .. tostring(path:IsA("GuiBase")))
local core = game:GetService("CoreGui")
print("COREGUI " .. tostring(core.Version) .. " " .. tostring(core:IsA("BasePlayerGui")) .. " " .. tostring(pcall(function() return core.SelectionImageObject end)))
print("UISREC " .. tostring(UIS.OnScreenKeyboardVisible) .. " " .. tostring(UIS.OnScreenKeyboardPosition) .. " " .. tostring(UIS.OnScreenKeyboardSize) .. " " .. tostring(UIS.PreferredInput) .. " " .. tostring(pcall(function() return UIS.TouchScreenEnabled end)))
print("GUISVCREC " .. tostring(GuiService.AutoSelectGuiEnabled) .. " " .. tostring(GuiService.CoreGuiNavigationEnabled) .. " " .. tostring(GuiService.GuiNavigationEnabled) .. " " .. tostring(GuiService.TouchControlsEnabled)
	.. " " .. tostring(GuiService.PreferredTextSize) .. " " .. tostring(GuiService.PreferredTransparency) .. " " .. tostring(GuiService.ReducedMotionEnabled) .. " " .. tostring(GuiService.SelectedObject))
GuiService.GuiNavigationEnabled = false
print("GUISVCRECW " .. tostring(GuiService.GuiNavigationEnabled))

-- GuiService: the inset, the flags, the selection
local tl0, br0 = GuiService:GetGuiInset()
print("INSET " .. tostring(tl0) .. " " .. tostring(br0))
print("FLAGS " .. tostring(GuiService:GetInspectMenuEnabled()) .. " " .. tostring(GuiService:GetEmotesMenuOpen()) .. " " .. tostring(GuiService:GetGameplayPausedNotificationEnabled()))
GuiService:SetInspectMenuEnabled(false)
GuiService:SetEmotesMenuOpen(true)
GuiService:SetGameplayPausedNotificationEnabled(false)
GuiService:CloseInspectMenu()
print("FLAGSW " .. tostring(GuiService:GetInspectMenuEnabled()) .. " " .. tostring(GuiService:GetEmotesMenuOpen()) .. " " .. tostring(GuiService:GetGameplayPausedNotificationEnabled()))
local other = Instance.new("TextButton")
other.Name = "Other"
other.SelectionOrder = 1
other.Parent = sg
local gained, lost, changed = {}, {}, {}
btn.SelectionGained:Connect(function() table.insert(gained, "btn") end)
other.SelectionGained:Connect(function() table.insert(gained, "other") end)
btn.SelectionLost:Connect(function() table.insert(lost, "btn") end)
other.SelectionLost:Connect(function() table.insert(lost, "other") end)
sg.SelectionChanged:Connect(function(mine, prev, new) table.insert(changed, tostring(mine) .. ":" .. tostring(prev and prev.Name) .. ">" .. tostring(new and new.Name)) end)
GuiService.SelectedObject = btn
GuiService.SelectedObject = other
GuiService.SelectedObject = nil
task.wait(0.1)
print("SELECT " .. table.concat(gained, ",") .. " " .. table.concat(lost, ",") .. " " .. tostring(GuiService.SelectedObject))
print("SELCHANGED " .. table.concat(changed, ","))
GuiService:Select(sg)   -- Other has the lower SelectionOrder
print("SELECTFN " .. tostring(GuiService.SelectedObject and GuiService.SelectedObject.Name))
GuiService.SelectedObject = nil
print("SELBAD " .. tostring(pcall(function() GuiService.SelectedObject = workspace end)))
local at = pg:GetGuiObjectsAtPosition(-500, -500)
print("GUIAT " .. tostring(typeof(at)) .. " " .. tostring(#at))

-- UserInputService: no gamepad, no sensor
print("GAMEPAD " .. tostring(#UIS:GetConnectedGamepads()) .. " " .. tostring(#UIS:GetNavigationGamepads()) .. " " .. tostring(UIS:GetGamepadConnected(Enum.UserInputType.Gamepad1))
	.. " " .. tostring(UIS:IsNavigationGamepad(Enum.UserInputType.Gamepad1)) .. " " .. tostring(UIS:GamepadSupports(Enum.UserInputType.Gamepad1, Enum.KeyCode.ButtonA))
	.. " " .. tostring(UIS:IsGamepadButtonDown(Enum.UserInputType.Gamepad1, Enum.KeyCode.ButtonA)) .. " " .. tostring(#UIS:GetSupportedGamepadKeyCodes(Enum.UserInputType.Gamepad1)) .. " " .. tostring(typeof(UIS:GetGamepadState(Enum.UserInputType.Gamepad1))))
UIS:SetNavigationGamepad(Enum.UserInputType.Gamepad1, true)
local grav = UIS:GetDeviceGravity()
local rotObj, rotCf = UIS:GetDeviceRotation()
print("MOTION " .. tostring(grav.ClassName) .. " " .. tostring(grav.UserInputType) .. " " .. tostring(rotObj.UserInputType) .. " " .. tostring(typeof(rotCf)) .. " " .. tostring(UIS:GetDeviceAcceleration().Position))
print("KEYSTR [" .. UIS:GetStringForKeyCode(Enum.KeyCode.E) .. "] [" .. UIS:GetStringForKeyCode(Enum.KeyCode.Semicolon) .. "] [" .. UIS:GetStringForKeyCode(Enum.KeyCode.Seven) .. "] [" .. UIS:GetStringForKeyCode(Enum.KeyCode.LeftShift) .. "]")
print("MOUSEBTNS " .. tostring(#UIS:GetMouseButtonsPressed()))
print("MODIFIER " .. tostring(grav:IsModifierKeyDown(Enum.ModifierKey.Shift)) .. " " .. tostring(grav:IsModifierKeyDown(Enum.ModifierKey.Ctrl)))
local lastTypes = {}
UIS.LastInputTypeChanged:Connect(function(t) table.insert(lastTypes, tostring(t)) end)

-- ContextActionService: the button settings, the bound info, the tool
CAS:BindAction("ParityAction", function() end, true, Enum.KeyCode.Q)
CAS:SetTitle("ParityAction", "Do it")
CAS:SetDescription("ParityAction", "Does it")
CAS:SetImage("ParityAction", "rbxassetid://9")
CAS:SetPosition("ParityAction", UDim2.new(0.5, 0, 0.5, 0))
local info = CAS:GetBoundActionInfo("ParityAction")
print("ACTIONINFO " .. tostring(info.title) .. " " .. tostring(info.description) .. " " .. tostring(info.image) .. " " .. tostring(info.position) .. " " .. tostring(#info.inputTypes) .. " " .. tostring(info.inputTypes[1]) .. " " .. tostring(CAS:GetButton("ParityAction")))
print("ACTIONNONE " .. tostring(next(CAS:GetBoundActionInfo("Nope"))))
CAS:BindActivate(Enum.UserInputType.Gamepad1, Enum.KeyCode.ButtonR2)
CAS:UnbindActivate(Enum.UserInputType.Gamepad1, Enum.KeyCode.ButtonR2)
print("TOOLICON0 " .. tostring(CAS:GetCurrentLocalToolIcon()))
local toolEvents = {}
CAS.LocalToolEquipped:Connect(function(tool) table.insert(toolEvents, "eq:" .. tool.Name) end)
CAS.LocalToolUnequipped:Connect(function(tool) table.insert(toolEvents, "uneq:" .. tool.Name) end)
local tool = Instance.new("Tool")
tool.Name = "ParityTool"
tool.TextureId = "rbxassetid://11"
local handle = Instance.new("Part")
handle.Name = "Handle"
handle.Parent = tool
tool.Parent = char
task.wait(0.1)
print("TOOLICON1 " .. tostring(CAS:GetCurrentLocalToolIcon()))
tool.Parent = nil
task.wait(0.1)
print("TOOLEVENTS " .. table.concat(toolEvents, ","))

-- ScrollingFrame: no inertia
local sf = Instance.new("ScrollingFrame")
sf:ResetScrollVelocity()
print("SCROLLVEL " .. tostring(sf:GetScrollVelocity()))

-- UIPageLayout: paging among the parent's GuiObjects
local pager = Instance.new("Frame")
for _, n in {"B", "A", "C"} do local pf = Instance.new("Frame"); pf.Name = n; pf.Parent = pager end
local pl = Instance.new("UIPageLayout")
pl.Parent = pager
local pageLog = {}
pl.PageEnter:Connect(function(pg2) table.insert(pageLog, "in:" .. pg2.Name) end)
pl.PageLeave:Connect(function(pg2) table.insert(pageLog, "out:" .. pg2.Name) end)
pl.Stopped:Connect(function(pg2) table.insert(pageLog, "stop:" .. pg2.Name) end)
print("PAGE0 " .. tostring(pl.CurrentPage) .. " " .. tostring(pl.Animated) .. " " .. tostring(pl.Circular) .. " " .. tostring(pl.EasingStyle) .. " " .. tostring(pl.EasingDirection) .. " " .. tostring(pl.TweenTime) .. " " .. tostring(pl.Padding) .. " " .. tostring(pl.SortOrder) .. " " .. tostring(pl.GamepadInputEnabled))
pl:JumpToIndex(0)
print("PAGE1 " .. tostring(pl.CurrentPage.Name))
pl:Next()
pl:Next()
print("PAGE2 " .. tostring(pl.CurrentPage.Name))
pl:Next()
print("PAGE3 " .. tostring(pl.CurrentPage.Name))
pl.Circular = true
pl:Next()
print("PAGE4 " .. tostring(pl.CurrentPage.Name))
pl:Previous()
pl:JumpTo(pager.B)
print("PAGE5 " .. tostring(pl.CurrentPage.Name))
print("PAGELOG " .. table.concat(pageLog, ","))
print("PAGERO " .. tostring(pcall(function() pl.CurrentPage = pager.A end)))

-- VideoFrame: Play / Pause and the flags
local vf = Instance.new("VideoFrame")
local vlog = {}
vf.Played:Connect(function() table.insert(vlog, "played") end)
vf.Paused:Connect(function() table.insert(vlog, "paused") end)
print("VIDEO0 " .. tostring(vf.Playing) .. " " .. tostring(vf.IsLoaded) .. " " .. tostring(vf.Looped) .. " " .. tostring(vf.Volume) .. " " .. tostring(vf.TimePosition) .. " " .. tostring(vf.TimeLength) .. " " .. tostring(vf.Resolution) .. " " .. tostring(vf.RollOffMode) .. " " .. tostring(vf.RollOffMaxDistance) .. " " .. tostring(vf.VideoContent))
vf.Video = "rbxassetid://13"
vf.Looped = true
vf.Volume = 0.5
vf:Play()
vf:Play()
vf:Pause()
task.wait(0.1)
print("VIDEO1 " .. tostring(vf.Playing) .. " " .. tostring(vf.Looped) .. " " .. tostring(vf.Volume) .. " " .. tostring(vf.VideoContent) .. " " .. table.concat(vlog, ",") .. " " .. tostring(pcall(function() vf.IsLoaded = true end)))

-- WireframeHandleAdornment: lines kept, then cleared
local wf = Instance.new("WireframeHandleAdornment")
wf:AddLine(Vector3.new(0, 0, 0), Vector3.new(1, 0, 0))
wf:AddLines({Vector3.new(0, 0, 0), Vector3.new(0, 1, 0), Vector3.new(0, 0, 0), Vector3.new(0, 0, 1)})
wf:AddPath({Vector3.new(0, 0, 0), Vector3.new(1, 1, 1), Vector3.new(2, 2, 2)}, true)
wf:Clear()
print("WIREFRAME " .. tostring(wf.Scale) .. " " .. tostring(wf.Thickness))

-- BillboardGui.CurrentDistance: from a scriptable camera to the adornee
local cam = workspace.CurrentCamera
cam.CameraType = Enum.CameraType.Scriptable
cam.CFrame = CFrame.new(100, 10, 100)
local target = Instance.new("Part")
target.Anchored = true
target.Position = Vector3.new(100, 10, 130)
target.Parent = workspace
local bb = Instance.new("BillboardGui")
bb.Adornee = target
bb.Parent = pg
task.wait(0.2)
print("BBDIST " .. tostring(bb.CurrentDistance) .. " " .. tostring(pcall(function() bb.CurrentDistance = 1 end)))

-- the pointer, driven from outside: hover, a secondary click, the wheel
local secondary, wheelF, wheelB = 0, 0, 0
btn.SecondaryActivated:Connect(function(io, n) secondary += 1 end)
btn.MouseWheelForward:Connect(function() wheelF += 1 end)
btn.MouseWheelBackward:Connect(function() wheelB += 1 end)
print("GUIREADY")
task.wait(1.0)
print("GSTATE1 " .. tostring(btn.GuiState))
task.wait(2.0)
print("GSTATE2 " .. tostring(btn.GuiState) .. " " .. tostring(secondary) .. " " .. tostring(wheelF) .. " " .. tostring(wheelB))
btn.Interactable = false
print("LASTTYPES " .. table.concat(lastTypes, ","))
print("DONE")
""")

func _id(named: String) -> int:
	var stack: Array[int] = [0]
	while not stack.is_empty():
		var at: int = stack.pop_back()
		for kid in world.get_child_ids(at):
			if world.get_instance(kid).name == named:
				return kid
			stack.append(kid)
	return 0

func _button(pressed: bool, index: MouseButton) -> void:
	var ev := InputEventMouseButton.new()
	ev.button_index = index
	ev.pressed = pressed
	ev.factor = 1.0
	ev.position = Vector2(10, 10)
	ev.global_position = Vector2(10, 10)
	world._on_gui_input(ev, btn)

func _process(delta: float) -> bool:
	t += delta
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	# the pointer's stages, timed from GUIREADY: hover, then a right press, then its release and the wheel
	if stage == 0 and heard.call("GUIREADY"):
		btn = _id("ParityBtn")
		stage = 1
		stageAt = t
	elif stage == 1 and t - stageAt > 0.2:
		world._on_gui_mouse(true, btn)
		stage = 2
	elif stage == 2 and t - stageAt > 0.5:
		_button(true, MOUSE_BUTTON_RIGHT)
		stage = 3
	elif stage == 3 and t - stageAt > 1.8:
		_button(false, MOUSE_BUTTON_RIGHT)
		_button(true, MOUSE_BUTTON_WHEEL_UP)
		_button(false, MOUSE_BUTTON_WHEEL_UP)
		_button(true, MOUSE_BUTTON_WHEEL_DOWN)
		_button(false, MOUSE_BUTTON_WHEEL_DOWN)
		stage = 4
	if t < 16.0 and not heard.call("DONE"):
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("GuiBase, BasePlayerGui, UILayout and UIGridStyleLayout sit in the chain", heard.call("ISA true true true true true true"), true)
	check("the absent classes are declared", heard.call("NEWCLASSES tttttttttttttttttttt"), true)
	check("the adornment chain", heard.call("ADORNISA true true true true true"), true)
	check("TextService answers GetService", heard.call("TEXTSERVICE TextService"), true)
	check("Frame.Style, GuiButton.Style and the haptic refs read their defaults", heard.call("FRAMESTYLE Enum.FrameStyle.Custom Enum.ButtonStyle.Custom nil"), true)
	check("and take a write", heard.call("FRAMESTYLEW Enum.FrameStyle.DropShadow Enum.ButtonStyle.RobloxRoundButton"), true)
	check("GuiBase2d's recorded members read their defaults", heard.call("GB2D true nil Enum.SelectionBehavior.Escape false"), true)
	check("and take a write", heard.call("GB2DW false Enum.SelectionBehavior.Stop true"), true)
	check("GuiObject's recorded members read their defaults", heard.call("GOREC Enum.GuiState.Idle 0 nil nil Enum.BorderMode.Outline"), true)
	check("and take a write; GuiState is read only", heard.call("GORECW 3 true Enum.BorderMode.Inset false"), true)
	check("ScreenGui's new members read their defaults", heard.call("SCREENGUI Enum.ScreenInsets.CoreUISafeInsets true Enum.SafeAreaCompatibility.FullscreenExtension false"), true)
	check("ScreenInsets None sets IgnoreGuiInset", heard.call("SCREENGUIW Enum.ScreenInsets.None true"), true)
	check("another inset clears it", heard.call("SCREENGUIW2 Enum.ScreenInsets.DeviceSafeInsets false"), true)
	check("IgnoreGuiInset reads back as None", heard.call("SCREENGUIW3 Enum.ScreenInsets.None"), true)
	check("LocalizedText is the Text; the text members read their defaults", heard.call("TEXTREC hello -1 [] [] Enum.TextDirection.Auto"), true)
	check("and take a write; LocalizedText is read only", heard.call("TEXTRECW 2 smcp Enum.TextDirection.RightToLeft false true"), true)
	check("SliceScale reads 1 and takes a write", heard.call("SLICE 1 2"), true)
	check("UIListLayout's flex members", heard.call("LIST Enum.UIFlexAlignment.SpaceBetween Enum.UIFlexAlignment.None Enum.ItemLineAlignment.Automatic true"), true)
	check("UIStroke's mode, join and ZIndex", heard.call("STROKE Enum.ApplyStrokeMode.Border Enum.LineJoinMode.Round 0"), true)
	check("UIFlexItem", heard.call("FLEX Enum.UIFlexMode.Grow 2 0 Enum.ItemLineAlignment.Automatic"), true)
	check("UIDragDetector reads its defaults", heard.call("DRAG Enum.UIDragDetectorDragStyle.TranslatePlane Enum.UIDragDetectorDragRelativity.Relative Enum.UIDragDetectorDragSpace.Parent Enum.UIDragDetectorResponseStyle.Offset true 1, 0 Enum.UIDragDetectorBoundingBehavior.Automatic "), true)
	check("and takes a write, the icon through its Content twin", heard.call("DRAGW Enum.UIDragDetectorDragStyle.Rotate false {0, 10}, {0, 20} rbxassetid://5"), true)
	check("UIShadow", heard.call("SHADOW true 1, 0, 0 0.5 0 0"), true)
	check("UITableLayout", heard.call("TABLE false Enum.TableMajorAxis.ColumnMajor {0, 0}, {0, 0} Enum.SortOrder.Name Enum.FillDirection.Horizontal"), true)
	check("CanvasGroup is a GuiObject with its group tint", heard.call("CANVAS 1, 1, 1 0.25 true"), true)
	check("StarterGui's recorded members; ProcessUserInput is a plugin's", heard.call("STARTERGUI Enum.ScreenOrientation.LandscapeSensor Enum.VirtualCursorMode.Default Enum.RtlTextSupport.Default false"), true)
	check("and take a write", heard.call("STARTERGUIW Enum.VirtualCursorMode.Enabled"), true)
	check("a HandleAdornment reads its defaults", heard.call("HANDLE 0.0509804, 0.411765, 0.67451 0 true nil false -1 1, 1, 1 Enum.AdornCullingMode.Automatic 0, 0, 0"), true)
	check("and takes CFrame, Adornee, Size and Visible", heard.call("HANDLEW 1, 2, 3 true 2, 3, 4 false"), true)
	check("the shaped adornments' figures", heard.call("SHAPES 360 1 0 1 2 0.5 1 5 1 1, 1 rbxassetid://7"), true)
	check("Handles.Style, Adornee and the mouse events", heard.call("HANDLES Enum.HandlesStyle.Movement nil RBXScriptSignal RBXScriptSignal"), true)
	check("SelectionBox and SelectionSphere", heard.call("SELBOX 0.15 0.0509804, 0.411765, 0.67451 1 1"), true)
	check("Path2D", heard.call("PATH2D true 1 true 1 true"), true)
	check("CoreGui's Version and base; SelectionImageObject is the engine's", heard.call("COREGUI 0 true false"), true)
	check("UserInputService's on-screen keyboard and PreferredInput; TouchScreenEnabled is the engine's", heard.call("UISREC false 0, 0 0, 0 Enum.PreferredInput.KeyboardAndMouse false"), true)
	check("GuiService's recorded members read their defaults", heard.call("GUISVCREC true true true true Enum.PreferredTextSize.Medium 1 false nil"), true)
	check("and take a write", heard.call("GUISVCRECW false"), true)
	check("GetGuiInset is the topbar gap", heard.call("INSET 0, 36 0, 0"), true)
	check("the inspect, emotes and pause flags read their defaults", heard.call("FLAGS true false true"), true)
	check("and follow their setters", heard.call("FLAGSW false true false"), true)
	check("SelectedObject: gained, lost, then nil", heard.call("SELECT btn,other btn,other nil"), true)
	check("SelectionChanged up the ScreenGui, with amISelected", heard.call("SELCHANGED true:nil>ParityBtn,true:ParityBtn>Other,false:Other>nil"), true)
	check("Select picks the lowest SelectionOrder", heard.call("SELECTFN Other"), true)
	check("SelectedObject refuses a non-GuiObject", heard.call("SELBAD false"), true)
	check("GetGuiObjectsAtPosition answers a table, empty off screen", heard.call("GUIAT table 0"), true)
	check("no gamepad is connected, supported or down", heard.call("GAMEPAD 0 0 false false false false 0 table"), true)
	check("the motion readings are InputObjects at rest", heard.call("MOTION InputObject Enum.UserInputType.Accelerometer Enum.UserInputType.Gyro CFrame 0, 0, 0"), true)
	check("GetStringForKeyCode gives the printable key", heard.call("KEYSTR [E] [;] [7] []"), true)
	check("no mouse button is held", heard.call("MOUSEBTNS 0"), true)
	check("no modifier is down", heard.call("MODIFIER false false"), true)
	check("the action's title, description, image, position and inputs read back", heard.call("ACTIONINFO Do it Does it rbxassetid://9 {0.5, 0}, {0.5, 0} 1 Enum.KeyCode.Q nil"), true)
	check("an unbound action has no info", heard.call("ACTIONNONE nil"), true)
	check("no tool held: no icon", heard.call("TOOLICON0 nil"), true)
	check("the held tool's TextureId is the icon", heard.call("TOOLICON1 rbxassetid://11"), true)
	check("LocalToolEquipped and LocalToolUnequipped fire", heard.call("TOOLEVENTS eq:ParityTool,uneq:ParityTool"), true)
	check("a ScrollingFrame has no velocity", heard.call("SCROLLVEL 0, 0"), true)
	check("UIPageLayout reads its defaults", heard.call("PAGE0 nil true false Enum.EasingStyle.Back Enum.EasingDirection.Out 1 0, 0 Enum.SortOrder.Name true"), true)
	check("JumpToIndex 0 is the first page by Name", heard.call("PAGE1 A"), true)
	check("Next twice reaches the last", heard.call("PAGE2 C"), true)
	check("Next at the end stays", heard.call("PAGE3 C"), true)
	check("Circular wraps to the first", heard.call("PAGE4 A"), true)
	check("Previous wraps back, JumpTo lands on the page", heard.call("PAGE5 B"), true)
	check("PageLeave, PageEnter and Stopped in order", heard.call("PAGELOG in:A,stop:A,out:A,in:B,stop:B,out:B,in:C,stop:C,out:C,in:A,stop:A,out:A,in:C,stop:C,out:C,in:B,stop:B"), true)
	check("CurrentPage is read only", heard.call("PAGERO false"), true)
	check("VideoFrame reads its defaults", heard.call("VIDEO0 false false false 1 0 0 0, 0 Enum.RollOffMode.Inverse 10000 "), true)
	check("Play and Pause set Playing and fire once each; IsLoaded is read only", heard.call("VIDEO1 false true 0.5 rbxassetid://13 played,paused false"), true)
	check("a WireframeHandleAdornment takes lines and clears", heard.call("WIREFRAME 1, 1, 1 1"), true)
	check("CurrentDistance is camera to adornee, read only", heard.call("BBDIST 30 false"), true)
	check("GuiState is Press while the button is held", heard.call("GSTATE1 Enum.GuiState.Press"), true)
	check("then Hover; SecondaryActivated and the wheel events fired", heard.call("GSTATE2 Enum.GuiState.Hover 1 1 1"), true)
	check("no script error", heard.call("ERROR"), false)
	for l in lines:
		print(l)
	print("GUIPAR %d passed, %d failed" % [ok, bad])
	print("gui parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
