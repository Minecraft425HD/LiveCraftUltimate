#include "lcu/player/camera.h"

#include <cmath>

namespace lcu::player {

namespace {
// ~89 degrees in radians - just under pi/2 (90deg), enough headroom to
// avoid the look_at up-vector degenerating at exactly the pole.
constexpr f32 kPitchLimit = 1.5533f;
}  // namespace

void FirstPersonCamera::add_yaw_pitch(f32 delta_yaw, f32 delta_pitch) {
    yaw += delta_yaw;
    pitch += delta_pitch;
    if (pitch > kPitchLimit) {
        pitch = kPitchLimit;
    }
    if (pitch < -kPitchLimit) {
        pitch = -kPitchLimit;
    }
}

math::Vec3 FirstPersonCamera::forward() const {
    const f32 cos_pitch = std::cos(pitch);
    return math::Vec3{
        -std::sin(yaw) * cos_pitch,
        std::sin(pitch),
        -std::cos(yaw) * cos_pitch,
    };
}

math::Vec3 FirstPersonCamera::right() const { return math::Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)}; }

math::Vec3 FirstPersonCamera::forward_horizontal() const {
    return math::Vec3{-std::sin(yaw), 0.0f, -std::cos(yaw)};
}

math::Mat4 FirstPersonCamera::view_matrix() const {
    return math::Mat4::look_at(position, position + forward(), math::Vec3{0.0f, 1.0f, 0.0f});
}

}  // namespace lcu::player
