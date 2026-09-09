#pragma once

#include "lcu/core/types.h"

namespace lcu::math {

struct Vec4 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}

    constexpr Vec4 operator+(const Vec4& rhs) const {
        return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w};
    }
    constexpr Vec4 operator*(f32 scalar) const { return {x * scalar, y * scalar, z * scalar, w * scalar}; }

    constexpr bool operator==(const Vec4& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
    }
};

}  // namespace lcu::math
