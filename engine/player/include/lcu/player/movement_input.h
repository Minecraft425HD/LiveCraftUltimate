#pragma once

#include "lcu/math/vec3.h"
#include "lcu/platform/input.h"
#include "lcu/player/camera.h"

namespace lcu::player {

// Combines the MoveForward/Backward/Left/Right InputState actions into a
// normalized horizontal (y=0) world-space direction, relative to
// `camera`'s current yaw - pitch doesn't affect movement direction
// (looking up/down doesn't change which way "forward" walks). Returns
// the zero vector if no movement actions are held, or the held ones
// exactly cancel out (e.g. forward+backward together).
math::Vec3 movement_direction_from_input(const platform::InputState& input, const FirstPersonCamera& camera);

}  // namespace lcu::player
