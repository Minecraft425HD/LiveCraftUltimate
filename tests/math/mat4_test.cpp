#include "lcu/math/mat4.h"

#include <gtest/gtest.h>

using lcu::math::Mat4;
using lcu::math::Vec3;

namespace {

void expect_mat4_near(const Mat4& a, const Mat4& b, float eps = 1e-5f) {
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(a.m[i], b.m[i], eps) << "at index " << i;
    }
}

}  // namespace

TEST(Mat4, IdentityTimesIdentityIsIdentity) {
    const Mat4 identity = Mat4::identity();
    expect_mat4_near(identity * identity, Mat4::identity());
}

TEST(Mat4, TranslationMovesOrigin) {
    const Mat4 t = Mat4::translation(Vec3{1.0f, 2.0f, 3.0f});
    EXPECT_FLOAT_EQ(t.m[12], 1.0f);
    EXPECT_FLOAT_EQ(t.m[13], 2.0f);
    EXPECT_FLOAT_EQ(t.m[14], 3.0f);
}

TEST(Mat4, TranslationComposition) {
    const Mat4 t1 = Mat4::translation(Vec3{1.0f, 0.0f, 0.0f});
    const Mat4 t2 = Mat4::translation(Vec3{0.0f, 2.0f, 0.0f});
    const Mat4 combined = t1 * t2;

    // Combined translation should move by (1, 2, 0).
    EXPECT_FLOAT_EQ(combined.m[12], 1.0f);
    EXPECT_FLOAT_EQ(combined.m[13], 2.0f);
}

TEST(Mat4, LookAtProducesOrthonormalBasis) {
    const Mat4 view = Mat4::look_at(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});

    // Looking down -Z from +Z with Y up should be close to identity
    // rotation, only translated.
    EXPECT_NEAR(view.m[0], 1.0f, 1e-5f);
    EXPECT_NEAR(view.m[5], 1.0f, 1e-5f);
    EXPECT_NEAR(view.m[10], 1.0f, 1e-5f);
}

TEST(Mat4, PerspectiveHasNegativeWComponent) {
    const Mat4 proj = Mat4::perspective(1.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    EXPECT_FLOAT_EQ(proj.m[11], -1.0f);
}

TEST(Mat4, OrthographicMapsScreenCornersToClipSpaceCorners) {
    // Real screen-space convention (Phase 44's UI use: top=0, bottom=
    // height, y increasing downward, matching SDL/mouse coordinates) -
    // the top-left pixel should land at clip-space (-1, 1) (top-left in
    // GL's y-up NDC), the bottom-right pixel at (1, -1).
    constexpr lcu::f32 kWidth = 1280.0f;
    constexpr lcu::f32 kHeight = 720.0f;
    const Mat4 proj = Mat4::orthographic(0.0f, kWidth, kHeight, 0.0f, -1.0f, 1.0f);

    const auto transform_x = [&](lcu::f32 x) { return proj.m[0] * x + proj.m[12]; };
    const auto transform_y = [&](lcu::f32 y) { return proj.m[5] * y + proj.m[13]; };

    EXPECT_NEAR(transform_x(0.0f), -1.0f, 1e-5f);
    EXPECT_NEAR(transform_x(kWidth), 1.0f, 1e-5f);
    EXPECT_NEAR(transform_y(0.0f), 1.0f, 1e-5f) << "screen y=0 (top) should map to clip-space y=1 (top)";
    EXPECT_NEAR(transform_y(kHeight), -1.0f, 1e-5f) << "screen y=height (bottom) should map to clip-space y=-1 "
                                                        "(bottom)";
}

TEST(Mat4, OrthographicCenterMapsToClipSpaceOrigin) {
    constexpr lcu::f32 kWidth = 800.0f;
    constexpr lcu::f32 kHeight = 600.0f;
    const Mat4 proj = Mat4::orthographic(0.0f, kWidth, kHeight, 0.0f, -1.0f, 1.0f);

    const lcu::f32 x_ndc = proj.m[0] * (kWidth / 2.0f) + proj.m[12];
    const lcu::f32 y_ndc = proj.m[5] * (kHeight / 2.0f) + proj.m[13];
    EXPECT_NEAR(x_ndc, 0.0f, 1e-5f);
    EXPECT_NEAR(y_ndc, 0.0f, 1e-5f);
}
