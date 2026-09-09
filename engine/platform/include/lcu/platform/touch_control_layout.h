#pragma once

#include <array>

#include "lcu/core/types.h"
#include "lcu/platform/input.h"

namespace lcu::platform {

// A single named button's hit region, normalized screen space (0..1),
// (0,0) top-left - same convention as TouchPoint (touch_input.h).
struct TouchButtonRect {
    Action action;
    const char* label;
    f32 x0, y0, x1, y1;  // inclusive
};

// The one authoritative definition of the mobile touch overlay's button
// cluster - TouchInputBackend hit-tests against this, and anything that
// draws the on-screen controls (engine/ui) draws exactly these rects, so
// the two can never drift apart into "button is visible here but the tap
// registers over there." Bottom-right thumb cluster (Jump/Interact/
// PlaceBlock/Sprint/Crouch) plus a top-right menu button (Inventory) - the
// standard mobile-FPS overlay layout, kept out of both drag regions' way
// (movement is the left half of the screen, look drag only starts where
// no button rect claims the touch first - see TouchInputBackend::update).
inline constexpr std::array<TouchButtonRect, 6> kTouchButtonLayout = {{
    {Action::Jump, "JUMP", 0.86f, 0.78f, 1.00f, 0.92f},
    {Action::Interact, "HIT", 0.72f, 0.78f, 0.86f, 0.92f},
    {Action::PlaceBlock, "PLACE", 0.72f, 0.62f, 0.86f, 0.76f},
    {Action::Sprint, "RUN", 0.86f, 0.62f, 1.00f, 0.76f},
    {Action::Crouch, "CROUCH", 0.58f, 0.78f, 0.72f, 0.92f},
    {Action::Inventory, "INV", 0.90f, 0.02f, 1.00f, 0.12f},
}};

inline bool touch_button_contains(const TouchButtonRect& button, f32 x, f32 y) {
    return x >= button.x0 && x <= button.x1 && y >= button.y0 && y <= button.y1;
}

}  // namespace lcu::platform
