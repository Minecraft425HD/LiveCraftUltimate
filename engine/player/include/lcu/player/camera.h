#pragma once

#include "lcu/math/mat4.h"
#include "lcu/math/vec3.h"

namespace lcu::player {

// Minimal first-person camera: position + yaw/pitch (radians). Yaw 0,
// pitch 0 looks down -Z, matching the right-handed, -Z-forward
// convention Mat4::look_at (and the rest of this repo) already uses.
// Pitch is clamped to just under +-90 degrees to avoid the view flipping
// through the poles.
class FirstPersonCamera {
   public:
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;

    void add_yaw_pitch(f32 delta_yaw, f32 delta_pitch);

    math::Vec3 forward() const;
    math::Vec3 right() const;
    // forward() with y forced to 0 and renormalized - used for player
    // movement, which stays level regardless of look pitch (looking up
    // doesn't make you walk into the air).
    math::Vec3 forward_horizontal() const;

    math::Mat4 view_matrix() const;
};

}  // namespace lcu::player
