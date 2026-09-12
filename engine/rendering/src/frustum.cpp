#include "lcu/rendering/frustum.h"

namespace lcu::rendering {

namespace {

// One mathematical row of `m`, indexed 0-3 top to bottom. `Mat4` stores
// its 16 floats column-major (`m.data()[col*4+row]`, matching bgfx/GPU
// convention - see Mat4's own doc comment), so row `r`'s 4 components
// live at indices `r`, `4+r`, `8+r`, `12+r`, one per column.
struct Row4 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    Row4 operator+(const Row4& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
    Row4 operator-(const Row4& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
};

Row4 matrix_row(const math::Mat4& m, int row) {
    return Row4{m.data()[row], m.data()[4 + row], m.data()[8 + row], m.data()[12 + row]};
}

// Builds a real, normalized Plane from one un-normalized Gribb/Hartmann
// row combination (see from_view_projection below) - dividing both the
// normal AND the distance term by the normal's own length keeps
// `dot(normal, point) + d` a real signed distance, not just a sign.
Plane make_plane(const Row4& row) {
    const math::Vec3 normal{row.x, row.y, row.z};
    const f32 len = math::length(normal);
    if (len < 1e-8f) {
        // Degenerate (a zero/near-singular vp - no real camera has been
        // set up yet) - a real, honest "accepts everything" plane
        // rather than dividing by ~0, matching contains_aabb's own "no
        // plane rejects it => visible" default for an as-yet-undefined
        // frustum.
        return Plane{math::Vec3{0.0f, 0.0f, 0.0f}, 1.0f};
    }
    return Plane{normal * (1.0f / len), row.w / len};
}

}  // namespace

// Real Gribb/Hartmann fast frustum extraction: for column-vector clip =
// vp * v (this project's own convention, v a column vector, vp = proj *
// view - see Mat4::operator* and Renderer's own view/proj usage), the 6
// OpenGL-style ([-1,1] NDC) frustum planes are exactly row3 +/- row0/1/2
// of vp, each namely satisfying `A*x+B*y+C*z+D >= 0` for a point inside
// that one plane (this is the brief's own literal "Zeile 3 ± Zeile
// 0/1/2, normalisieren").
Frustum Frustum::from_view_projection(const math::Mat4& vp) {
    const Row4 row0 = matrix_row(vp, 0);
    const Row4 row1 = matrix_row(vp, 1);
    const Row4 row2 = matrix_row(vp, 2);
    const Row4 row3 = matrix_row(vp, 3);

    Frustum frustum;
    frustum.planes_[0] = make_plane(row3 + row0);  // left
    frustum.planes_[1] = make_plane(row3 - row0);  // right
    frustum.planes_[2] = make_plane(row3 + row1);  // bottom
    frustum.planes_[3] = make_plane(row3 - row1);  // top
    frustum.planes_[4] = make_plane(row3 + row2);  // near
    frustum.planes_[5] = make_plane(row3 - row2);  // far
    return frustum;
}

bool Frustum::contains_aabb(const physics::AABB& box) const {
    const math::Vec3 corners[8] = {
        {box.min.x, box.min.y, box.min.z}, {box.max.x, box.min.y, box.min.z},
        {box.min.x, box.max.y, box.min.z}, {box.max.x, box.max.y, box.min.z},
        {box.min.x, box.min.y, box.max.z}, {box.max.x, box.min.y, box.max.z},
        {box.min.x, box.max.y, box.max.z}, {box.max.x, box.max.y, box.max.z},
    };
    for (const Plane& plane : planes_) {
        bool all_outside = true;
        for (const math::Vec3& corner : corners) {
            if (math::dot(plane.normal, corner) + plane.d >= 0.0f) {
                all_outside = false;
                break;
            }
        }
        if (all_outside) {
            // One plane, all 8 corners outside it - definitely outside
            // the frustum (the brief's own literal "Eine Plane, alle 8
            // außen -> false").
            return false;
        }
    }
    return true;
}

}  // namespace lcu::rendering
