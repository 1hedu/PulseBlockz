-- src/server is ServerScriptService: a Script. A guard walks between two posts
-- with PathfindingService, never over the lava: a Model with a Humanoid and a
-- HumanoidRootPart is a character the engine walks (Humanoid:MoveTo), and a
-- Path finds the way round what is in between (Costs make Neon impassable).
local PathfindingService = game:GetService("PathfindingService")
local RunService = game:GetService("RunService")
local map = workspace:WaitForChild("Map")
local lava = map:WaitForChild("Lava")
map:WaitForChild("Spawn")

local guard = Instance.new("Model")
guard.Name = "Patrol"
local root = Instance.new("Part")
root.Name = "HumanoidRootPart"
root.Size = Vector3.new(2, 2, 1)
root.Position = Vector3.new(20, 3, -20)
root.Transparency = 1
root.Parent = guard
local torso = Instance.new("Part")
torso.Name = "Torso"
torso.Size = Vector3.new(2, 2, 1)
torso.Position = root.Position
torso.Color = Color3.fromRGB(120, 40, 40)
torso.Parent = guard
local head = Instance.new("Part")
head.Name = "Head"
head.Shape = Enum.PartType.Ball
head.Size = Vector3.new(1.2, 1.2, 1.2)
head.Position = root.Position + Vector3.new(0, 1.6, 0)
head.Color = Color3.fromRGB(255, 204, 153)
head.Parent = guard
local humanoid = Instance.new("Humanoid")
humanoid.WalkSpeed = 14
humanoid.Parent = guard
guard.PrimaryPart = root
guard.Parent = workspace

-- Over the lava this leg? Sampled every Heartbeat, reported at the post.
local overLava = false
RunService.Heartbeat:Connect(function()
	local d = root.Position - lava.Position
	if math.abs(d.X) < lava.Size.X / 2 and math.abs(d.Z) < lava.Size.Z / 2 then overLava = true end
end)

local posts = { Vector3.new(20, 3, -20), Vector3.new(-20, 3, -20) }
local path = PathfindingService:CreatePath({ AgentRadius = 2, AgentHeight = 5, Costs = { Neon = math.huge } })
local leg = 0
while true do
	leg += 1
	path:ComputeAsync(root.Position, posts[leg % 2 + 1])
	local waypoints = path:GetWaypoints()
	print("patrol", path.Status.Name, #waypoints)
	if path.Status ~= Enum.PathStatus.Success then
		task.wait(1)
		continue
	end
	overLava = false
	local arrived = true
	for _, waypoint in ipairs(waypoints) do
		if waypoint.Action == Enum.PathWaypointAction.Jump then humanoid.Jump = true end
		humanoid:MoveTo(waypoint.Position)
		if not humanoid.MoveToFinished:Wait() then
			arrived = false
			break
		end
	end
	print("post", leg, overLava, arrived)
	task.wait(0.5)
end
