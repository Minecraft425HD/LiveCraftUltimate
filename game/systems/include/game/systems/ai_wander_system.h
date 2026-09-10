#pragma once

#include <random>

#include "lcu/core/types.h"
#include "lcu/ecs/registry.h"

namespace game::systems {

struct AIWanderConfig {
    lcu::f32 wander_radius = 5.0f;      // blocks, new target picked within this of the current position
    lcu::f32 idle_seconds_min = 1.0f;
    lcu::f32 idle_seconds_max = 4.0f;
    lcu::f32 arrival_distance = 0.1f;   // blocks; "close enough" to the target to stop walking
};

// Updates every entity that has both a Position and an AIWander
// component: while AIWander::wait_seconds > 0, counts it down (idling,
// no movement). Otherwise steps Position toward AIWander::target at
// AIWander::speed blocks/s; on arrival (within config.arrival_distance),
// picks a new target uniformly within config.wander_radius of the
// current position via `rng` and starts idling for a duration uniformly
// sampled from [idle_seconds_min, idle_seconds_max]. An explicit `rng`
// (rather than a hidden global one) keeps this deterministic and
// testable, matching lcu::world::worldgen's "no hidden global state"
// approach.
void update_ai_wander(lcu::ecs::Registry& registry, const AIWanderConfig& config, std::mt19937& rng, lcu::f32 dt);

}  // namespace game::systems
