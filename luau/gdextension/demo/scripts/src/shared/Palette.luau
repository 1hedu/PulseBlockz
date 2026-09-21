-- A ModuleScript: src/shared is ReplicatedStorage, so game.ReplicatedStorage.Palette
local Palette = {}

Palette.Colors = {
	Color3.fromRGB(196, 40, 28),   -- Bright red
	Color3.fromRGB(13, 105, 172),  -- Bright blue
	Color3.fromRGB(245, 205, 48),  -- Bright yellow
	Color3.fromRGB(75, 151, 75),   -- Bright green
	Color3.fromRGB(255, 176, 0),   -- Deep orange
	Color3.fromRGB(107, 50, 124),  -- Royal purple
}

function Palette.random(rng: Random?): Color3
	local r = rng or Random.new()
	return Palette.Colors[r:NextInteger(1, #Palette.Colors)]
end

return Palette
