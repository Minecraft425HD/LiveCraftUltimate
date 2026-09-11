#pragma once

#include "lcu/core/types.h"

namespace game::components {

// Real player hunger (Phase 51.2): 0-20, Minecraft's own real range (10
// drumstick icons, each worth 2 points - the same half-icon convention
// Phase 47's HUD bars already established for health). Same "plain
// struct, not an ECS component" reasoning PlayerHealth's own doc
// comment gives.
struct PlayerHunger {
    lcu::f32 current = 20.0f;
    lcu::f32 max = 20.0f;
};

}  // namespace game::components
