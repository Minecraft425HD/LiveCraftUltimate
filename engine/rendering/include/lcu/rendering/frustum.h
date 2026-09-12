#pragma once

#include <array>

#include "lcu/core/types.h"
#include "lcu/math/mat4.h"
#include "lcu/math/vec3.h"
#include "lcu/physics/aabb.h"

namespace lcu::rendering {

// One frustum clip plane in the form `dot(normal, point) + d >= 0` for a
// point on the "inside" (visible) side - `normal` points INTO the
// frustum, matching the sign convention `Frustum::contains_aabb` tests
// against.
struct Plane {
    math::Vec3 normal{0.0f, 0.0f, 0.0f};
    f32 d = 0.0f;
};

// Real view-frustum culling (Phase 68, brief section 68: "Alles was man
// nicht sieht, wird nicht gerendert"): a real 6-plane camera frustum
// extracted directly from a real view*projection matrix via the
// standard Gribb/Hartmann method - no separate FOV/near/far bookkeeping
// duplicated here, this always reflects exactly whatever real
// projection the caller is actually using this frame, so it can never
// silently drift out of sync with what's really drawn.
class Frustum {
   public:
    // `vp` must be `proj * view` (projection applied AFTER view - see
    // frustum.cpp's own doc comment on why the multiplication order
    // matters for the row-extraction math below to be correct for this
    // project's column-major `Mat4` convention, matching bgfx/GPU
    // layout). Real, deterministic - the same `vp` any real draw call
    // this frame would use, so a chunk this rejects is genuinely never
    // drawn.
    static Frustum from_view_projection(const math::Mat4& vp);

    // True if `box` is at least partially inside the frustum (a
    // conservative test: a box merely straddling a plane still counts
    // as visible - real occlusion culling elsewhere, Phase 69, handles
    // finer rejection) - false only when at least one of the 6 planes
    // has ALL 8 corners strictly outside it, the same "definitely
    // outside" test every real frustum culler uses.
    bool contains_aabb(const physics::AABB& box) const;

   private:
    std::array<Plane, 6> planes_;
};

}  // namespace lcu::rendering
