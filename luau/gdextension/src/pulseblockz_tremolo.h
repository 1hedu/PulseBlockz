// Tremolo: every sample multiplied by a slow wave. Godot ships no tremolo effect; Roblox's
// other eight sound effects map onto ones it does. Roblox's controls: Depth is how far the
// volume drops (1 is silence), Frequency is dips per second, Duty is where the dip sits in
// the cycle -- 0.5 a plain sine, away from it the wave is time-warped.
#pragma once

#include <godot_cpp/classes/audio_effect.hpp>
#include <godot_cpp/classes/audio_effect_instance.hpp>
#include <godot_cpp/classes/audio_frame.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

class PulseBlockzTremolo : public AudioEffect {
	GDCLASS(PulseBlockzTremolo, AudioEffect)

	float depth_ = 0.5f;
	float frequency_ = 5.0f;
	float duty_ = 0.5f;

protected:
	static void _bind_methods();

public:
	Ref<AudioEffectInstance> _instantiate() override;

	void set_depth(float v) { depth_ = v; }
	float get_depth() const { return depth_; }
	void set_frequency(float v) { frequency_ = v; }
	float get_frequency() const { return frequency_; }
	void set_duty(float v) { duty_ = v; }
	float get_duty() const { return duty_; }
};

class PulseBlockzTremoloInstance : public AudioEffectInstance {
	GDCLASS(PulseBlockzTremoloInstance, AudioEffectInstance)

	// Kept across buffers: restarting the wave each buffer clicks at the buffer rate.
	double phase_ = 0.0;

protected:
	static void _bind_methods() {}

public:
	Ref<PulseBlockzTremolo> base;

	void _process(const void *src, AudioFrame *dst, int32_t frames) override;
};

}   // namespace godot
