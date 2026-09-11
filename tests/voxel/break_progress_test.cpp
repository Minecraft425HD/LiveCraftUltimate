#include "lcu/voxel/break_progress.h"

#include <gtest/gtest.h>

using lcu::voxel::break_progress_fraction;
using lcu::voxel::is_break_ready;

TEST(BreakProgress, ZeroHeldSecondsIsZeroFraction) {
    EXPECT_FLOAT_EQ(break_progress_fraction(0.0f, 2.0f), 0.0f);
}

TEST(BreakProgress, HalfwayThroughHardnessIsHalfFraction) {
    EXPECT_FLOAT_EQ(break_progress_fraction(1.0f, 2.0f), 0.5f);
}

TEST(BreakProgress, FullyElapsedIsFullFraction) {
    EXPECT_FLOAT_EQ(break_progress_fraction(2.0f, 2.0f), 1.0f);
}

TEST(BreakProgress, OvershootingClampsToOneNotBeyond) {
    EXPECT_FLOAT_EQ(break_progress_fraction(10.0f, 2.0f), 1.0f);
}

TEST(BreakProgress, ZeroHardnessIsAlwaysInstantlyFull) {
    // game:torch's real hardness (Phase 48) - any hold time, including
    // zero, reads as fully progressed.
    EXPECT_FLOAT_EQ(break_progress_fraction(0.0f, 0.0f), 1.0f);
    EXPECT_FLOAT_EQ(break_progress_fraction(5.0f, 0.0f), 1.0f);
}

TEST(BreakProgress, NegativeHardnessIsAlwaysInstantlyFullTooNotADivideByZeroCrash) {
    EXPECT_FLOAT_EQ(break_progress_fraction(0.0f, -1.0f), 1.0f);
}

TEST(BreakProgress, RealPerBlockHardnessDurations) {
    // This phase's own directive's exact table.
    EXPECT_FLOAT_EQ(break_progress_fraction(2.0f, 2.0f), 1.0f) << "stone";
    EXPECT_FLOAT_EQ(break_progress_fraction(1.5f, 1.5f), 1.0f) << "wood";
    EXPECT_FLOAT_EQ(break_progress_fraction(0.5f, 0.5f), 1.0f) << "dirt";
    EXPECT_FLOAT_EQ(break_progress_fraction(0.2f, 0.2f), 1.0f) << "leaves";
    // Not yet fully elapsed for each.
    EXPECT_LT(break_progress_fraction(1.9f, 2.0f), 1.0f) << "stone";
    EXPECT_LT(break_progress_fraction(0.49f, 0.5f), 1.0f) << "dirt";
}

TEST(IsBreakReady, FalseBeforeThresholdTrueAtAndAfter) {
    EXPECT_FALSE(is_break_ready(1.0f, 2.0f));
    EXPECT_TRUE(is_break_ready(2.0f, 2.0f));
    EXPECT_TRUE(is_break_ready(3.0f, 2.0f));
}

TEST(IsBreakReady, ZeroHardnessIsReadyImmediately) {
    EXPECT_TRUE(is_break_ready(0.0f, 0.0f));
}
