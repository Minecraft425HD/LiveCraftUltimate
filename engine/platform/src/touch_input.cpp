#include "lcu/platform/touch_input.h"

#include <algorithm>
#include <cmath>

#include "lcu/platform/touch_control_layout.h"

namespace lcu::platform {

namespace {

const TouchPoint* find_touch(const std::vector<TouchPoint>& touches, u64 id) {
    auto it = std::find_if(touches.begin(), touches.end(), [id](const TouchPoint& t) { return t.id == id; });
    return it == touches.end() ? nullptr : &*it;
}

// Applies one drag's dead-zone-thresholded direction to `state`, along the
// given (negative-direction, positive-direction) Action pair for each axis.
void apply_drag(f32 origin_x, f32 origin_y, f32 current_x, f32 current_y, InputState& state, Action neg_x,
                 Action pos_x, Action neg_y, Action pos_y) {
    const f32 dx = current_x - origin_x;
    const f32 dy = current_y - origin_y;
    if (dx <= -TouchInputBackend::kDragDeadZone) {
        state.set_down(neg_x, true);
    }
    if (dx >= TouchInputBackend::kDragDeadZone) {
        state.set_down(pos_x, true);
    }
    // Screen y increases downward: dragging up (dy negative) means "look
    // up"/"move forward", matching the arrow-key LookUp/MoveForward sense
    // used elsewhere (see input.cpp's default keyboard bindings).
    if (dy <= -TouchInputBackend::kDragDeadZone) {
        state.set_down(neg_y, true);
    }
    if (dy >= TouchInputBackend::kDragDeadZone) {
        state.set_down(pos_y, true);
    }
}

}  // namespace

void TouchInputBackend::update(const std::vector<TouchPoint>& active_touches, InputState& state) {
    state = InputState{};

    std::vector<bool> consumed(active_touches.size(), false);

    // Buttons claim first: any touch currently inside a button's rect sets
    // that action, regardless of drag state, and is removed from
    // consideration for starting/continuing a drag.
    for (usize i = 0; i < active_touches.size(); ++i) {
        for (const TouchButtonRect& button : kTouchButtonLayout) {
            if (touch_button_contains(button, active_touches[i].x, active_touches[i].y)) {
                state.set_down(button.action, true);
                consumed[i] = true;
                break;
            }
        }
    }

    // Drop a drag whose finger lifted (no longer in active_touches) or got
    // claimed by a button this frame.
    auto drag_still_active = [&](const std::optional<ActiveDrag>& drag) {
        if (!drag.has_value()) {
            return false;
        }
        for (usize i = 0; i < active_touches.size(); ++i) {
            if (!consumed[i] && active_touches[i].id == drag->touch_id) {
                return true;
            }
        }
        return false;
    };
    if (!drag_still_active(movement_drag_)) {
        movement_drag_.reset();
    }
    if (!drag_still_active(look_drag_)) {
        look_drag_.reset();
    }

    // Apply the two active drags (if any) against this frame's touch
    // position for that finger.
    if (movement_drag_.has_value()) {
        if (const TouchPoint* touch = find_touch(active_touches, movement_drag_->touch_id)) {
            apply_drag(movement_drag_->origin_x, movement_drag_->origin_y, touch->x, touch->y, state,
                       Action::MoveLeft, Action::MoveRight, Action::MoveForward, Action::MoveBackward);
        }
    }
    if (look_drag_.has_value()) {
        if (const TouchPoint* touch = find_touch(active_touches, look_drag_->touch_id)) {
            apply_drag(look_drag_->origin_x, look_drag_->origin_y, touch->x, touch->y, state, Action::LookLeft,
                       Action::LookRight, Action::LookUp, Action::LookDown);
        }
    }

    // A not-yet-claimed touch in the left half starts a movement drag, one
    // in the right half starts a look drag - first such touch each frame,
    // one drag of each kind at a time (a second finger landing in the same
    // half is ignored for dragging, though it can still hit a button).
    for (usize i = 0; i < active_touches.size(); ++i) {
        if (consumed[i]) {
            continue;
        }
        const TouchPoint& touch = active_touches[i];
        if (touch.x < 0.5f) {
            if (!movement_drag_.has_value()) {
                movement_drag_ = ActiveDrag{touch.id, touch.x, touch.y};
            }
        } else {
            if (!look_drag_.has_value()) {
                look_drag_ = ActiveDrag{touch.id, touch.x, touch.y};
            }
        }
    }
}

}  // namespace lcu::platform
