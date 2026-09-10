#pragma once

#include <vector>

#include "lcu/audio/positional.h"
#include "lcu/core/types.h"

// Forward-declared, not included: engine/audio is the only place
// <SDL3/SDL_audio.h> is actually #included (in audio_engine.cpp),
// mirroring engine/platform's Window being the only place SDL's window/
// event headers leak, and engine/scripting's Lua-header confinement - see
// DECISIONS.md.
struct SDL_AudioStream;

namespace lcu::audio {

// Thin wrapper around one SDL3 audio playback device+stream (Phase 12).
// Only meaningful on the client - VoxelServer never constructs one (no
// speakers on a headless dedicated server - mirrors engine/platform's
// LCU_BUILD_CLIENT gating, see ARCHITECTURE.md).
class AudioEngine : public NonCopyable {
   public:
    AudioEngine() = default;
    ~AudioEngine();

    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    // Opens the default playback device at 44.1kHz mono-source/stereo-
    // output float. Returns false (logged) on failure - a headless/no-
    // audio-device environment (this sandbox, most CI) is expected to
    // fail here; callers treat that as "no sound this session," not
    // fatal (see VoxelClient).
    bool init();

    // Applies `gain` per channel to `mono_samples` (see waveform.h for a
    // real source of samples - generate_sine_wave) and queues the
    // resulting interleaved stereo buffer for playback. No-op if init()
    // failed or wasn't called.
    void play(const std::vector<f32>& mono_samples, StereoGain gain);

    bool is_initialized() const { return stream_ != nullptr; }

   private:
    SDL_AudioStream* stream_ = nullptr;
};

}  // namespace lcu::audio
