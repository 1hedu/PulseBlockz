# The lighting family: Lighting's clock methods, the sun and moon directions, LightingChanged,
# the local transparency modifiers on a Decal, Beam, Trail, Fire and ParticleEmitter, a Decal's
# UV scale and offset, a ParticleEmitter's flipbook, Trail:Clear, Beam:SetTextureOffset, the
# Sky's SkyboxOrientation, MaterialService's base material overrides and variants, and the
# recorded-only members (Clouds, ColorGradingEffect, the rest).
#
#   godot --headless --path . -s res://tests/lighting_parity_test.gd
#
# A Play Solo world; a server script does the work and the host's nodes are read from here.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var ok := 0
var bad := 0
var said: Array[String] = []
var lines: Array[String] = []
var trail_seen := false      # the trail was visible while its ribbon was laid
var trail_seen_at := -1.0
var trail_after_clear := true
var cleared_at := -1.0

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("LIGHTPAR %-66s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = false
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("ServerScriptService/LightingFamily.server.luau", """
local Lighting = game:GetService("Lighting")
local function b(v) return v and "t" or "f" end
local function r3(v) return string.format("%.3f,%.3f,%.3f", v.X, v.Y, v.Z) end
local function near(a, b) return math.abs(a - b) < 1e-3 end

-- Lighting: the clock as minutes, and the sun on the arc the host draws
print("MINUTES " .. tostring(Lighting:GetMinutesAfterMidnight()))
Lighting:SetMinutesAfterMidnight(6 * 60)
local sun6 = Lighting:GetSunDirection()
Lighting:SetMinutesAfterMidnight(12 * 60)
local sun12 = Lighting:GetSunDirection()
local moon12 = Lighting:GetMoonDirection()
Lighting:SetMinutesAfterMidnight(-60)   -- wraps to 23:00
print("CLOCK " .. tostring(Lighting.ClockTime) .. " " .. Lighting.TimeOfDay .. " " .. b(near(sun6.X, 1) and near(sun6.Y, 0)) .. " "
	.. b(near(sun12.Y, 1)) .. " " .. b(near(moon12.X, -sun12.X) and near(moon12.Y, -sun12.Y) and near(moon12.Z, -sun12.Z)))
Lighting.ClockTime = 14

-- LightingChanged: false for Lighting's own properties, true for a Sky's under it and a Sky arriving
local heard = {}
Lighting.LightingChanged:Connect(function(skyChanged) table.insert(heard, b(skyChanged)) end)
Lighting.Brightness = 2
local sky = Instance.new("Sky")
sky.Parent = Lighting
sky.SkyboxUp = "images/sheet.png"
task.wait(0.1)
print("LCHANGED " .. table.concat(heard, "") .. ".")
-- the recorded ones: default, write, the engine-only ones refuse a read
local okLS = pcall(function() return Lighting.LightingStyle end)
local okPQ = pcall(function() return Lighting.PrioritizeLightingQuality end)
local okEL = pcall(function() return Lighting.ExtendLightRangeTo120 end)
print("LIGHTREC " .. tostring(Lighting.GeographicLatitude) .. " " .. tostring(Lighting.ShadowSoftness) .. " " .. b(okLS) .. b(okPQ) .. b(okEL))
Lighting.GeographicLatitude = 10; Lighting.ShadowSoftness = 0.5
print("LIGHTRECW " .. tostring(Lighting.GeographicLatitude) .. " " .. tostring(Lighting.ShadowSoftness))

-- the Sky's six faces turned a quarter about Y
for _, f in ipairs({"Bk", "Dn", "Ft", "Lf", "Rt", "Up"}) do sky["Skybox" .. f] = "images/sheet.png" end
print("SKYORIENT " .. r3(sky.SkyboxOrientation))
sky.SkyboxOrientation = Vector3.new(0, 90, 0)

-- a Decal at Transparency 0.5 and LocalTransparencyModifier 0.5, its uv scaled and shifted
local sign = Instance.new("Part"); sign.Name = "Sign"; sign.Anchored = true; sign.Size = Vector3.new(6, 3, 1); sign.Position = Vector3.new(0, 10, 0); sign.Parent = workspace
local logo = Instance.new("Decal"); logo.Name = "Logo"; logo.Texture = "images/sheet.png"; logo.Transparency = 0.5; logo.Parent = sign
print("DECAL " .. tostring(logo.LocalTransparencyModifier) .. " " .. r3(Vector3.new(logo.UVOffset.X, logo.UVOffset.Y, 0)) .. " " .. r3(Vector3.new(logo.UVScale.X, logo.UVScale.Y, 0)) .. " " .. b(logo.AutoLocalize))
logo.LocalTransparencyModifier = 0.5
logo.UVOffset = Vector2.new(0.25, 0); logo.UVScale = Vector2.new(2, 2); logo.AutoLocalize = true
print("DECALW " .. tostring(logo.LocalTransparencyModifier) .. " " .. b(logo.AutoLocalize))

-- a ParticleEmitter with a 4x4 flipbook at 8 frames a second from a random frame, half faded
local pe = Instance.new("ParticleEmitter"); pe.Name = "Puff"; pe.Texture = "images/sheet.png"
print("EMITTER " .. tostring(pe.FlipbookLayout) .. " " .. tostring(pe.FlipbookMode) .. " " .. tostring(pe.FlipbookFramerate) .. " " .. b(pe.FlipbookStartRandom)
	.. " " .. b(pe.FlipbookBlendFrames) .. " " .. tostring(pe.FlipbookSizeX) .. " " .. tostring(pe.FlipbookSizeY) .. " [" .. pe.FlipbookIncompatible .. "] "
	.. tostring(pe.LocalTransparencyModifier) .. " " .. tostring(pe.ShapePartial) .. " " .. b(pe.WindAffectsDrag))
pe.FlipbookLayout = Enum.ParticleFlipbookLayout.Grid4x4; pe.FlipbookMode = Enum.ParticleFlipbookMode.Loop
pe.FlipbookFramerate = NumberRange.new(8, 8); pe.FlipbookStartRandom = true; pe.LocalTransparencyModifier = 0.5
pe.Lifetime = NumberRange.new(2, 2); pe.ShapePartial = 0.5; pe.WindAffectsDrag = true; pe.FlipbookBlendFrames = true; pe.FlipbookSizeX = 64; pe.FlipbookSizeY = 64
local okInc = pcall(function() pe.FlipbookIncompatible = "x" end)
pe.Parent = sign
print("EMITTERW " .. tostring(pe.FlipbookLayout) .. " " .. tostring(pe.ShapePartial) .. " " .. b(pe.WindAffectsDrag) .. " " .. b(pe.FlipbookBlendFrames) .. " " .. tostring(pe.FlipbookSizeX) .. " " .. b(okInc))
local fire = Instance.new("Fire"); fire.Name = "Flame"; fire.Parent = sign
print("FIRE " .. tostring(fire.LocalTransparencyModifier))
fire.LocalTransparencyModifier = 0.75

-- a Beam half faded, its texture cycle set
local a0 = Instance.new("Attachment"); a0.Position = Vector3.new(-3, 0, 0); a0.Parent = sign
local a1 = Instance.new("Attachment"); a1.Position = Vector3.new(3, 0, 0); a1.Parent = sign
local beam = Instance.new("Beam"); beam.Name = "Arc"; beam.Attachment0 = a0; beam.Attachment1 = a1; beam.Transparency = NumberSequence.new(0)
print("BEAM " .. tostring(beam.LocalTransparencyModifier))
beam.LocalTransparencyModifier = 0.5
beam:SetTextureOffset(0.25)
beam.Parent = sign

-- a Trail laid by a moving part, then cleared
local runner = Instance.new("Part"); runner.Name = "Runner"; runner.Anchored = true; runner.Size = Vector3.new(1, 1, 1); runner.Position = Vector3.new(-10, 5, 0); runner.Parent = workspace
local t0 = Instance.new("Attachment"); t0.Position = Vector3.new(0, 0.5, 0); t0.Parent = runner
local t1 = Instance.new("Attachment"); t1.Position = Vector3.new(0, -0.5, 0); t1.Parent = runner
local trail = Instance.new("Trail"); trail.Name = "Wake"; trail.Attachment0 = t0; trail.Attachment1 = t1; trail.Lifetime = 30; trail.Parent = runner
print("TRAIL " .. tostring(trail.LocalTransparencyModifier))
trail.LocalTransparencyModifier = 0.25
for i = 1, 30 do
	runner.Position = runner.Position + Vector3.new(0.5, 0, 0)
	task.wait(0.05)
end
print("TRAILMOVED")
task.wait(1.5)
trail:Clear()
print("TRAILCLEARED")

-- MaterialService: overrides by name, the variant lookup, the engine-only names unreadable
local MS = game:GetService("MaterialService")
local mv = Instance.new("MaterialVariant"); mv.Name = "Mossy"; mv.BaseMaterial = Enum.Material.Grass; mv.Parent = MS
local okName = pcall(function() return MS.GrassName end)
local okNeon = pcall(function() MS:SetBaseMaterialOverride(Enum.Material.Neon, "x") end)
print("MATSVC [" .. MS:GetBaseMaterialOverride(Enum.Material.Grass) .. "] " .. b(okName) .. " " .. b(okNeon) .. " " .. tostring(MS:GetMaterialVariant(Enum.Material.Grass, "Mossy"))
	.. " " .. tostring(MS:GetMaterialVariant(Enum.Material.Concrete, "Mossy")) .. " " .. tostring(MS:GetMaterialVariant(Enum.Material.Grass, "None")))
MS:SetBaseMaterialOverride(Enum.Material.Grass, "Mossy")
print("MATSVCW [" .. MS:GetBaseMaterialOverride(Enum.Material.Grass) .. "] [" .. MS:GetBaseMaterialOverride(Enum.Material.Carpet) .. "]")
print("MATVAR " .. tostring(mv.AlphaMode) .. " " .. tostring(mv.CustomPhysicalProperties))
mv.AlphaMode = Enum.AlphaMode.Transparency; mv.CustomPhysicalProperties = PhysicalProperties.new(2, 0.5, 0.5)
print("MATVARW " .. tostring(mv.AlphaMode) .. " " .. tostring(mv.CustomPhysicalProperties.Density))

-- the recorded-only classes: Clouds under Terrain, a ColorGradingEffect under Lighting
local clouds = Instance.new("Clouds"); clouds.Parent = workspace.Terrain
local grade = Instance.new("ColorGradingEffect"); grade.Parent = Lighting
print("RECORDED " .. tostring(clouds.Cover) .. " " .. tostring(clouds.Density) .. " " .. tostring(grade.TonemapperPreset) .. " " .. b(grade.Enabled))
clouds.Cover = 0.9; clouds.Density = 0.1; grade.TonemapperPreset = Enum.TonemapperPreset.Retro
print("RECORDEDW " .. tostring(clouds.Cover) .. " " .. tostring(clouds.Density) .. " " .. tostring(grade.TonemapperPreset))
print("ENUMS " .. tostring(Enum.LightingStyle.Soft.Value) .. " " .. tostring(Enum.RolloutState.Enabled.Value) .. " " .. tostring(Enum.ParticleFlipbookLayout.Custom.Value)
	.. " " .. tostring(Enum.ParticleFlipbookMode.Random.Value) .. " " .. tostring(Enum.Material.Rubber.Value))
print("DONE")
""")

func _find(cls: String, name: String) -> Node:
	for n in world.find_children("*", cls, true, false):
		if n.name == name:
			return n
	return null

func _process(delta: float) -> bool:
	t += delta
	var heard := func(s: String) -> bool:
		for l in said:
			if l.find(s) != -1:
				return true
		return false
	# The trail: visible once laid, gone after Clear (Lifetime 30 would otherwise keep it).
	var wake := _find("MeshInstance3D", "Wake")
	if heard.call("TRAILMOVED") and not heard.call("TRAILCLEARED") and wake != null and wake.visible:
		trail_seen = true
	if heard.call("TRAILCLEARED"):
		if cleared_at < 0:
			cleared_at = t
		elif t > cleared_at + 0.3:
			trail_after_clear = wake != null and wake.visible
	if t < 40.0 and not (heard.call("DONE") and cleared_at >= 0 and t > cleared_at + 0.5):
		return false
	check("the script ran to the end", heard.call("DONE"), true)
	check("GetMinutesAfterMidnight is ClockTime in minutes", heard.call("MINUTES 840"), true)
	check("SetMinutesAfterMidnight sets the clock, wrapping; the sun is east at 6, overhead at noon, the moon opposite", heard.call("CLOCK 23 23:00:00 t t t"), true)
	check("LightingChanged: false for Lighting's own, true for a Sky arriving and its faces", heard.call("LCHANGED ftt."), true)
	check("the recorded members read their defaults; the engine-only ones refuse", heard.call("LIGHTREC 41.733 0.2 fff"), true)
	check("and take a write", heard.call("LIGHTRECW 10 0.5"), true)
	check("SkyboxOrientation reads its zero", heard.call("SKYORIENT 0.000,0.000,0.000"), true)
	var env: WorldEnvironment = world.get_node_or_null("Lighting")
	var sky_mat: ShaderMaterial = env.environment.sky.sky_material as ShaderMaterial if env and env.environment and env.environment.sky else null
	var rot = sky_mat.get_shader_parameter("faces_rot") if sky_mat else null
	check("and turns the six faces a quarter about Y (the lookup's inverse)", rot is Basis and (rot as Basis).is_equal_approx(Basis(Vector3(0, 1, 0), deg_to_rad(90.0)).inverse()), true)
	check("a Decal's new members read their defaults", heard.call("DECAL 0 0.000,0.000,0.000 1.000,1.000,0.000 f"), true)
	check("and take a write", heard.call("DECALW 0.5 t"), true)
	var logo := _find("MeshInstance3D", "Logo")
	var lm := logo.material_override as StandardMaterial3D if logo else null
	check("its alpha folds Transparency and LocalTransparencyModifier: 0.25", lm != null and is_equal_approx(lm.albedo_color.a, 0.25), true)
	check("its uv is scaled by UVScale and shifted by UVOffset", lm != null and lm.uv1_scale.is_equal_approx(Vector3(2, 2, 1)) and lm.uv1_offset.is_equal_approx(Vector3(0.25, 0, 0)), true)
	check("a ParticleEmitter's new members read their defaults", heard.call("EMITTER Enum.ParticleFlipbookLayout.None Enum.ParticleFlipbookMode.Loop 1 1 f f 0 0 [] 0 1 f"), true)
	check("and take a write; FlipbookIncompatible is read only", heard.call("EMITTERW Enum.ParticleFlipbookLayout.Grid4x4 0.5 t t 64 f"), true)
	var puff := _find("GPUParticles3D", "Puff")
	var pmat := puff.draw_pass_1.material as StandardMaterial3D if puff and puff.draw_pass_1 else null
	var ppm := puff.process_material as ParticleProcessMaterial if puff else null
	check("its flipbook is a 4x4 grid, looping", pmat != null and pmat.particles_anim_h_frames == 4 and pmat.particles_anim_v_frames == 4 and pmat.particles_anim_loop, true)
	check("at 8 frames a second over a 2 second life: one animation per life, from a random frame", ppm != null and is_equal_approx(ppm.anim_speed_max, 1.0) and is_equal_approx(ppm.anim_offset_max, 1.0), true)
	var ramp := ppm.color_ramp as GradientTexture1D if ppm else null
	check("its colour ramp's alpha is halved by LocalTransparencyModifier", ramp != null and ramp.gradient.get_color(0).a == 0.5, true)
	check("Fire's LocalTransparencyModifier reads its zero", heard.call("FIRE 0"), true)
	var flame := _find("GPUParticles3D", "Flame")
	var framp := (flame.process_material as ParticleProcessMaterial).color_ramp as GradientTexture1D if flame and flame.process_material else null
	check("and fades the flames: 0.8 * 0.25", framp != null and is_equal_approx(framp.gradient.get_color(0).a, 0.2), true)
	check("a Beam's LocalTransparencyModifier reads its zero", heard.call("BEAM 0"), true)
	var arc := _find("MeshInstance3D", "Arc")
	var colors: PackedColorArray = (arc.mesh as ImmediateMesh).surface_get_arrays(0)[Mesh.ARRAY_COLOR] if arc and arc.mesh and arc.mesh.get_surface_count() > 0 else PackedColorArray()
	check("and halves the ribbon's alpha", colors.size() > 0 and absf(colors[0].a - 0.5) < 0.01, true)
	check("a Trail's LocalTransparencyModifier reads its zero", heard.call("TRAIL 0"), true)
	check("a trail laid by a moving part shows", trail_seen, true)
	check("and Clear() drops it, Lifetime notwithstanding", trail_after_clear, false)
	check("MaterialService: no override; the names are engine-only; Neon takes none; the variant by material and name", heard.call("MATSVC [] f f Mossy nil nil"), true)
	check("SetBaseMaterialOverride reads back through GetBaseMaterialOverride", heard.call("MATSVCW [Mossy] []"), true)
	check("a MaterialVariant's new members read their defaults", heard.call("MATVAR Enum.AlphaMode.Overlay nil"), true)
	check("and take a write", heard.call("MATVARW Enum.AlphaMode.Transparency 2"), true)
	check("Clouds and ColorGradingEffect read their defaults", heard.call("RECORDED 0.5 0.7 Enum.TonemapperPreset.Default t"), true)
	check("and take a write", heard.call("RECORDEDW 0.9 0.1 Enum.TonemapperPreset.Retro"), true)
	check("the enums resolve", heard.call("ENUMS 1 2 4 3 2311"), true)
	check("no script error", heard.call("ERROR"), false)
	for l in lines:
		print(l)
	if bad > 0:
		for l in said:
			print("LIGHTPAR said: " + l)
	print("LIGHTPAR %d passed, %d failed" % [ok, bad])
	print("lighting parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
