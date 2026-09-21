# COPY -- do not edit. The original is luau/gdextension/host/Audio.gd; this was put here by
# scripts/sync-host.js because a Godot project cannot read a file outside its own res://.
# Edit the original and run: node scripts/sync-host.js
# Master-bus limiter. The 2A03's five voices are five separate Sounds summing in the engine
# mixer rather than in a chip, so together they peak over unity; a real 2A03 does not clip
# because its mixer is non-linear. Covers menu and script-played sounds as well as music.
extends Node

## Just under full scale: hold peaks rather than wrap them, leave everything below alone.
const CEILING_DB := -1.0
## Above ordinary playing level, so quiet passages are untouched.
const THRESHOLD_DB := -3.0

func _ready() -> void:
	var bus := AudioServer.get_bus_index("Master")
	if bus < 0:
		return
	# A reload would otherwise stack a second limiter on the bus.
	for i in range(AudioServer.get_bus_effect_count(bus) - 1, -1, -1):
		if AudioServer.get_bus_effect(bus, i) is AudioEffectLimiter:
			AudioServer.remove_bus_effect(bus, i)
	var limiter := AudioEffectLimiter.new()
	limiter.ceiling_db = CEILING_DB
	limiter.threshold_db = THRESHOLD_DB
	limiter.soft_clip_db = 2.0
	AudioServer.add_bus_effect(bus, limiter)
	print("[audio] limiter on the master bus, ceiling %.0f dB" % CEILING_DB)
