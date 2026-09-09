#include "lcu/player/movement_input.h"

#include <cmath>

#include <gtest/gtest.h>

using lcu::math::Vec3;
using lcu::platform::Action;
using lcu::platform::InputState;
using lcu::player::FirstPersonCamera;
using lcu::player::movement_direction_from_input;

namespace {
constexpr float kPi = 3.14159265358979323846f;
}  // namespace

TEST(MovementInput, NoActionsHeldReturnsZeroVector) {
    InputState input;
    FirstPersonCamera camera;

    const Vec3 dir = movement_direction_from_input(input, camera);

    EXPECT_FLOAT_EQ(dir.x, 0.0f);
    EXPECT_FLOAT_EQ(dir.y, 0.0f);
    EXPECT_FLOAT_EQ(dir.z, 0.0f);
}

TEST(MovementInput, ForwardHeldMatchesCameraForwardHorizontal) {
    InputState input;
    input.set_down(Action::MoveForward, true);
    FirstPersonCamera camera;

    const Vec3 dir = movement_direction_from_input(input, camera);

    EXPECT_NEAR(dir.x, 0.0f, 1e-5f);
    EXPECT_NEAR(dir.z, -1.0f, 1e-5f);
}

TEST(MovementInput, ForwardAndBackwardCancelOut) {
    InputState input;
    input.set_down(Action::MoveForward, true);
    input.set_down(Action::MoveBackward, true);
    FirstPersonCamera camera;

    const Vec3 dir = movement_direction_from_input(input, camera);

    EXPECT_FLOAT_EQ(dir.x, 0.0f);
    EXPECT_FLOAT_EQ(dir.z, 0.0f);
}

TEST(MovementInput, StrafeRightMatchesCameraRight) {
    InputState input;
    input.set_down(Action::MoveRight, true);
    FirstPersonCamera camera;

    const Vec3 dir = movement_direction_from_input(input, camera);

    EXPECT_NEAR(dir.x, 1.0f, 1e-5f);
    EXPECT_NEAR(dir.z, 0.0f, 1e-5f);
}

TEST(MovementInput, DiagonalMovementIsNormalized) {
    InputState input;
    input.set_down(Action::MoveForward, true);
    input.set_down(Action::MoveRight, true);
    FirstPersonCamera camera;

    const Vec3 dir = movement_direction_from_input(input, camera);

    const float magnitude = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    EXPECT_NEAR(magnitude, 1.0f, 1e-5f);
}

TEST(MovementInput, YawRotatesMovementDirection) {
    InputState input;
    input.set_down(Action::MoveForward, true);
    FirstPersonCamera camera;
    camera.add_yaw_pitch(kPi * 0.5f, 0.0f);

    const Vec3 dir = movement_direction_from_input(input, camera);

    EXPECT_NEAR(dir.x, -1.0f, 1e-4f);
    EXPECT_NEAR(dir.z, 0.0f, 1e-4f);
}
