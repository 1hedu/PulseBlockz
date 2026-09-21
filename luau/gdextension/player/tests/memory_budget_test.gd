# A place cannot eat this machine's memory.
#
#   godot --headless --path . -s res://tests/memory_budget_test.gd
#
# luau_sandbox.h defaults to a million steps, 4 ms a call and 8 MB; PulseBlockzWorld's constructor
# zeroes steps, frame and memory and sets a ten second wall clock, so the memory cap is whatever
# the host sets. Player.PLACE_MEMORY_MB is 64, about twelve times the 4.98 MiB of Luau heap
# pBlockz Home peaks at (get_stats() memory_peak; meshes and textures are Godot's, not the
# sandbox's). The world is built the way the Player builds one, so a Player that stopped applying
# the cap fails this rather than the constant being read back.
extends SceneTree

const PLAYER := preload("res://Player.gd")

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
	lines.append("MEMORY %-52s %-10s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	check("the Player names a budget", PLAYER.PLACE_MEMORY_MB > 0, true)
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.max_memory_mb = PLAYER.PLACE_MEMORY_MB     # what play() and join() both set
	world.auto_join = false
	world.data_store_path = ""
	world.script_print.connect(func(_n, line): said.append(String(line)))
	world.script_error.connect(func(_n, e): said.append("ERROR " + String(e)))
	root.add_child(world)

	# Held in one table so nothing is collected mid-loop, and each string carries an index prefix:
	# Luau interns strings, so 400 identical string.rep("x", 1MB) results allocate one megabyte.
	world.load_file("ServerScriptService/Greedy.server.luau", """
local held = {}
local ok, err = pcall(function()
	for i = 1, 400 do
		held[i] = tostring(i) .. string.rep("x", 1024 * 1024)   -- a distinct megabyte, up to 400
	end
end)
print("GREEDY done=" .. tostring(ok) .. " held=" .. tostring(#held) .. " kb=" .. tostring(math.floor(collectgarbage("count"))) .. " err=" .. tostring(err))
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 12.0:
		return false
	var heard := func(s: String) -> bool:
		var hit := false
		for l in said:
			if l.find(s) != -1:
				hit = true
		return hit
	var stats: Dictionary = world.get_stats()
	var peak := int(stats.get("memory_peak", 0))
	check("the sandbox reports what it holds", stats.has("memory_now"), true)
	check("the greedy script was stopped", heard.call("GREEDY done=false") or heard.call("ERROR"), true)
	check("it never held 400 MB", heard.call("held=400"), false)
	check("the peak stayed under the budget", peak <= PLAYER.PLACE_MEMORY_MB * 1048576, true)
	lines.append("MEMORY   peak was %.1f MiB against a %d MiB budget" % [peak / 1048576.0, PLAYER.PLACE_MEMORY_MB])
	for l in said:
		if l.find("GREEDY") != -1:
			lines.append("MEMORY   " + l)
	for l in lines:
		print(l)
	print("MEMORY %d passed, %d failed" % [ok, bad])
	print("memory budget: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
