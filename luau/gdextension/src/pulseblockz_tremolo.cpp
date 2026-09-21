#include "pulseblockz_tremolo.h"

#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/core/math.hpp>

#include <algorithm>
#include <cmath>

using namespace godot;

void PulseBlockzTremolo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_depth", "v"), &PulseBlockzTremolo::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &PulseBlockzTremolo::get_depth);
	ClassDB::bind_method(D_METHOD("set_frequency", "v"), &PulseBlockzTremolo::set_frequency);
	ClassDB::bind_method(D_METHOD("get_frequency"), &PulseBlockzTremolo::get_frequency);
	ClassDB::bind_method(D_METHOD("set_duty", "v"), &PulseBlockzTremolo::set_duty);
	ClassDB::bind_method(D_METHOD("get_duty"), &PulseBlockzTremolo::get_duty);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth"), "set_depth", "get_depth");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frequency"), "set_frequency", "get_frequency");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "duty"), "set_duty", "get_duty");
}

Ref<AudioEffectInstance> PulseBlockzTremolo::_instantiate() {
	Ref<PulseBlockzTremoloInstance> made;
	made.instantiate();
	made->base = Ref<PulseBlockzTremolo>(this);
	return made;
}

void PulseBlockzTremoloInstance::_process(const void *src, AudioFrame *dst, int32_t frames) {
	const AudioFrame *in = static_cast<const AudioFrame *>(src);
	if (!in || !dst || frames <= 0) {
		return;
	}
	if (base.is_null()) {
		for (int32_t i = 0; i < frames; i++) dst[i] = in[i];
		return;
	}

	const double depth = std::clamp((double)base->get_depth(), 0.0, 1.0);
	const double freq = std::max(0.0, (double)base->get_frequency());
	// Held off 0 and 1: the warp below divides by duty and by 1 - duty.
	const double duty = std::clamp((double)base->get_duty(), 0.02, 0.98);

	const double rate = (double)AudioServer::get_singleton()->get_mix_rate();
	const double step = (rate > 0.0 && freq > 0.0) ? freq / rate : 0.0;

	for (int32_t i = 0; i < frames; i++) {
		// The first `duty` of the cycle maps onto the first half of the wave; identity at 0.5.
		const double t = phase_ < duty
			? (phase_ / duty) * 0.5
			: 0.5 + ((phase_ - duty) / (1.0 - duty)) * 0.5;

		// Raised cosine: 0 at both ends of the cycle, 1 in the middle, so the gain never steps.
		const double w = 0.5 * (1.0 - std::cos(t * Math_TAU));
		const float gain = (float)std::clamp(1.0 - depth * w, 0.0, 1.0);

		dst[i].left = in[i].left * gain;
		dst[i].right = in[i].right * gain;

		phase_ += step;
		if (phase_ >= 1.0) phase_ -= std::floor(phase_);
	}
}
