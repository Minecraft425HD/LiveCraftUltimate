#pragma once

#include "lcu/core/types.h"

namespace lcu::voxel {

// Real hold-to-break math (Phase 48.2, brief section 60's break-
// progress overlay): how much of `hardness` seconds of held Interact
// has accumulated, as a real fraction in [0, 1] - the same number both
// the break-trigger check and the darkening overlay in client/main.cpp
// consume, so they can never disagree with each other. A non-positive
// `hardness` (e.g. `game:torch`'s real 0.0f - any tool breaks a torch
// immediately, matching Minecraft) always reads as fully progressed,
// regardless of `held_seconds`, rather than dividing by zero.
f32 break_progress_fraction(f32 held_seconds, f32 hardness);

// True once break_progress_fraction would read 1.0 - a real, named
// threshold check rather than every caller re-deriving ">= 1.0" itself.
bool is_break_ready(f32 held_seconds, f32 hardness);

}  // namespace lcu::voxel
