#include "lcu/audio/waveform.h"

#include <algorithm>
#include <cmath>

namespace lcu::audio {

std::vector<f32> generate_sine_wave(f32 frequency_hz, f32 duration_seconds, u32 sample_rate) {
    const auto sample_count = static_cast<usize>(std::max(duration_seconds, 0.0f) * static_cast<f32>(sample_rate));
    std::vector<f32> samples(sample_count);

    constexpr f32 kTwoPi = 6.28318530718f;
    for (usize i = 0; i < sample_count; ++i) {
        const f32 t = static_cast<f32>(i) / static_cast<f32>(sample_rate);
        samples[i] = std::sin(kTwoPi * frequency_hz * t);
    }
    return samples;
}

}  // namespace lcu::audio
