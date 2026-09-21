# Roblox's version 7 meshes turn into geometry.
#
#   godot --headless --path . -s res://tests/roblox_mesh_test.gd
#
# A v7 file is a "COREMESH" container: a format word, a section, then tagged chunks (LODS
# always, SKINNING on a rigged mesh) running to the end of the file. Format 2 holds one Draco
# blob, format 1 plain arrays. Fixtures are in luau/test/fixtures.
#
# The parts come in through import_place because MeshPart.MeshId is NoScriptWrite, as on
# Roblox: no script sets it, not even the command bar, so the host is the only way in.
extends SceneTree

const FIXTURES := "res://../../test/fixtures"
const MESHES := ["rbx_v7_draco", "rbx_v7_raw", "rbx_v7_draco_skinned", "rbx_v7_missing"]

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
	lines.append("RBXMESH %-50s %-9s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = false
	world.data_store_path = ""
	world.asset_root = ProjectSettings.globalize_path(FIXTURES).simplify_path() + "/"
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_warn.connect(func(_n, l): said.append("WARN " + String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	# A *.model.json, the shape the host's own loader takes: the file names the class, and
	# properties arrive under their Roblox names. import_place wants Roblox's binary model.
	var parts := []
	for m in MESHES:
		parts.append({"className": "MeshPart", "name": m, "properties": {"MeshId": m + ".mesh"}})
	world.load_file("Workspace/Meshes.model.json", JSON.stringify({
		"className": "Model", "children": parts,
	}))

	# MeshSize is written back by the engine once geometry is in, so a non-zero one is the
	# mesh having decoded -- not merely the file having been found.
	world.load_file("ServerScriptService/Meshes.server.luau", """
task.wait(2)
for _, p in ipairs(workspace:WaitForChild("Meshes"):GetChildren()) do
	if p:IsA("MeshPart") then
		local s = p.MeshSize
		print("MESH " .. p.Name .. " " .. string.format("%.3f,%.3f,%.3f", s.X, s.Y, s.Z))
	end
end
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	if t < 10.0:
		return false
	var size_of := func(name: String) -> Vector3:
		for l in said:
			if l.begins_with("MESH " + name + " "):
				var nums := l.substr(("MESH " + name + " ").length()).split(",")
				if nums.size() == 3:
					return Vector3(float(nums[0]), float(nums[1]), float(nums[2]))
		return Vector3.ZERO
	var draco: Vector3 = size_of.call("rbx_v7_draco")
	var raw: Vector3 = size_of.call("rbx_v7_raw")
	var skin: Vector3 = size_of.call("rbx_v7_draco_skinned")
	var gone: Vector3 = size_of.call("rbx_v7_missing")

	check("every mesh was tried", said.has("DONE"), true)
	check("a Draco v7 mesh decodes to real bounds", draco.length() > 0.001, true)
	check("a format 1 v7 mesh decodes too", raw.length() > 0.001, true)
	check("a skinned one decodes, its chunks skipped", skin.length() > 0.001, true)
	check("one that is not there stays empty", gone.length() < 0.001, true)
	check("and it is the only one that warned",
		said.filter(func(l): return l.find("could not load") != -1).size(), 1)

	lines.append("RBXMESH   draco %.2f x %.2f x %.2f | raw %.2f x %.2f x %.2f | skinned %.2f x %.2f x %.2f"
		% [draco.x, draco.y, draco.z, raw.x, raw.y, raw.z, skin.x, skin.y, skin.z])
	for l in lines:
		print(l)
	print("RBXMESH %d passed, %d failed" % [ok, bad])
	print("roblox mesh: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
