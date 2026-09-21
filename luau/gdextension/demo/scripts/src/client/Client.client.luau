-- src/client is StarterPlayer.StarterPlayerScripts (default.project.json), so
-- this is a LocalScript: copied into every player's PlayerScripts, runs on the
-- client.
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local Palette = require(ReplicatedStorage:WaitForChild("Palette"))

local player = Players.LocalPlayer
print("client up;", RunService:IsClient() and "client" or "server", "- I am", player.Name)

local drop = ReplicatedStorage:WaitForChild("DropBrick")
local waveEvent = ReplicatedStorage:WaitForChild("Wave")
local rng = Random.new()

-- Q waves: the server plays the animation on our character, so everyone sees it.
game:GetService("UserInputService").InputBegan:Connect(function(input, typing)
	if not typing and input.KeyCode == Enum.KeyCode.Q then waveEvent:FireServer() end
end)

-- Every few seconds ask the server for a brick; only the server can make
-- things everyone sees. (Spawning it here would make a client-only part.)
task.spawn(function()
	while true do
		task.wait(4)
		drop:FireServer(Palette.random(rng))
	end
end)

-- The camera is ours: workspace.CurrentCamera exists only on the client.
local camera = workspace.CurrentCamera
print("camera", camera and camera:GetFullName() or "none", "type", camera and camera.CameraType.Name or "?")

local function onCharacter(character)
	local humanoid = character:WaitForChild("Humanoid")
	print("my character is here, walk speed", humanoid.WalkSpeed)
	humanoid.Died:Connect(function() print("ouch") end)
end
if player.Character then onCharacter(player.Character) end
player.CharacterAdded:Connect(onCharacter)

-- The escape menu (Escape, or the top bar's button): GuiService says when it is up.
local guiService = game:GetService("GuiService")
guiService.MenuOpened:Connect(function() print("menu", "opened", guiService.MenuIsOpen) end)
guiService.MenuClosed:Connect(function() print("menu", "closed", guiService.MenuIsOpen) end)

-- The mouse: Player:GetMouse() gives Target / Hit through the camera.
local mouse = player:GetMouse()
mouse.Button1Down:Connect(function()
	print("mouse target", mouse.Target and mouse.Target.Name or "nothing", mouse.X, mouse.Y)
end)

-- A client-side visual: the Glass wall tints only for us.
local map = workspace:WaitForChild("Map")
local glass = map:WaitForChild("Glass")
glass.Color = Color3.fromRGB(255, 200, 120)

-- Input arrives here, on the client. ContextActionService sees a key first (E
-- asks for a brick right away), then UserInputService, with gameProcessedEvent
-- true when an action took it.
local UserInputService = game:GetService("UserInputService")
local ContextActionService = game:GetService("ContextActionService")
ContextActionService:BindAction("DropBrick", function(name, state, input)
	if state == Enum.UserInputState.Begin then
		print("use", name, input.KeyCode.Name)
		drop:FireServer(Palette.random(rng))
	end
end, false, Enum.KeyCode.E)
UserInputService.InputBegan:Connect(function(input, gameProcessed)
	if input.UserInputType == Enum.UserInputType.Keyboard then print("key", input.KeyCode.Name, gameProcessed) end
end)
UserInputService.JumpRequest:Connect(function() print("jump requested") end)

-- ProximityPrompts show up on the client as the character nears them.
local ProximityPromptService = game:GetService("ProximityPromptService")
ProximityPromptService.PromptShown:Connect(function(prompt, inputType) print("prompt shown", prompt.Name, inputType.Name) end)
ProximityPromptService.PromptHidden:Connect(function(prompt) print("prompt hidden", prompt.Name) end)
ProximityPromptService.PromptTriggered:Connect(function(prompt, who) print("prompt triggered", prompt.Name, who.Name) end)
-- The chest's Sound: the server's Play() reaches this side as Played.
task.spawn(function()
	local creak = workspace:WaitForChild("Map"):WaitForChild("Chest"):WaitForChild("Creak")
	creak.Played:Connect(function(id) print("heard", id) end)
end)

-- A HUD, built the way most Roblox games build theirs: Instances parented to
-- PlayerGui. The engine draws it; a click on the button comes back as Activated.
local playerGui = player:WaitForChild("PlayerGui")
local hud = Instance.new("ScreenGui")
hud.Name = "Hud"
hud.ResetOnSpawn = false
local panel = Instance.new("Frame", hud)
panel.Name = "Panel"
panel.AnchorPoint = Vector2.new(0, 1)
panel.Position = UDim2.new(0, 24, 1, -24)   -- bottom left: the hotbar has the bottom middle
panel.Size = UDim2.new(0, 240, 0, 136)
panel.BackgroundColor3 = Color3.fromRGB(30, 30, 40)
panel.BackgroundTransparency = 0.2
panel.BorderSizePixel = 0
Instance.new("UICorner", panel).CornerRadius = UDim.new(0, 12)
local list = Instance.new("UIListLayout", panel)
list.Padding = UDim.new(0, 8)
list.HorizontalAlignment = Enum.HorizontalAlignment.Center
list.VerticalAlignment = Enum.VerticalAlignment.Center
list.SortOrder = Enum.SortOrder.LayoutOrder
local counter = Instance.new("TextLabel", panel)
counter.Name = "Counter"
counter.LayoutOrder = 1
counter.Size = UDim2.new(1, 0, 0, 28)
counter.BackgroundTransparency = 1
counter.TextColor3 = Color3.new(1, 1, 1)
counter.TextSize = 20
counter.Text = "Bricks dropped: 0"
local button = Instance.new("TextButton", panel)
button.Name = "Drop"
button.LayoutOrder = 2
button.Size = UDim2.new(0, 160, 0, 40)
button.BackgroundColor3 = Color3.fromRGB(80, 160, 255)
button.TextColor3 = Color3.new(1, 1, 1)
button.TextSize = 18
button.Text = "Drop a brick"
Instance.new("UICorner", button)
local nameBox = Instance.new("TextBox", panel)
nameBox.Name = "NameBox"
nameBox.LayoutOrder = 3
nameBox.Size = UDim2.new(0, 200, 0, 28)
nameBox.BackgroundColor3 = Color3.fromRGB(240, 240, 240)
nameBox.TextColor3 = Color3.fromRGB(20, 20, 20)
nameBox.PlaceholderText = "Name the next brick"
nameBox.TextSize = 16
nameBox.Text = ""
Instance.new("UICorner", nameBox)
local brickName = ""
nameBox.Focused:Connect(function() print("box", "focused", nameBox.CursorPosition) end)
nameBox.FocusLost:Connect(function(enter)
	print("box", "lost", nameBox.Text, enter, nameBox.CursorPosition)
	if enter then brickName = nameBox.Text end
end)
-- A MultiLine TextBox: Enter puts in a newline instead of letting the focus go.
local note = Instance.new("TextBox", panel)
note.Name = "NoteBox"
note.LayoutOrder = 4
note.Size = UDim2.new(0, 200, 0, 56)
note.BackgroundColor3 = Color3.fromRGB(240, 240, 240)
note.TextColor3 = Color3.fromRGB(20, 20, 20)
note.PlaceholderText = "A note, in as many lines as you like"
note.TextSize = 16
note.MultiLine = true
note.TextWrapped = true
note.Text = ""
Instance.new("UICorner", note)
note:GetPropertyChangedSignal("Text"):Connect(function()
	local lines = 1
	for _ in string.gmatch(note.Text, "\n") do lines += 1 end
	print("note", (note.Text:gsub("\n", "|")), lines, note.CursorPosition)
end)
note.FocusLost:Connect(function(enter) print("note", "lost", enter) end)

local dropped = 0
button.Activated:Connect(function(input)
	dropped += 1
	counter.Text = "Bricks dropped: " .. dropped
	print("hud", "Activated", input.UserInputType.Name, dropped)
	print("hudsize", hud.AbsoluteSize.X, hud.AbsoluteSize.Y, button.AbsoluteSize.X, button.AbsoluteSize.Y)
	drop:FireServer(Palette.random(rng))
end)
-- A second ScreenGui with ZIndexBehavior Global: there a child's ZIndex is weighed
-- against every other object on the gui, not just against its siblings.
local layers = Instance.new("ScreenGui")
layers.Name = "Layers"
layers.ZIndexBehavior = Enum.ZIndexBehavior.Global
layers.Parent = playerGui
local under = Instance.new("Frame", layers)
under.Name = "Under"
under.Position = UDim2.new(0, 300, 0, 24)
under.Size = UDim2.new(0, 80, 0, 80)
under.ZIndex = 4
under.BackgroundColor3 = Color3.fromRGB(255, 120, 0)
local over = Instance.new("Frame", under)
over.Name = "Over"
over.Position = UDim2.new(0, 40, 0, 40)
over.Size = UDim2.new(0, 80, 0, 80)
over.ZIndex = 9
over.BackgroundColor3 = Color3.fromRGB(0, 120, 255)

-- A palette of swatches in a ScrollingFrame: a UIGridLayout lays the buttons
-- out in cells, and the frame scrolls once they run past its window.
local swatches = Instance.new("ScrollingFrame", hud)
swatches.Name = "Swatches"
swatches.AnchorPoint = Vector2.new(1, 0)
swatches.Position = UDim2.new(1, -24, 0, 24)
swatches.Size = UDim2.new(0, 160, 0, 120)
swatches.BackgroundColor3 = Color3.fromRGB(30, 30, 40)
swatches.BackgroundTransparency = 0.2
swatches.BorderSizePixel = 0
swatches.AutomaticCanvasSize = Enum.AutomaticSize.Y
swatches.ScrollingDirection = Enum.ScrollingDirection.Y
swatches.ScrollBarThickness = 8
swatches.ScrollBarImageColor3 = Color3.new(1, 1, 1)
local grid = Instance.new("UIGridLayout", swatches)
grid.CellSize = UDim2.fromOffset(40, 40)
grid.CellPadding = UDim2.fromOffset(4, 4)
grid.SortOrder = Enum.SortOrder.LayoutOrder
for i = 1, 12 do
	local color = Palette.Colors[(i - 1) % #Palette.Colors + 1]
	if i > #Palette.Colors then color = color:Lerp(Color3.new(1, 1, 1), 0.4) end
	local swatch = Instance.new("TextButton", swatches)
	swatch.Name = "Swatch" .. i
	swatch.LayoutOrder = i
	swatch.BackgroundColor3 = color
	swatch.Text = ""
	Instance.new("UICorner", swatch)
	swatch.Activated:Connect(function()
		print("swatch", i)
		drop:FireServer(color)
	end)
end
swatches:GetPropertyChangedSignal("CanvasPosition"):Connect(function() print("scrolled", swatches.CanvasPosition) end)
-- A badge kept in shape by a UIAspectRatioConstraint, no wider than a
-- UISizeConstraint allows, drawn at half size by a UIScale.
local badge = Instance.new("TextLabel", hud)
badge.Name = "Badge"
badge.Position = UDim2.new(0, 24, 0, 24)
badge.Size = UDim2.new(0.2, 0, 0.2, 0)
badge.BackgroundColor3 = Color3.fromRGB(30, 30, 40)
badge.BackgroundTransparency = 0.2
badge.BorderSizePixel = 0
badge.TextColor3 = Color3.new(1, 1, 1)
badge.TextScaled = true
badge.Text = "PulseBlockz"
Instance.new("UIAspectRatioConstraint", badge).AspectRatio = 3
Instance.new("UISizeConstraint", badge).MaxSize = Vector2.new(180, math.huge)
Instance.new("UITextSizeConstraint", badge).MaxTextSize = 24
Instance.new("UIScale", badge).Scale = 0.5
-- Images from the project: a sprite off images/sheet.png in an ImageLabel, and an
-- ImageButton (the ring sprite) that swaps the pointer for images/cursor.png.
local logo = Instance.new("ImageLabel", hud)
logo.Name = "Logo"
logo.Position = UDim2.new(0, 120, 0, 24)
logo.Size = UDim2.new(0, 48, 0, 30)
logo.BackgroundTransparency = 1
logo.Image = "images/sheet.png"
logo.ImageRectOffset = Vector2.new(0, 0)
logo.ImageRectSize = Vector2.new(16, 16)
logo.ScaleType = Enum.ScaleType.Fit
logo.ResampleMode = Enum.ResamplerMode.Pixelated
local aim = Instance.new("ImageButton", hud)
aim.Name = "Aim"
aim.Position = UDim2.new(0, 176, 0, 24)
aim.Size = UDim2.new(0, 30, 0, 30)
aim.BackgroundColor3 = Color3.fromRGB(30, 30, 40)
aim.BackgroundTransparency = 0.2
aim.BorderSizePixel = 0
aim.Image = "images/sheet.png"
aim.ImageRectOffset = Vector2.new(16, 0)
aim.ImageRectSize = Vector2.new(16, 16)
aim.ImageColor3 = Color3.fromRGB(180, 180, 180)
aim.HoverImage = "images/cursor.png"
Instance.new("UICorner", aim)
local mouse = player:GetMouse()
aim.Activated:Connect(function()
	mouse.Icon = mouse.Icon == "" and "images/cursor.png" or ""
	aim.ImageColor3 = mouse.Icon ~= "" and Color3.new(1, 1, 1) or Color3.fromRGB(180, 180, 180)
	print("aim", mouse.Icon)
end)
hud.Parent = playerGui
-- The Board's SurfaceGui (workspace.Map.Board.Screen) is everyone's; its button
-- fires here, on the client that pressed it. The label is ours to change.
task.spawn(function()
	local screen = workspace.Map:WaitForChild("Board"):WaitForChild("Screen")
	local presses = 0
	screen.Press.Activated:Connect(function()
		presses += 1
		screen.Title.Text = "Pressed: " .. presses
		print("board pressed", presses)
	end)
end)

task.delay(1, function()
	print("swatches", grid.AbsoluteCellCount, grid.AbsoluteCellSize, swatches.AbsoluteCanvasSize, swatches.AbsoluteWindowSize)
	print("badge", badge.AbsoluteSize, badge.AbsolutePosition)
	print("logo", logo.IsLoaded, logo.ContentImageSize, aim.IsLoaded)
	print("pyramid", workspace.Map:WaitForChild("Pyramid").MeshSize)
	swatches.CanvasPosition = Vector2.new(0, 999)   -- to the end: the engine clamps it
end)
