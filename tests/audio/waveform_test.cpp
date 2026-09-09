#include "lcu/audio/waveform.h"

#include <cmath>

#include <gtest/gtest.h>

namespace lcu::audio {
namespace {

TEST(GenerateSineWave, ProducesExpectedSampleCount) {
    const auto samples = generate_sine_wave(440.0f, 1.0f, 44100);
    EXPECT_EQ(samples.size(), 44100u);
}

TEST(GenerateSineWave, SampleCountScalesWithDuration) {
    const auto samples = generate_sine_wave(440.0f, 0.5f, 44100);
    EXPECT_EQ(samples.size(), 22050u);
}

TEST(GenerateSineWave, ZeroDurationProducesNoSamples) {
    const auto samples = generate_sine_wave(440.0f, 0.0f, 44100);
    EXPECT_TRUE(samples.empty());
}

TEST(GenerateSineWave, FirstSampleIsZero) {
    // sin(2*pi*f*0) == 0 regardless of frequency - a real, exact check on
    // the actual generated waveform, not just its length.
    const auto samples = generate_sine_wave(440.0f, 1.0f, 44100);
    ASSERT_FALSE(samples.empty());
    EXPECT_NEAR(samples[0], 0.0f, 1e-6f);
}

TEST(GenerateSineWave, StaysWithinUnitAmplitude) {
    const auto samples = generate_sine_wave(261.63f, 1.0f, 44100);  // middle C
    for (f32 sample : samples) {
        EXPECT_GE(sample, -1.0f);
        EXPECT_LE(sample, 1.0f);
    }
}

TEST(GenerateSineWave, OneSecondAt1HzCompletesExactlyOneCycle) {
    // A 1 Hz tone sampled for 1 full second should cross zero going
    // upward at the start and be back near zero (completing the cycle)
    // just before the buffer ends.
    const auto samples = generate_sine_wave(1.0f, 1.0f, 44100);
    ASSERT_FALSE(samples.empty());
    EXPECT_NEAR(samples.front(), 0.0f, 1e-6f);
    EXPECT_NEAR(samples.back(), std::sin(6.28318530718f * 1.0f * (44099.0f / 44100.0f)), 1e-4f);
}

TEST(GenerateSineWave, DifferentFrequenciesProduceDifferentWaveforms) {
    const auto low = generate_sine_wave(220.0f, 0.01f, 44100);
    const auto high = generate_sine_wave(880.0f, 0.01f, 44100);
    ASSERT_EQ(low.size(), high.size());
    bool any_different = false;
    for (usize i = 0; i < low.size(); ++i) {
        if (std::abs(low[i] - high[i]) > 1e-4f) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

}  // namespace
}  // namespace lcu::audio
