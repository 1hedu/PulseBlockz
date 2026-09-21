-- A LocalScript in a tool runs on its owner's client. Equipped hands us the
-- Mouse; a click fires Activated here first, then on the server.
local player = game:GetService("Players").LocalPlayer
local tool = script.Parent

tool.Equipped:Connect(function(mouse)
	print("wand in hand", mouse.ClassName, tool.Parent == player.Character)
end)
tool.Activated:Connect(function() print("wand activated") end)
tool.Unequipped:Connect(function() print("wand away") end)
