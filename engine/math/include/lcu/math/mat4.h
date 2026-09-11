#pragma once

#include <cmath>

#include "lcu/math/vec3.h"
#include "lcu/math/vec4.h"

namespace lcu::math {

// Column-major 4x4 matrix, matching bgfx/GPU convention (m[col][row]).
// Stored as 16 contiguous floats so `data()` can be passed straight to
// bgfx::setTransform without any layout conversion.
struct Mat4 {
    f32 m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };

    constexpr f32* data() { return m; }
    constexpr const f32* data() const { return m; }

    static constexpr Mat4 identity() { return Mat4{}; }

    static Mat4 translation(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r{};
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        r.m[15] = 1.0f;
        return r;
    }

    // Right-handed perspective projection, depth range [-1, 1] (OpenGL-style;
    // bgfx normalizes this per-backend internally when the flag is set, but
    // for now callers use bgfx::setViewTransform with a homogeneous-depth
    // aware helper once the rendering layer exists in Phase 1/2).
    static Mat4 perspective(f32 fov_y_radians, f32 aspect, f32 near_z, f32 far_z) {
        Mat4 r{};
        const f32 tan_half_fov = std::tan(fov_y_radians * 0.5f);
        r.m[0] = 1.0f / (aspect * tan_half_fov);
        r.m[5] = 1.0f / tan_half_fov;
        r.m[10] = -(far_z + near_z) / (far_z - near_z);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * far_z * near_z) / (far_z - near_z);
        r.m[15] = 0.0f;
        return r;
    }

    // Right-handed orthographic projection, depth range [-1, 1] - same
    // OpenGL-style convention perspective() above already uses (bgfx
    // normalizes per-backend internally). Real use (Phase 44): 2D UI
    // rendering, where `left`/`right`/`bottom`/`top` are screen pixel
    // bounds (bottom > top for SDL/mouse's own y-increases-downward
    // convention, not the "bottom < top, y-up" a 3D scene would use) and
    // `near_z`/`far_z` just need to be a real, non-degenerate range (UI
    // quads have no meaningful depth of their own - see Renderer::
    // submit_ui_quad, drawn with depth testing off).
    static Mat4 orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near_z, f32 far_z) {
        Mat4 r{};
        r.m[0] = 2.0f / (right - left);
        r.m[5] = 2.0f / (top - bottom);
        r.m[10] = -2.0f / (far_z - near_z);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(far_z + near_z) / (far_z - near_z);
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
        const Vec3 f = normalize(target - eye);
        const Vec3 s = normalize(cross(f, up));
        const Vec3 u = cross(s, f);

        Mat4 r{};
        r.m[0] = s.x;
        r.m[4] = s.y;
        r.m[8] = s.z;
        r.m[1] = u.x;
        r.m[5] = u.y;
        r.m[9] = u.z;
        r.m[2] = -f.x;
        r.m[6] = -f.y;
        r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] = dot(f, eye);
        r.m[15] = 1.0f;
        return r;
    }

    friend Mat4 operator*(const Mat4& a, const Mat4& b) {
        Mat4 result{};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                f32 sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                }
                result.m[col * 4 + row] = sum;
            }
        }
        return result;
    }
};

}  // namespace lcu::math
