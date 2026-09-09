#pragma once

#include "lcu/math/vec3.h"

namespace game::components {

// World-space position (block units, same convention as
// lcu::physics::AABB/raycast). Deliberately just a Vec3, not a full
// transform (rotation/scale) - nothing so far needs an entity to have an
// orientation or size of its own (brief section 98).
struct Position {
    lcu::math::Vec3 value;
};

}  // namespace game::components
