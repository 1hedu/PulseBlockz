-- A Script under Workspace runs on the server. src/map is mounted as
-- Workspace.Map (default.project.json), so this is workspace.Map.Build.
--
-- The map itself is data: every part beside this file is a *.model.json (and
-- Shrine is an .rbxmx saved out of Studio), so the Studio can open the place
-- and move a brick without editing code. What is left here is the behaviour --
-- what the map does when someone clicks, opens, or falls into something --
-- which is the half a file cannot hold. Every model is in the tree before any
-- script starts, so they can be reached by name straight away.
local map = script.Parent

-- A sign with a ClickDetector: clicking it (on any client) fires MouseClick
-- here on the server with the Player who clicked. The BillboardGui over it is
-- drawn through the camera and shrinks with distance.
local sign = map.Sign
local count = sign.Tag.Count
local clicks = 0
sign.ClickDetector.MouseClick:Connect(function(player)
	sign.Color = if sign.Color == Color3.fromRGB(245, 205, 48) then Color3.fromRGB(75, 151, 75) else Color3.fromRGB(245, 205, 48)
	clicks += 1
	count.Text = "Clicks: " .. clicks
	print("sign clicked by", player.Name)
end)

-- A chest with a ProximityPrompt: walk up to it and press F. The client shows
-- the prompt; Triggered fires there and here, with the Player. Its creak is a
-- Sound in the part, the file loaded from the project.
local chest = map.Chest
local creak = chest.Creak
creak.Loaded:Connect(function() print("chest sound loaded", creak.TimeLength) end)
creak.Ended:Connect(function() print("chest sound ended") end)
chest.Open.Triggered:Connect(function(player)
	chest.Color = if chest.Color == Color3.fromRGB(120, 80, 40) then Color3.fromRGB(240, 200, 60) else Color3.fromRGB(120, 80, 40)
	creak:Play()
	print("chest opened by", player.Name)
end)

-- Lava kills whatever touches it: a character dies (and respawns), a part is destroyed.
map.Lava.Touched:Connect(function(hit)
	if hit.Anchored then return end
	local humanoid = hit.Parent:FindFirstChildOfClass("Humanoid")
	if humanoid then
		if humanoid.Health > 0 then print("lava got", hit.Parent.Name) end
		humanoid.Health = 0
		return
	end
	print("lava got", hit:GetFullName())
	task.wait(0.3)
	hit:Destroy()
end)

print("map built:", #map:GetChildren(), "parts")
