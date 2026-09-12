#include <gtest/gtest.h>

#include "lcu/math/mat4.h"
#include "lcu/math/vec3.h"
#include "lcu/physics/aabb.h"
#include "lcu/rendering/frustum.h"

using lcu::math::Mat4;
using lcu::math::Vec3;
using lcu::physics::AABB;
using lcu::rendering::Frustum;

namespace {

// A real 90-degree-vertical-FOV, 1:1-aspect perspective frustum, camera
// at the world origin looking down -Z (the same "identity-ish" forward
// direction Mat4::look_at produces for eye=(0,0,0), target=(0,0,-1)) -
// matches the real per-frame vp = proj * view this project's own render
// loop will build (see Frustum::from_view_projection's own doc comment
// on why that multiplication order matters).
Frustum origin_frustum_looking_down_negative_z() {
    const Mat4 view = Mat4::look_at({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 proj = Mat4::perspective(1.5708f /* ~90 deg */, 1.0f, 0.1f, 100.0f);
    return Frustum::from_view_projection(proj * view);
}

AABB unit_box_at(const Vec3& center) {
    return AABB{center - Vec3{0.5f, 0.5f, 0.5f}, center + Vec3{0.5f, 0.5f, 0.5f}};
}

}  // namespace

TEST(Frustum, BoxDirectlyAheadIsVisible) {
    const Frustum frustum = origin_frustum_looking_down_negative_z();
    EXPECT_TRUE(frustum.contains_aabb(unit_box_at({0.0f, 0.0f, -10.0f})));
}

TEST(Frustum, BoxBehindTheCameraIsNotVisible) {
    const Frustum frustum = origin_frustum_looking_down_negative_z();
    // +Z is behind the camera (it looks toward -Z) - well outside the
    // near plane on the wrong side entirely.
    EXPECT_FALSE(frustum.contains_aabb(unit_box_at({0.0f, 0.0f, 10.0f})));
}

TEST(Frustum, BoxFarOutsideTheHorizontalFovIsNotVisible) {
    const Frustum frustum = origin_frustum_looking_down_negative_z();
    // Directly ahead in depth, but 1000 units to the side - a real
    // 90-degree FOV frustum at z=-10 is only ~20 units wide, so this is
    // real, unambiguously outside the left/right planes.
    EXPECT_FALSE(frustum.contains_aabb(unit_box_at({1000.0f, 0.0f, -10.0f})));
}

TEST(Frustum, BoxBeyondTheFarPlaneIsNotVisible) {
    const Frustum frustum = origin_frustum_looking_down_negative_z();
    EXPECT_FALSE(frustum.contains_aabb(unit_box_at({0.0f, 0.0f, -1000.0f})));
}

TEST(Frustum, BoxCloserThanTheNearPlaneIsNotVisible) {
    const Frustum frustum = origin_frustum_looking_down_negative_z();
    // z=-0.01 sits entirely between the camera and the real near plane
    // at z=-0.1 - real, deliberately excluded geometry, the same way a
    // real renderer's own near-clip would reject it.
    EXPECT_FALSE(frustum.contains_aabb(AABB{{-0.005f, -0.005f, -0.015f}, {0.005f, 0.005f, -0.005f}}));
}

TEST(Frustum, BoxStraddlingAPlaneBoundaryStillCountsAsVisible) {
    const Frustum frustum = origin_frustum_looking_down_negative_z();
    // A real box centered right at the far plane (z=-100), half-extent
    // 1 - half inside, half outside. contains_aabb's own real contract
    // is conservative (only a box with ALL 8 corners outside one plane
    // is rejected), so a genuinely-straddling box must stay visible.
    EXPECT_TRUE(frustum.contains_aabb(unit_box_at({0.0f, 0.0f, -100.0f})));
}

TEST(Frustum, HugeBoxContainingTheWholeFrustumIsVisible) {
    const Frustum frustum = origin_frustum_looking_down_negative_z();
    EXPECT_TRUE(frustum.contains_aabb(AABB{{-1000.0f, -1000.0f, -1000.0f}, {1000.0f, 1000.0f, 1000.0f}}));
}

TEST(Frustum, DegenerateViewProjectionAcceptsEverything) {
    // An all-zero vp (no real camera set up yet) - every extracted plane
    // normal is the zero vector, hitting from_view_projection's own
    // degenerate-length fallback (accept-everything), not a crash or a
    // divide-by-zero NaN.
    const Frustum frustum = Frustum::from_view_projection(Mat4{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                                 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}});
    EXPECT_TRUE(frustum.contains_aabb(unit_box_at({0.0f, 0.0f, -10.0f})));
}
