#pragma once

#include "lcu/core/types.h"

namespace game::components {

// Real player health (Phase 51.1, brief section 55/60's survival
// mechanics): 20 = 10 hearts, Minecraft's own real default. A plain
// struct, not an ECS component attached to an entity - the player isn't
// itself an ECS entity in this codebase (see `Position`/`AIWander`'s own
// real consumers, engine/ecs entities), so this is held directly by
// whichever real player-state owner needs it (client/main.cpp), the
// same way `lcu::physics::PlayerPhysicsState` already is.
struct PlayerHealth {
    lcu::f32 current = 20.0f;
    lcu::f32 max = 20.0f;
};

}  // namespace game::components
