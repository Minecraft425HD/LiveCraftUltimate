#include "lcu/player/mouse_look.h"

namespace lcu::player {

MouseLookDelta mouse_look_delta(f32 mouse_delta_x, f32 mouse_delta_y, f32 sensitivity) {
    return MouseLookDelta{
        /*yaw=*/-mouse_delta_x * sensitivity,
        /*pitch=*/-mouse_delta_y * sensitivity,
    };
}

}  // namespace lcu::player
