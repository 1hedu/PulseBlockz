-- The Wand: src/tools/Wand is a Tool (init.meta.json) with a Handle and a Tip
-- (*.model.json). StarterPack copies it into every character's Backpack, where
-- this Script runs on the server. 1 equips it (the engine hangs it off the
-- right arm), a click fires Activated here and on the client, Backspace drops
-- it into the Workspace.
local Players = game:GetService("Players")
local Debris = game:GetService("Debris")
local tool = script.Parent
local tip = tool:WaitForChild("Tip")

local function owner()
	local character = tool.Parent
	return Players:GetPlayerFromCharacter(character) or character.Parent
end

tool.Equipped:Connect(function()
	print("wand equipped by", owner().Name)
end)

tool.Activated:Connect(function()
	print("wand swung by", owner().Name)
	-- a spark at the tip, gone in a second
	local spark = Instance.new("Part")
	spark.Shape = Enum.PartType.Ball
	spark.Size = Vector3.new(1, 1, 1)
	spark.Material = Enum.Material.Neon
	spark.Color = tip.Color
	spark.Position = tip.Position
	spark.Anchored = true
	spark.CanCollide = false
	spark.Parent = workspace
	Debris:AddItem(spark, 1)
end)
