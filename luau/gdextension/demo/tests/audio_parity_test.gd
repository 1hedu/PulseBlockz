# The audio family: the Audio API's effects (AudioFader, AudioEcho, AudioCompressor, AudioLimiter,
# AudioDistortion, AudioEqualizer, AudioChorus, AudioFlanger, AudioGate, AudioPitchShifter,
# AudioTremolo) heard through the host mixer, the channel splitter and mixer's pins, the sinks and
# sources that only declare (AudioAnalyzer, AudioDeviceInput, AudioRecorder, AudioSpeechToText,
# AudioTextToSpeech, AudioSearchParams), AudioPlayer.AudioContent and GetWaveformAsync, the
# emitter's and listener's engine-only curves, and a Sound's playback and loop regions.
#
#   godot --headless --path . -s res://tests/audio_parity_test.gd
#
# A Play Solo world; a server script does the work and the host's buses are read from here.
extends SceneTree

var world: PulseBlockzWorld
var t := 0.0
var ok := 0
var bad := 0
var said: Array[String] = []
var lines: Array[String] = []
var bus_before := -1        # effects on the voice's bus before AudioFader.Bypass
var bus_kinds := ""

func check(what: String, got, want) -> void:
	if got == want:
		ok += 1
	else:
		bad += 1
	lines.append("AUDIOPAR %-70s %-8s (wanted %s)%s" % [what, str(got), str(want), "" if got == want else "   <-- WRONG"])

func _line(prefix: String) -> String:
	for l in said:
		if l.begins_with(prefix):
			return l.substr(prefix.length())
	return ""

func _heard(s: String) -> bool:
	for l in said:
		if l.find(s) != -1:
			return true
	return false

func _bus() -> int:
	for i in AudioServer.get_bus_count():
		if AudioServer.get_bus_name(i).begins_with("pblockz_audio_"):
			return i
	return -1

func _kinds(bus: int) -> String:
	var out: Array[String] = []
	for k in AudioServer.get_bus_effect_count(bus):
		out.append(AudioServer.get_bus_effect(bus, k).get_class())
	return ",".join(out)

func _initialize() -> void:
	world = PulseBlockzWorld.new()
	world.name = "World"
	world.mode = PulseBlockzWorld.MODE_PLAY_SOLO
	world.auto_join = false
	world.data_store_path = ""
	world.script_print.connect(func(_n, l): said.append(String(l)))
	world.script_error.connect(func(_n, l): said.append("ERROR " + String(l)))
	root.add_child(world)

	world.load_file("ServerScriptService/AudioFamily.server.luau", """
local function b(v) return v and "t" or "f" end
local function near(a, c) return math.abs(a - c) < 1e-4 end
local function same(a, c)
	if typeof(a) == "number" then return typeof(c) == "number" and near(a, c) end
	return a == c
end

-- Every new class: Instance.new, the defaults read back, and a write of another value lands.
local defaults = {
	AudioFader = { Bypass = false, Volume = 1 },
	AudioEcho = { Bypass = false, DelayTime = 1, DryLevel = 0, Feedback = 0.5, RampTime = 0, WetLevel = 0 },
	AudioCompressor = { Attack = 0.1, Bypass = false, MakeupGain = 0, Ratio = 40, Release = 0.1, Threshold = -40 },
	AudioLimiter = { Bypass = false, MaxLevel = 0, Release = 0.01 },
	AudioDistortion = { Bypass = false, Level = 0.5 },
	AudioEqualizer = { Bypass = false, HighGain = 0, LowGain = 0, MidGain = 0, MidRange = NumberRange.new(400, 4000) },
	AudioChorus = { Bypass = false, Depth = 0.45, Mix = 0.85, Rate = 5 },
	AudioFlanger = { Bypass = false, Depth = 0.45, Mix = 0.85, Rate = 5 },
	AudioGate = { Attack = 0.01, Bypass = false, Release = 0.1, Threshold = NumberRange.new(-36, -24) },
	AudioPitchShifter = { Bypass = false, Pitch = 1.25, WindowSize = Enum.AudioWindowSize.Medium },
	AudioTremolo = { Bypass = false, Depth = 1, Duty = 0.5, Frequency = 5, Shape = 0, Skew = 0, Square = 0 },
	AudioAnalyzer = { PeakLevel = 0, RmsLevel = 0, SpectrumEnabled = true, WindowSize = Enum.AudioWindowSize.Medium },
	AudioChannelMixer = { Layout = Enum.AudioChannelLayout.Stereo },
	AudioChannelSplitter = { Layout = Enum.AudioChannelLayout.Stereo },
	AudioDeviceInput = { AccessType = Enum.AccessModifierType.Deny, Active = true, IsReady = false, Muted = false, Player = nil, Volume = 1 },
	AudioRecorder = { IsRecording = false, TimeLength = 0 },
	AudioSearchParams = { Album = "", Artist = "", AudioSubType = Enum.AudioSubType.Music, MaxDuration = 0, MinDuration = 0, SearchKeyword = "", Tag = "", Title = "" },
	AudioSpeechToText = { Enabled = false, Text = "", VoiceDetected = false },
	AudioTextToSpeech = { IsLoaded = false, IsPlaying = false, Looping = false, Pitch = 0, PlaybackSpeed = 1, Speed = 1, Text = "",
		TimeLength = 0, TimePosition = 0, VoiceId = "", Volume = 1 },
	Sound = { LoopRegion = NumberRange.new(0, 60000), PlaybackRegion = NumberRange.new(0, 60000), PlaybackRegionsEnabled = false },
}
local readOnly = { PeakLevel = true, RmsLevel = true, IsReady = true, IsRecording = true, TimeLength = true, VoiceDetected = true,
	IsLoaded = true, IsPlaying = true, Active = true }
local other = { boolean = function(v) return not v end, number = function(v) return v + 1.5 end, string = function() return "x" end,
	NumberRange = function() return NumberRange.new(1, 2) end }
local badDefaults, badWrites = {}, {}
for cls, props in pairs(defaults) do
	local i = Instance.new(cls)
	for name, want in pairs(props) do
		local got = i[name]
		if not same(got, want) then table.insert(badDefaults, cls .. "." .. name .. "=" .. tostring(got)) end
		if not readOnly[name] then
			local ty = typeof(want)
			if ty == "EnumItem" then
				local items = want.EnumType:GetEnumItems()
				local pick = items[1] == want and items[2] or items[1]
				i[name] = pick
				if i[name] ~= pick then table.insert(badWrites, cls .. "." .. name) end
			elseif want == nil then
				local part = Instance.new("Part")
				i[name] = part
				if i[name] ~= part then table.insert(badWrites, cls .. "." .. name) end
			else
				local v = other[ty](want)
				i[name] = v
				if not same(i[name], v) then table.insert(badWrites, cls .. "." .. name) end
			end
		else
			local okw = pcall(function() i[name] = other[typeof(want)](want) end)
			if okw then table.insert(badWrites, cls .. "." .. name .. " (writable)") end
		end
	end
end
print("DEFAULTS " .. (#badDefaults == 0 and "ok" or table.concat(badDefaults, " ")))
print("WRITES " .. (#badWrites == 0 and "ok" or table.concat(badWrites, " ")))

-- Pins: an effect's, a compressor's Sidechain, a splitter's and mixer's channels, a sink's, a source's.
local comp = Instance.new("AudioCompressor")
local split = Instance.new("AudioChannelSplitter") split.Layout = Enum.AudioChannelLayout.Quad
local mix = Instance.new("AudioChannelMixer")
local analyzer = Instance.new("AudioAnalyzer")
local mic = Instance.new("AudioDeviceInput")
local tts = Instance.new("AudioTextToSpeech")
print("PINS " .. table.concat(comp:GetInputPins(), ",") .. "|" .. table.concat(comp:GetOutputPins(), ",")
	.. "|" .. table.concat(split:GetInputPins(), ",") .. "|" .. table.concat(split:GetOutputPins(), ",")
	.. "|" .. table.concat(mix:GetInputPins(), ",") .. "|" .. table.concat(mix:GetOutputPins(), ",")
	.. "|" .. table.concat(analyzer:GetInputPins(), ",") .. "|" .. table.concat(analyzer:GetOutputPins(), ",")
	.. "|" .. table.concat(mic:GetInputPins(), ",") .. "|" .. table.concat(mic:GetOutputPins(), ",")
	.. "|" .. table.concat(tts:GetOutputPins(), ","))

-- The chain the host hears: a player through every effect, a splitter and a mixer, to the speakers.
local rig = Instance.new("Folder") rig.Name = "Rig" rig.Parent = workspace
local player = Instance.new("AudioPlayer") player.Name = "Player" player.Asset = "sounds/creak.wav" player.Parent = rig
local order = { "AudioFader", "AudioEcho", "AudioCompressor", "AudioLimiter", "AudioDistortion", "AudioEqualizer", "AudioChorus",
	"AudioFlanger", "AudioGate", "AudioPitchShifter", "AudioTremolo" }
local nodes = { player }
for _, cls in ipairs(order) do
	local n = Instance.new(cls) n.Name = cls n.Parent = rig
	table.insert(nodes, n)
end
local splitter = Instance.new("AudioChannelSplitter") splitter.Name = "Splitter" splitter.Parent = rig
local mixer = Instance.new("AudioChannelMixer") mixer.Name = "Mixer" mixer.Parent = rig
local out = Instance.new("AudioDeviceOutput") out.Name = "Out" out.Parent = rig
table.insert(nodes, splitter) table.insert(nodes, mixer) table.insert(nodes, out)
local wires = {}
local function wire(a, c, from, to)
	local w = Instance.new("Wire") w.SourceInstance = a w.TargetInstance = c
	w.SourceName = from or "Output" w.TargetName = to or "Input" w.Parent = rig
	table.insert(wires, w)
	return w
end
for k = 1, #nodes - 1 do
	local a, c = nodes[k], nodes[k + 1]
	wire(a, c, a == splitter and "Left" or "Output", c == mixer and "Left" or "Input")
end
local side = Instance.new("AudioPlayer") side.Name = "Side" side.Parent = rig
local sideWire = wire(side, rig.AudioCompressor, "Output", "Sidechain")
local badWire = wire(analyzer, out, "Output", "Input")   -- an analyzer has no Output
analyzer.Parent = rig
rig.AudioFader.Volume = 0.5
task.wait(0.5)
local connected = {}
for _, w in ipairs(wires) do table.insert(connected, b(w.Connected)) end
print("WIRES " .. table.concat(connected, ""))
print("SIDECHAIN " .. b(sideWire.Connected) .. " " .. b(badWire.Connected) .. " " .. #rig.AudioCompressor:GetConnectedWires("Sidechain"))
print("CHAINED")
task.wait(1)
rig.AudioFader.Bypass = true
print("BYPASSED")
task.wait(1)

-- AudioPlayer: AudioContent is Asset's Content twin; GetWaveformAsync is a table.
local wf = player:GetWaveformAsync(NumberRange.new(0, 1), 8)
print("CONTENT " .. tostring(player.AudioContent.SourceType) .. " " .. tostring(player.AudioContent.Uri) .. " " .. typeof(wf))
player.AudioContent = Content.fromUri("sounds/other.wav")
print("TWIN " .. player.Asset)

-- The declared-only members' methods keep their state honest.
print("RECORDER " .. b(Instance.new("AudioRecorder"):CanRecordAsync()) .. " " .. b(pcall(function() Instance.new("AudioRecorder"):RecordAsync() end))
	.. " " .. tostring(Instance.new("AudioRecorder"):GetTemporaryContent().SourceType) .. " " .. #Instance.new("AudioRecorder"):GetUnrecordableInstancesAsync())
mic:SetUserIdAccessList({ 12, 345 })
local list = mic:GetUserIdAccessList()
print("ACCESS " .. table.concat(list, ",") .. " " .. #mic:GetUserIdAccessList())
tts:Play() local p1 = tts.IsPlaying tts:Pause() local p2 = tts.IsPlaying
tts:LoadAsync() local l1 = tts.IsLoaded tts:Unload()
print("TTS " .. b(p1) .. b(p2) .. b(l1) .. " " .. typeof(tts:GetWaveformAsync(NumberRange.new(0, 1), 4)) .. " " .. #analyzer:GetSpectrum())

-- The emitter's and listener's curves: reached by the methods, not by name.
local em = Instance.new("AudioEmitter") em.Parent = rig
em:SetDistanceAttenuation({ [0] = 1, [40] = 0 })
local okRead = pcall(function() return em.DistanceAttenuation end)
local okWrite = pcall(function() em.AngleAttenuation = "x" end)
print("CURVES " .. b(okRead) .. b(okWrite) .. " " .. tostring(em:GetDistanceAttenuation()[40]))

-- A Sound with regions: plays [0.1, 0.2) of a 0.25 s clip once, then ends where the region starts.
local snd = Instance.new("Sound") snd.Name = "Regioned" snd.SoundId = "sounds/creak.wav"
snd.PlaybackRegionsEnabled = true snd.PlaybackRegion = NumberRange.new(0.1, 0.2) snd.Parent = rig
repeat task.wait(0.1) until snd.TimeLength > 0
local ended = 0
snd.Ended:Connect(function() ended += 1 end)
snd:Play()
task.wait(0.05)
local mid = snd.TimePosition
task.wait(0.6)
print(("REGION %.2f %s %.2f %d %s"):format(snd.TimeLength, b(snd.Playing), snd.TimePosition, ended, b(mid >= 0.1 and mid < 0.2)))

-- Looped inside LoopRegion [0.15, 0.2): many more loops than the whole clip would make.
local loops = 0
snd.Looped = true snd.LoopRegion = NumberRange.new(0.15, 0.2)
snd.DidLoop:Connect(function() loops += 1 end)
snd:Play()
task.wait(0.6)
local inside = snd.TimePosition >= 0.15 and snd.TimePosition < 0.2
snd:Stop()
print(("LOOPS %d %s"):format(loops, b(inside)))

-- Off, a Sound plays the whole clip as before.
local plain = Instance.new("Sound") plain.SoundId = "sounds/creak.wav" plain.Parent = rig
plain.PlaybackRegion = NumberRange.new(0.1, 0.2)
repeat task.wait(0.1) until plain.TimeLength > 0
local plainEnded = 0
plain.Ended:Connect(function() plainEnded += 1 end)
plain:Play()
task.wait(0.05)
local early = plain.TimePosition
task.wait(0.5)
print(("PLAIN %s %d %.2f"):format(b(early < 0.1), plainEnded, plain.TimePosition))
print("DONE")
""")

func _process(delta: float) -> bool:
	t += delta
	# The host's bus, while the chain is up and again once the fader is bypassed.
	if bus_before < 0 and _heard("CHAINED") and not _heard("BYPASSED"):
		var bus := _bus()
		if bus >= 0:
			bus_before = AudioServer.get_bus_effect_count(bus)
			bus_kinds = _kinds(bus)
			var amp := AudioServer.get_bus_effect(bus, 0) as AudioEffectAmplify
			check("AudioFader.Volume 0.5 is the amplifier at -6 dB", amp != null and is_equal_approx(amp.volume_db, 20.0 * log(0.5) / log(10.0)), true)
	if t < 14.0 and not _heard("DONE"):
		return false
	check("the script ran to the end", _heard("DONE"), true)
	check("every new class reads its defaults back: %s" % _line("DEFAULTS "), _line("DEFAULTS "), "ok")
	check("every writable member takes a write, no read-only one does: %s" % _line("WRITES "), _line("WRITES "), "ok")
	check("pins: a compressor's Sidechain, a Quad splitter's and a Stereo mixer's channels, a sink's, a mic's, a voice's",
		_line("PINS "), "Input,Sidechain|Output|Input|Left,Right,BackLeft,BackRight|Left,Right|Output|Input|||Output|Output")
	# Fourteen wires down the chain, then the Sidechain one and the one from the analyzer's missing Output.
	check("every wire of the chain connects, the splitter's Left into the mixer's Left included", _line("WIRES "), "tttttttttttttttf")
	check("a Sidechain wire connects and is listed; an analyzer has no Output to leave by", _line("SIDECHAIN "), "t f 1")
	check("the host heard the chain: one voice bus", bus_before >= 0, true)
	check("with the ten Godot effects in wire order (gate, splitter and mixer add none): %s" % bus_kinds, bus_kinds,
		"AudioEffectAmplify,AudioEffectDelay,AudioEffectCompressor,AudioEffectHardLimiter,AudioEffectDistortion,AudioEffectEQ6,AudioEffectChorus,AudioEffectChorus,AudioEffectPitchShift,PulseBlockzTremolo")
	var bus := _bus()
	check("AudioFader.Bypass drops the amplifier from the bus", bus >= 0 and bus_before == 10 and AudioServer.get_bus_effect_count(bus) == 9, true)
	check("AudioPlayer.AudioContent is Asset as a Content; GetWaveformAsync is a table", _line("CONTENT "), "Enum.ContentSourceType.Uri sounds/creak.wav table")
	check("writing AudioContent writes Asset", _line("TWIN "), "sounds/other.wav")
	check("AudioRecorder: cannot record, RecordAsync refuses, no content, nothing unrecordable", _line("RECORDER "), "f f Enum.ContentSourceType.None 0")
	check("AudioDeviceInput's access list round-trips", _line("ACCESS "), "12,345 2")
	check("AudioTextToSpeech: IsPlaying follows Play and Pause, IsLoaded stays false; empty waveform and spectrum", _line("TTS "), "tff table 0")
	check("the curves are the engine's: no read or write by name, the methods reach them", _line("CURVES "), "ff 0")
	var region := _line("REGION ").split(" ")
	check("Sound: the region played once and ended at its start: %s" % _line("REGION "),
		region.size() == 5 and region[0] == "0.25" and region[1] == "f" and region[2] == "0.10" and region[3] == "1" and region[4] == "t", true)
	var loops := _line("LOOPS ").split(" ")
	check("Sound: Looped inside a 50 ms LoopRegion loops many times in 0.6 s and stays inside it: %s" % _line("LOOPS "),
		loops.size() == 2 and int(loops[0]) >= 5 and loops[1] == "t", true)
	var plain := _line("PLAIN ").split(" ")
	check("Sound: regions off, the whole clip plays from 0 and ends at 0: %s" % _line("PLAIN "),
		plain.size() == 3 and plain[0] == "t" and plain[1] == "1" and plain[2] == "0.00", true)
	check("no script error", _heard("ERROR"), false)
	for l in lines:
		print(l)
	for l in said:
		if l.begins_with("ERROR"):
			print(l)
	print("AUDIOPAR %d passed, %d failed" % [ok, bad])
	print("audio parity: %s" % ("PASS" if bad == 0 else "FAIL"))
	quit(0 if bad == 0 else 1)
	return true
