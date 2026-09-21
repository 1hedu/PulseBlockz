// Audio volume curves, shared by the runtime's GetAudibilityFor and the host mixer.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace pulseblockz::rbx::audio {

inline constexpr double kPi = 3.14159265358979323846;

// SetDistanceAttenuation / SetAngleAttenuation points, sorted by key; replicates as a hidden "key=volume;" string on the instance.
using Curve = std::vector<std::pair<double, double>>;

inline Curve parseCurve(const std::string& s) {
    Curve c;
    size_t at = 0;
    while (at < s.size()) {
        size_t end = s.find(';', at);
        if (end == std::string::npos) end = s.size();
        double k = 0, v = 0;
        if (std::sscanf(s.substr(at, end - at).c_str(), "%lf=%lf", &k, &v) == 2) c.push_back({k, v});
        at = end + 1;
    }
    std::sort(c.begin(), c.end());
    return c;
}

inline std::string formatCurve(const Curve& c) {
    std::string out;
    char buf[64];
    for (auto& [k, v] : c) { std::snprintf(buf, sizeof buf, "%.9g=%.9g;", k, v); out += buf; }
    return out;
}

// Linear between points, flat beyond the ends, as Roblox specifies.
inline double sampleCurve(const Curve& c, double x, double fallback) {
    if (c.empty()) return fallback;
    if (x <= c.front().first) return c.front().second;
    if (x >= c.back().first) return c.back().second;
    for (size_t i = 1; i < c.size(); i++) {
        if (x > c[i].first) continue;
        const auto& a = c[i - 1];
        const auto& b = c[i];
        const double span = b.first - a.first;
        return span > 0 ? a.second + (b.second - a.second) * (x - a.first) / span : b.second;
    }
    return c.back().second;
}

// AudioEmitter gain at `d` studs; `lo`/`hi` are DistanceAttenuationBounds. Custom with no curve
// is inverse-square from the near bound, so gain is full inside it, not unbounded at the emitter.
inline double emitterDistanceGain(const std::string& mode, double lo, double hi, const Curve& curve, double d) {
    lo = std::max(lo, 0.0);
    hi = std::max(hi, lo + 1e-3);
    if (mode == "Custom" || mode.empty()) {
        if (!curve.empty()) return std::clamp(sampleCurve(curve, d, 1), 0.0, 1.0);
        const double ref = std::max(lo, 1.0);
        return d <= ref ? 1.0 : (ref * ref) / (d * d);
    }
    if (d <= lo) return 1;
    if (d >= hi) return 0;
    const double linear = (hi - d) / (hi - lo);
    if (mode == "Linear") return linear;
    if (mode == "LinearSquared") return linear * linear;
    if (mode == "Inverse") return lo > 0 ? lo / d : 1;
    return std::min(lo > 0 ? lo / d : 1, linear * linear);   // InverseTapered
}

// RBJ biquad gain in dB at `f` Hz, for AudioFilter:GetGainAt; the shapes the host mixer builds.
inline double filterGainAt(const std::string& type, double f0, double gainDb, double q, double f) {
    const double fs = 48000.0;
    f0 = std::clamp(f0, 20.0, 22000.0);
    q = std::clamp(q, 0.1, 10.0);
    const double w0 = 2 * kPi * f0 / fs, cw = std::cos(w0), sw = std::sin(w0);
    const double A = std::pow(10.0, gainDb / 40.0);
    double alpha = sw / (2 * q);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    int stages = 1;
    std::string t = type;
    if (t == "Lowpass24dB" || t == "Highpass24dB") stages = 2;
    if (t == "Lowpass48dB" || t == "Highpass48dB") stages = 4;
    if (t == "Lowpass6dB") {
        // One pole: |H| = 1 / sqrt(1 + (f/f0)^2).
        return -10.0 * std::log10(1.0 + (f / f0) * (f / f0));
    }
    if (t.rfind("Lowpass", 0) == 0) { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; }
    else if (t.rfind("Highpass", 0) == 0) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; }
    else if (t == "Bandpass") { b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; }
    else if (t == "Notch") { b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; }
    else if (t == "LowShelf" || t == "HighShelf") {
        const double s = 2 * std::sqrt(A) * (sw / (2 * q));
        const double sign = t == "LowShelf" ? 1 : -1;
        b0 = A * ((A + 1) - sign * (A - 1) * cw + s);
        b1 = sign * 2 * A * ((A - 1) - sign * (A + 1) * cw);
        b2 = A * ((A + 1) - sign * (A - 1) * cw - s);
        a0 = (A + 1) + sign * (A - 1) * cw + s;
        a1 = -sign * 2 * ((A - 1) + sign * (A + 1) * cw);
        a2 = (A + 1) + sign * (A - 1) * cw - s;
    } else {   // Peak
        b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
    }
    const double w = 2 * kPi * f / fs;
    // |H(e^jw)|^2 for one stage.
    const double cr = std::cos(w), sr = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
    const double nr = b0 + b1 * cr + b2 * c2, ni = -(b1 * sr + b2 * s2);
    const double dr = a0 + a1 * cr + a2 * c2, di = -(a1 * sr + a2 * s2);
    const double mag2 = (nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-30);
    return stages * 10.0 * std::log10(std::max(mag2, 1e-30));
}

// The channels of an AudioChannelLayout, in order: an AudioChannelSplitter's output pins and an
// AudioChannelMixer's input pins. Stereo's and Quad's names are the ones Roblox's docs wire; the
// wider layouts follow the same naming and are not verified against Roblox.
inline std::vector<std::string> channelPins(const std::string& layout) {
    if (layout == "Mono") return {"Mono"};
    if (layout == "Quad") return {"Left", "Right", "BackLeft", "BackRight"};
    if (layout == "Surround_5") return {"Left", "Right", "Center", "BackLeft", "BackRight"};
    if (layout == "Surround_5_1") return {"Left", "Right", "Center", "LFE", "BackLeft", "BackRight"};
    if (layout == "Surround_7_1") return {"Left", "Right", "Center", "LFE", "BackLeft", "BackRight", "SideLeft", "SideRight"};
    if (layout == "Surround_7_1_4") return {"Left", "Right", "Center", "LFE", "BackLeft", "BackRight", "SideLeft", "SideRight",
                                            "TopFrontLeft", "TopFrontRight", "TopBackLeft", "TopBackRight"};
    return {"Left", "Right"};   // Stereo
}

// The Audio API classes with an Input and an Output pin: a stream passes through them.
inline bool effectClass(const std::string& c) {
    return c == "AudioFilter" || c == "AudioReverb" || c == "AudioFader" || c == "AudioEcho" || c == "AudioCompressor"
        || c == "AudioLimiter" || c == "AudioDistortion" || c == "AudioEqualizer" || c == "AudioChorus" || c == "AudioFlanger"
        || c == "AudioGate" || c == "AudioPitchShifter" || c == "AudioTremolo";
}

}  // namespace pulseblockz::rbx::audio
