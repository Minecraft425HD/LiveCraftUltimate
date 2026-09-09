#include "lcu/player/movement_input.h"

namespace lcu::player {

math::Vec3 movement_direction_from_input(const platform::InputState& input, const FirstPersonCamera& camera) {
    math::Vec3 direction{0.0f, 0.0f, 0.0f};

    if (input.is_down(platform::Action::MoveForward)) {
        direction += camera.forward_horizontal();
    }
    if (input.is_down(platform::Action::MoveBackward)) {
        direction -= camera.forward_horizontal();
    }
    if (input.is_down(platform::Action::MoveRight)) {
        direction += camera.right();
    }
    if (input.is_down(platform::Action::MoveLeft)) {
        direction -= camera.right();
    }

    return math::normalize(direction);
}

}  // namespace lcu::player
