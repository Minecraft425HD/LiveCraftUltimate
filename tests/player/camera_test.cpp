#include "lcu/player/camera.h"

#include <gtest/gtest.h>

using lcu::math::Vec3;
using lcu::player::FirstPersonCamera;

namespace {
constexpr float kPi = 3.14159265358979323846f;
}  // namespace

TEST(FirstPersonCamera, DefaultForwardLooksDownNegativeZ) {
    FirstPersonCamera camera;
    const Vec3 f = camera.forward();
    EXPECT_NEAR(f.x, 0.0f, 1e-5f);
    EXPECT_NEAR(f.y, 0.0f, 1e-5f);
    EXPECT_NEAR(f.z, -1.0f, 1e-5f);
}

TEST(FirstPersonCamera, DefaultRightIsPositiveX) {
    FirstPersonCamera camera;
    const Vec3 r = camera.right();
    EXPECT_NEAR(r.x, 1.0f, 1e-5f);
    EXPECT_NEAR(r.y, 0.0f, 1e-5f);
    EXPECT_NEAR(r.z, 0.0f, 1e-5f);
}

TEST(FirstPersonCamera, YawNinetyDegreesFacesNegativeX) {
    FirstPersonCamera camera;
    camera.add_yaw_pitch(kPi * 0.5f, 0.0f);

    const Vec3 f = camera.forward();
    EXPECT_NEAR(f.x, -1.0f, 1e-4f);
    EXPECT_NEAR(f.y, 0.0f, 1e-4f);
    EXPECT_NEAR(f.z, 0.0f, 1e-4f);
}

TEST(FirstPersonCamera, PitchClampsBeforeReachingStraightUp) {
    FirstPersonCamera camera;
    camera.add_yaw_pitch(0.0f, 100.0f);  // absurdly large, must clamp

    EXPECT_LT(camera.pitch, kPi * 0.5f);
    EXPECT_GT(camera.pitch, 0.0f);
}

TEST(FirstPersonCamera, PitchClampsBeforeReachingStraightDown) {
    FirstPersonCamera camera;
    camera.add_yaw_pitch(0.0f, -100.0f);

    EXPECT_GT(camera.pitch, -kPi * 0.5f);
    EXPECT_LT(camera.pitch, 0.0f);
}

TEST(FirstPersonCamera, ForwardHorizontalIgnoresPitch) {
    FirstPersonCamera level;
    FirstPersonCamera pitched;
    pitched.add_yaw_pitch(0.0f, 0.7f);

    const Vec3 fh_level = level.forward_horizontal();
    const Vec3 fh_pitched = pitched.forward_horizontal();

    EXPECT_NEAR(fh_level.x, fh_pitched.x, 1e-5f);
    EXPECT_NEAR(fh_level.z, fh_pitched.z, 1e-5f);
    EXPECT_FLOAT_EQ(fh_pitched.y, 0.0f);
}

TEST(FirstPersonCamera, ViewMatrixAtOriginLookingForwardIsNearIdentityRotation) {
    FirstPersonCamera camera;  // position (0,0,0), looking down -Z, up +Y
    const lcu::math::Mat4 view = camera.view_matrix();

    EXPECT_NEAR(view.m[0], 1.0f, 1e-4f);
    EXPECT_NEAR(view.m[5], 1.0f, 1e-4f);
    EXPECT_NEAR(view.m[10], 1.0f, 1e-4f);
}
