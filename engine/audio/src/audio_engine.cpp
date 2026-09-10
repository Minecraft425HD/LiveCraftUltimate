#include "lcu/audio/audio_engine.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include "lcu/core/log.h"

namespace lcu::audio {

namespace {
constexpr int kSampleRate = 44100;
}  // namespace

AudioEngine::~AudioEngine() {
    if (stream_ != nullptr) {
        // Destroying an SDL_AudioStream created via SDL_OpenAudioDeviceStream
        // also closes the device it opened alongside it.
        SDL_DestroyAudioStream(stream_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

bool AudioEngine::init() {
    // Unlike SDL_INIT_VIDEO (initialized once by engine/platform::Window),
    // nothing else in this codebase needs SDL_INIT_AUDIO - initialize it
    // here, reference-counted by SDL itself same as Window does for video.
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        LCU_LOG_WARN("AudioEngine::init: SDL_InitSubSystem(SDL_INIT_AUDIO) failed ({}) - continuing without audio",
                     SDL_GetError());
        return false;
    }

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = kSampleRate;

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (stream_ == nullptr) {
        LCU_LOG_WARN("AudioEngine::init: SDL_OpenAudioDeviceStream failed ({}) - continuing without audio",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    if (!SDL_ResumeAudioStreamDevice(stream_)) {
        LCU_LOG_WARN("AudioEngine::init: SDL_ResumeAudioStreamDevice failed ({})", SDL_GetError());
    }

    LCU_LOG_INFO("AudioEngine initialized: {} Hz, stereo float", kSampleRate);
    return true;
}

void AudioEngine::play(const std::vector<f32>& mono_samples, StereoGain gain) {
    if (stream_ == nullptr) {
        return;
    }

    std::vector<f32> interleaved(mono_samples.size() * 2);
    for (usize i = 0; i < mono_samples.size(); ++i) {
        interleaved[i * 2] = mono_samples[i] * gain.left;
        interleaved[i * 2 + 1] = mono_samples[i] * gain.right;
    }

    const int byte_size = static_cast<int>(interleaved.size() * sizeof(f32));
    if (!SDL_PutAudioStreamData(stream_, interleaved.data(), byte_size)) {
        LCU_LOG_WARN("AudioEngine::play: SDL_PutAudioStreamData failed ({})", SDL_GetError());
    }
}

}  // namespace lcu::audio
