#include "lcu/debug/frame_stats.h"

#include <gtest/gtest.h>

using lcu::debug::FrameStats;

TEST(FrameStats, NoReportBeforeIntervalElapsed) {
    FrameStats stats(1.0f);
    EXPECT_FALSE(stats.update(0.016f).has_value());
    EXPECT_FALSE(stats.update(0.016f).has_value());
}

TEST(FrameStats, ReportsOnceIntervalElapsed) {
    FrameStats stats(1.0f);
    for (int i = 0; i < 60; ++i) {
        stats.update(1.0f / 60.0f);
    }

    const auto report = stats.update(1.0f / 60.0f);
    ASSERT_TRUE(report.has_value());
    EXPECT_NEAR(report->avg_frame_ms, 1000.0f / 60.0f, 0.5f);
    EXPECT_NEAR(report->fps, 60.0f, 1.0f);
}

TEST(FrameStats, TotalFramesAccumulatesAcrossReports) {
    FrameStats stats(0.5f);
    for (int i = 0; i < 100; ++i) {
        stats.update(0.01f);
    }
    EXPECT_EQ(stats.total_frames(), 100u);
}
