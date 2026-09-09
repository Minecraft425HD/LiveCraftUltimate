#pragma once

#include "lcu/core/types.h"
#include "lcu/math/vec3.h"

namespace game::components {

// Simple wander AI (brief section 60's "einfache KI"): an entity with
// this component alternates between idling in place for `wait_seconds`
// and walking in a straight line toward `target` at `speed` - see
// game/systems/ai_wander_system.h for the system that actually drives
// it. No pathfinding, obstacle avoidance, or awareness of the player;
// just enough real behavior to be a genuine (not faked) AI consumer for
// engine/ecs - see DECISIONS.md.
struct AIWander {
    lcu::math::Vec3 target;
    lcu::f32 speed = 1.5f;          // blocks/s while walking
    lcu::f32 wait_seconds = 0.0f;   // counts down while > 0 (idling); <= 0 means "walk toward target"
};

}  // namespace game::components
