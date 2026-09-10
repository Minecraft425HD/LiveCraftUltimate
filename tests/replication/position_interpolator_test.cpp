#include "lcu/replication/position_interpolator.h"

#include <gtest/gtest.h>

using lcu::replication::PositionInterpolator;

TEST(PositionInterpolator, NoSamplesReturnsZeroVector) {
    PositionInterpolator interp;
    const auto pos = interp.interpolated_position(1.0f);
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
    EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

TEST(PositionInterpolator, SingleSampleReturnsThatSampleRegardlessOfTime) {
    PositionInterpolator interp(0.1f);
    interp.add_sample(1.0f, {5.0f, 0.0f, 0.0f});

    EXPECT_FLOAT_EQ(interp.interpolated_position(0.0f).x, 5.0f);
    EXPECT_FLOAT_EQ(interp.interpolated_position(100.0f).x, 5.0f);
}

TEST(PositionInterpolator, InterpolatesLinearlyBetweenTwoSamples) {
    PositionInterpolator interp(0.0f);  // no render delay, for simple arithmetic
    interp.add_sample(0.0f, {0.0f, 0.0f, 0.0f});
    interp.add_sample(1.0f, {10.0f, 0.0f, 0.0f});

    const auto midpoint = interp.interpolated_position(0.5f);
    EXPECT_NEAR(midpoint.x, 5.0f, 1e-4f);

    const auto quarter = interp.interpolated_position(0.25f);
    EXPECT_NEAR(quarter.x, 2.5f, 1e-4f);
}

TEST(PositionInterpolator, RenderDelayShiftsTheEffectiveQueryTime) {
    PositionInterpolator interp(0.2f);  // render 0.2s in the past
    interp.add_sample(0.0f, {0.0f, 0.0f, 0.0f});
    interp.add_sample(1.0f, {10.0f, 0.0f, 0.0f});

    // Querying at t=0.7 with a 0.2s delay is equivalent to an undelayed
    // query at t=0.5 - the midpoint.
    const auto pos = interp.interpolated_position(0.7f);
    EXPECT_NEAR(pos.x, 5.0f, 1e-4f);
}

TEST(PositionInterpolator, ClampsToEarliestSampleBeforeTheBuffer) {
    PositionInterpolator interp(0.0f);
    interp.add_sample(5.0f, {1.0f, 0.0f, 0.0f});
    interp.add_sample(6.0f, {2.0f, 0.0f, 0.0f});

    const auto pos = interp.interpolated_position(0.0f);  // well before the first sample
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
}

TEST(PositionInterpolator, ClampsToLatestSampleRatherThanExtrapolating) {
    PositionInterpolator interp(0.0f);
    interp.add_sample(0.0f, {0.0f, 0.0f, 0.0f});
    interp.add_sample(1.0f, {10.0f, 0.0f, 0.0f});

    // Query far past the newest sample - should hold, not keep going in
    // the direction the last segment was moving.
    const auto pos = interp.interpolated_position(100.0f);
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
}

TEST(PositionInterpolator, InterpolatesAcrossMultipleSegments) {
    PositionInterpolator interp(0.0f);
    interp.add_sample(0.0f, {0.0f, 0.0f, 0.0f});
    interp.add_sample(1.0f, {10.0f, 0.0f, 0.0f});
    interp.add_sample(2.0f, {10.0f, 10.0f, 0.0f});  // second segment moves in y instead

    const auto in_first_segment = interp.interpolated_position(0.5f);
    EXPECT_NEAR(in_first_segment.x, 5.0f, 1e-4f);
    EXPECT_NEAR(in_first_segment.y, 0.0f, 1e-4f);

    const auto in_second_segment = interp.interpolated_position(1.5f);
    EXPECT_NEAR(in_second_segment.x, 10.0f, 1e-4f);
    EXPECT_NEAR(in_second_segment.y, 5.0f, 1e-4f);
}

TEST(PositionInterpolator, SampleCountReflectsBufferedSamples) {
    PositionInterpolator interp;
    EXPECT_EQ(interp.sample_count(), 0u);
    interp.add_sample(0.0f, {0.0f, 0.0f, 0.0f});
    interp.add_sample(1.0f, {1.0f, 0.0f, 0.0f});
    EXPECT_EQ(interp.sample_count(), 2u);
}

TEST(PositionInterpolator, BufferEvictsOldestSampleBeyondMaxCount) {
    PositionInterpolator interp(0.0f);
    // kMaxBufferedSamples is 32 - push well past it and confirm the
    // buffer doesn't grow unbounded and the earliest sample is gone.
    for (int i = 0; i < 40; ++i) {
        interp.add_sample(static_cast<float>(i), {static_cast<float>(i), 0.0f, 0.0f});
    }
    EXPECT_LE(interp.sample_count(), 32u);
    // The very first sample (t=0) should have been evicted.
    EXPECT_GT(interp.interpolated_position(0.0f).x, 0.0f);
}
