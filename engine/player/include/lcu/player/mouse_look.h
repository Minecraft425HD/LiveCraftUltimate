#pragma once

#include "lcu/core/types.h"

namespace lcu::player {

// The real yaw/pitch delta (radians) a frame's raw mouse motion should
// apply to a FirstPersonCamera - extracted out of client/main.cpp's own
// inline mouse-look block (Phase 74 bugfix) so the sign convention is a
// real, independently unit-tested function instead of two un-tested
// minus signs buried in a ~6000-line main().
struct MouseLookDelta {
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;
};

// `mouse_delta_x`/`mouse_delta_y` are InputState::mouse_delta_x/y's own
// raw values (positive x = mouse moved right, positive y = mouse moved
// down - see InputState's own doc comment); `sensitivity` is
// Options::mouse_sensitivity, always >= 0.
//
// Yaw is negated relative to a naive `dx * sensitivity`: FirstPerson
// Camera::add_yaw_pitch's own convention is that *increasing* yaw turns
// the camera left (confirmed by FirstPersonCamera's own YawNinetyDegrees
// FacesNegativeX test - yaw=+90 deg faces -X, and DefaultRightIsPositiveX
// puts "right" at +X, so increasing yaw rotates away from "right").
// Moving the mouse right must turn the camera right (decrease yaw), so
// mouse_delta_x's sign is flipped here - this was Phase 74's own Bug 1
// (moving the mouse right visibly turned the camera left before this
// fix, since client/main.cpp previously passed +mouse_delta_x straight
// through with no flip).
//
// Pitch is also negated relative to a naive `dy * sensitivity`: positive
// mouse_delta_y is the mouse moving DOWN, which must look DOWN, i.e.
// decrease pitch (FirstPersonCamera::forward()'s own `sin(pitch)` is the
// look direction's Y component - a more negative pitch looks down).
MouseLookDelta mouse_look_delta(f32 mouse_delta_x, f32 mouse_delta_y, f32 sensitivity);

}  // namespace lcu::player
