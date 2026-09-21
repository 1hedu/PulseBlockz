-- A runaway script. Its Heartbeat handler is killed at the per-call budget every
-- frame, on the worker thread; the frame rate does not notice, the other scripts
-- keep running. Delete this file to unload it.
game:GetService("RunService").Heartbeat:Connect(function()
	while true do end
end)
