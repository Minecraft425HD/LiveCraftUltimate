#include "lcu/audio/positional.h"

#include <gtest/gtest.h>

namespace lcu::audio {
namespace {

using math::Vec3;

TEST(ComputeStereoPan, SourceDirectlyAheadPansCenter) {
    const StereoGain gain = compute_stereo_pan({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f});
    EXPECT_NEAR(gain.left, 1.0f, 1e-5f);
    EXPECT_NEAR(gain.right, 1.0f, 1e-5f);
}

TEST(ComputeStereoPan, SourceFullyToTheRightMutesLeftEar) {
    const StereoGain gain = compute_stereo_pan({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f});
    EXPECT_NEAR(gain.left, 0.0f, 1e-5f);
    EXPECT_NEAR(gain.right, 1.0f, 1e-5f);
}

TEST(ComputeStereoPan, SourceFullyToTheLeftMutesRightEar) {
    const StereoGain gain = compute_stereo_pan({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {-5.0f, 0.0f, 0.0f});
    EXPECT_NEAR(gain.left, 1.0f, 1e-5f);
    EXPECT_NEAR(gain.right, 0.0f, 1e-5f);
}

TEST(ComputeStereoPan, SourceAtHalfRightAngleGivesPartialGainOnBothEars) {
    const StereoGain gain = compute_stereo_pan({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, -1.0f});
    // 45 degrees to the right: pan = dot(normalize(1,0,-1), (1,0,0)) = 1/sqrt(2).
    const f32 expected_pan = 0.70710678f;
    EXPECT_NEAR(gain.left, 1.0f - expected_pan, 1e-4f);
    EXPECT_NEAR(gain.right, 1.0f, 1e-4f);
}

TEST(ComputeStereoPan, SourceAtListenerPositionPansCenterWithoutDividingByZero) {
    const StereoGain gain = compute_stereo_pan({3.0f, 1.0f, 2.0f}, {1.0f, 0.0f, 0.0f}, {3.0f, 1.0f, 2.0f});
    EXPECT_NEAR(gain.left, 1.0f, 1e-5f);
    EXPECT_NEAR(gain.right, 1.0f, 1e-5f);
}

TEST(DistanceAttenuation, ZeroDistanceIsFullVolume) {
    EXPECT_FLOAT_EQ(distance_attenuation(0.0f, 20.0f), 1.0f);
}

TEST(DistanceAttenuation, AtMaxDistanceIsSilent) {
    EXPECT_NEAR(distance_attenuation(20.0f, 20.0f), 0.0f, 1e-5f);
}

TEST(DistanceAttenuation, BeyondMaxDistanceClampsToZeroNotNegative) {
    EXPECT_FLOAT_EQ(distance_attenuation(100.0f, 20.0f), 0.0f);
}

TEST(DistanceAttenuation, HalfwayIsHalfVolume) {
    EXPECT_NEAR(distance_attenuation(10.0f, 20.0f), 0.5f, 1e-5f);
}

TEST(DistanceAttenuation, ZeroMaxDistanceIsSilentUnlessSourceIsExactlyAtTheListener) {
    EXPECT_FLOAT_EQ(distance_attenuation(0.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(distance_attenuation(5.0f, 0.0f), 0.0f);
}

}  // namespace
}  // namespace lcu::audio
