#pragma once

#include <vector>

#include "lcu/core/types.h"

namespace lcu::audio {

// Generates one channel's worth of PCM samples for a pure sine tone, in
// the [-1, 1] range (Phase 12). engine/audio has no WAV/asset-loading
// pipeline yet, and any checked-in audio asset would need to be this
// project's own work anyway (GPL-3.0/own-IP-only, no Minecraft assets -
// brief section 12) - a procedurally generated tone is real, immediately
// playable audio content that is trivially this project's own IP, not a
// silent placeholder standing in for a future asset.
std::vector<f32> generate_sine_wave(f32 frequency_hz, f32 duration_seconds, u32 sample_rate);

}  // namespace lcu::audio
