#include "lcu/math/vec3.h"

#include <gtest/gtest.h>

using lcu::math::Vec3;

TEST(Vec3, AddSubtract) {
    const Vec3 a{1.0f, 2.0f, 3.0f};
    const Vec3 b{4.0f, 5.0f, 6.0f};

    EXPECT_EQ(a + b, Vec3(5.0f, 7.0f, 9.0f));
    EXPECT_EQ(b - a, Vec3(3.0f, 3.0f, 3.0f));
}

TEST(Vec3, DotProduct) {
    const Vec3 a{1.0f, 0.0f, 0.0f};
    const Vec3 b{0.0f, 1.0f, 0.0f};
    const Vec3 c{2.0f, 3.0f, 4.0f};

    EXPECT_FLOAT_EQ(lcu::math::dot(a, b), 0.0f);
    EXPECT_FLOAT_EQ(lcu::math::dot(c, c), 4.0f + 9.0f + 16.0f);
}

TEST(Vec3, CrossProductIsPerpendicular) {
    const Vec3 x{1.0f, 0.0f, 0.0f};
    const Vec3 y{0.0f, 1.0f, 0.0f};
    const Vec3 z = lcu::math::cross(x, y);

    EXPECT_EQ(z, Vec3(0.0f, 0.0f, 1.0f));
    EXPECT_FLOAT_EQ(lcu::math::dot(z, x), 0.0f);
    EXPECT_FLOAT_EQ(lcu::math::dot(z, y), 0.0f);
}

TEST(Vec3, NormalizeUnitLength) {
    const Vec3 v{3.0f, 4.0f, 0.0f};
    const Vec3 n = lcu::math::normalize(v);

    EXPECT_NEAR(lcu::math::length(n), 1.0f, 1e-6f);
    EXPECT_NEAR(n.x, 0.6f, 1e-6f);
    EXPECT_NEAR(n.y, 0.8f, 1e-6f);
}

TEST(Vec3, NormalizeZeroVectorIsSafe) {
    const Vec3 zero{0.0f, 0.0f, 0.0f};
    const Vec3 n = lcu::math::normalize(zero);

    EXPECT_EQ(n, zero);
}
