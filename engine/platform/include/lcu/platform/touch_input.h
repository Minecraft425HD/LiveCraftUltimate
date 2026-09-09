#pragma once

#include <optional>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/platform/input.h"

namespace lcu::platform {

// One active finger, in normalized screen space: x/y in [0, 1], (0, 0) top
// left, y increasing downward (matches SDL's own finger-event coordinate
// convention, so a real SDL_EVENT_FINGER_* pump can feed this directly
// without a conversion step). `id` is SDL's own per-finger id - stable for
// the lifetime of one finger touching the screen, which is what lets
// TouchInputBackend tell "the same finger, moved" from "a different
// finger, happens to be nearby".
struct TouchPoint {
    u64 id = 0;
    f32 x = 0.0f;
    f32 y = 0.0f;
};

// Maps a frame's active touches onto the same device-agnostic Action set
// keyboard/gamepad drive (brief section 28) - gameplay code never knows or
// cares that the input came from a finger. Two drag regions (movement on
// the left half of the screen, look on the right half - the standard
// twin-virtual-stick mobile FPS layout) plus fixed button rects for the
// remaining discrete actions.
//
// A drag region's actions are discrete (Move*/Look* are booleans, matching
// every other Action - see input.h), not an analog vector: once a finger
// drags past kDragDeadZone from where it first touched down, the
// corresponding direction(s) go true, mirroring how WASD already works.
// This keeps `MovementInput`/the camera fully unaware of *how* an action
// became true - no separate analog code path to keep in sync with the
// digital one.
class TouchInputBackend {
   public:
    // Normalized-screen-space distance a drag must travel from its origin
    // before a direction registers as held - avoids a finger that barely
    // trembles at touch-down spuriously triggering movement/look.
    static constexpr f32 kDragDeadZone = 0.04f;

    // Recomputes InputState from this frame's active touches. Call once
    // per frame with the full current touch list (not deltas) - like
    // KeyboardInputBackend::update, this fully overwrites every Action
    // Action it's responsible for, so a released finger's action clears
    // automatically the frame it stops appearing in `active_touches`.
    void update(const std::vector<TouchPoint>& active_touches, InputState& state);

   private:
    struct ActiveDrag {
        u64 touch_id = 0;
        f32 origin_x = 0.0f;
        f32 origin_y = 0.0f;
    };

    std::optional<ActiveDrag> movement_drag_;
    std::optional<ActiveDrag> look_drag_;
};

}  // namespace lcu::platform
