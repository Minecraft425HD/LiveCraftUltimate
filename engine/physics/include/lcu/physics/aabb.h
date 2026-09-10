#pragma once

#include "lcu/math/vec3.h"

namespace lcu::physics {

// Axis-aligned bounding box in world-block-space (1 unit = 1 block, same
// convention as raycast.h and the mesher's vertex positions).
struct AABB {
    math::Vec3 min;
    math::Vec3 max;

    math::Vec3 center() const { return (min + max) * 0.5f; }
    math::Vec3 half_extents() const { return (max - min) * 0.5f; }

    AABB translated(const math::Vec3& offset) const { return AABB{min + offset, max + offset}; }

    bool intersects(const AABB& other) const {
        return min.x < other.max.x && max.x > other.min.x && min.y < other.max.y && max.y > other.min.y &&
               min.z < other.max.z && max.z > other.min.z;
    }
};

}  // namespace lcu::physics
