#include "lcu/player/mouse_look.h"

#include <gtest/gtest.h>

using lcu::player::mouse_look_delta;

// Phase 74 Bug 1: moving the mouse right (positive mouse_delta_x)
// visibly turned the camera left before this fix - the real, exact
// verification the bug report itself asked for.
TEST(MouseLook, PositiveMouseDeltaXProducesNegativeYaw) {
    const auto delta = mouse_look_delta(/*mouse_delta_x=*/10.0f, /*mouse_delta_y=*/0.0f, /*sensitivity=*/0.002f);
    EXPECT_LT(delta.yaw, 0.0f);
}

TEST(MouseLook, NegativeMouseDeltaXProducesPositiveYaw) {
    const auto delta = mouse_look_delta(/*mouse_delta_x=*/-10.0f, /*mouse_delta_y=*/0.0f, /*sensitivity=*/0.002f);
    EXPECT_GT(delta.yaw, 0.0f);
}

// Mouse moving down (positive mouse_delta_y) must look down, i.e.
// decrease pitch - unchanged by this bugfix, still asserted so a future
// regression here is caught too.
TEST(MouseLook, PositiveMouseDeltaYProducesNegativePitch) {
    const auto delta = mouse_look_delta(/*mouse_delta_x=*/0.0f, /*mouse_delta_y=*/10.0f, /*sensitivity=*/0.002f);
    EXPECT_LT(delta.pitch, 0.0f);
}

TEST(MouseLook, MagnitudeScalesLinearlyWithSensitivity) {
    const auto low = mouse_look_delta(10.0f, 5.0f, 0.001f);
    const auto high = mouse_look_delta(10.0f, 5.0f, 0.002f);
    EXPECT_FLOAT_EQ(high.yaw, low.yaw * 2.0f);
    EXPECT_FLOAT_EQ(high.pitch, low.pitch * 2.0f);
}

TEST(MouseLook, ZeroDeltaProducesZeroYawAndPitch) {
    const auto delta = mouse_look_delta(0.0f, 0.0f, 0.002f);
    EXPECT_FLOAT_EQ(delta.yaw, 0.0f);
    EXPECT_FLOAT_EQ(delta.pitch, 0.0f);
}
