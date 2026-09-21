-- src/server is ServerScriptService (default.project.json): a Script.
-- Spins a ring of parts every Heartbeat and drops a coloured brick every
-- second; bricks that land on the lava are destroyed by workspace.Map.Build.
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local Debris = game:GetService("Debris")
local Palette = require(ReplicatedStorage:WaitForChild("Palette"))

local rng = Random.new(7)
local ring = Instance.new("Model")
ring.Name = "Ring"
ring.Parent = workspace

local N = 12
for i = 1, N do
	local p = Instance.new("Part")
	p.Name = "Spinner" .. i
	p.Size = Vector3.new(1.5, 1.5, 1.5)
	p.Color = Palette.Colors[(i - 1) % #Palette.Colors + 1]
	p.Material = if i % 3 == 0 then Enum.Material.Neon else Enum.Material.SmoothPlastic
	p.Anchored = true
	p.Parent = ring
end

local t = 0
RunService.Heartbeat:Connect(function(dt)
	t += dt
	for i, p in ipairs(ring:GetChildren()) do
		local a = t + (i / N) * math.pi * 2
		p.Position = Vector3.new(math.cos(a) * 9, 6 + math.sin(t * 2 + i) * 1.5, math.sin(a) * 9)
		p.Orientation = Vector3.new(0, -math.deg(a), 0)
	end
end)

task.spawn(function()
	while true do
		local brick = Instance.new("Part")
		brick.Name = "Brick"
		brick.Size = Vector3.new(2, 1, 1)
		brick.Position = Vector3.new(rng:NextNumber(-6, 6), 14, rng:NextNumber(-22, 6))
		brick.Orientation = Vector3.new(0, rng:NextNumber(0, 360), 0)
		brick.Color = Palette.random(rng)
		brick.Parent = workspace
		Debris:AddItem(brick, 20)
		task.wait(1)
	end
end)

-- Players: greet them, and answer the client's RemoteEvent with a brick
-- dropped over their head.
local Players = game:GetService("Players")
local drop = Instance.new("RemoteEvent")
drop.Name = "DropBrick"
drop.Parent = ReplicatedStorage

-- The wave: a KeyframeSequence saved from Studio (src/shared/animations/Wave.rbxmx,
-- so ReplicatedStorage.animations.Wave), loaded onto the character's Animator and
-- played when a client asks -- animations belong to the server here, so everyone sees them.
local wave = Instance.new("RemoteEvent")
wave.Name = "Wave"
wave.Parent = ReplicatedStorage
local waveAnim = Instance.new("Animation")
waveAnim.Name = "Wave"
waveAnim.AnimationId = "ReplicatedStorage.animations.Wave"
waveAnim.Parent = ReplicatedStorage

-- The shrug underneath it: Movement priority, so the wave (Action) takes the arm
-- it names and the shrug keeps the other, the way animations layer in Roblox.
local shrugAnim = Instance.new("Animation")
shrugAnim.Name = "Shrug"
shrugAnim.AnimationId = "ReplicatedStorage.animations.Shrug"
shrugAnim.Parent = ReplicatedStorage

wave.OnServerEvent:Connect(function(player)
	local humanoid = player.Character and player.Character:FindFirstChildOfClass("Humanoid")
	if not humanoid then return end
	local shrug = humanoid.Animator:LoadAnimation(shrugAnim)
	shrug:Play(0.2)
	local track = humanoid.Animator:LoadAnimation(waveAnim)
	track.KeyframeReached:Connect(function(name) print("keyframe", name) end)
	track.Stopped:Connect(function()
		shrug:Stop(0)
		print("waved", track.IsPlaying, #humanoid.Animator:GetPlayingAnimationTracks())
	end)
	track:Play()
	print("waving", track.Length, track.Looped, track.IsPlaying, shrug.Priority.Name, track.Priority.Name)
	task.delay(2.5, function() track:Stop(0.3) end)
end)

Players.PlayerAdded:Connect(function(player)
	print("welcome", player.Name, "userId", player.UserId)
	player.Chatted:Connect(function(message) print(player.Name, "said", message) end)
	-- leaderstats: the player list shows these columns
	local leaderstats = Instance.new("Folder")
	leaderstats.Name = "leaderstats"
	local bricks = Instance.new("IntValue")
	bricks.Name = "Bricks"
	bricks.Parent = leaderstats
	local deaths = Instance.new("IntValue")
	deaths.Name = "Deaths"
	deaths.Parent = leaderstats
	leaderstats.Parent = player
	player.CharacterAdded:Connect(function(character)
		local humanoid = character:WaitForChild("Humanoid")
		-- Everyone gets a cap: an Accessory whose Handle's HatAttachment meets the
		-- one on the Head, so it rides there (Roblox's own way of wearing things).
		local cap = Instance.new("Accessory")
		cap.Name = "Cap"
		cap.AccessoryType = Enum.AccessoryType.Hat
		local handle = Instance.new("Part")
		handle.Name = "Handle"
		handle.Size = Vector3.new(2, 0.6, 2)
		handle.Color = Palette.Colors[6]
		handle.CanCollide = false
		handle.Parent = cap
		local hook = Instance.new("Attachment")
		hook.Name = "HatAttachment"
		hook.Position = Vector3.new(0, -0.3, 0)
		hook.Parent = handle
		humanoid:AddAccessory(cap)
		print("wearing", #humanoid:GetAccessories(), cap.Parent == character)
		humanoid.Died:Connect(function()
			print(player.Name, "died")
			deaths.Value += 1
		end)
	end)
end)

drop.OnServerEvent:Connect(function(player, color)
	local character = player.Character
	local root = character and character:FindFirstChild("HumanoidRootPart")
	if not root then return end
	local brick = Instance.new("Part")
	brick.Name = player.Name .. "Brick"
	brick.Size = Vector3.new(2, 1, 1)
	brick.Position = root.Position + Vector3.new(0, 8, 0)
	brick.Color = color
	brick.Parent = workspace
	Debris:AddItem(brick, 20)
	player.leaderstats.Bricks.Value += 1
	print("dropped a brick on", player.Name)
end)

print("server up;", RunService:IsServer() and "server" or "client", "-", #ring:GetChildren(), "spinners")
