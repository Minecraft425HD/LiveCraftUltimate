#include "game/systems/ai_wander_system.h"

#include <algorithm>

#include "game/components/ai_wander.h"
#include "game/components/position.h"
#include "lcu/math/vec3.h"

namespace game::systems {

void update_ai_wander(lcu::ecs::Registry& registry, const AIWanderConfig& config, std::mt19937& rng, lcu::f32 dt) {
    using components::AIWander;
    using components::Position;
    using lcu::math::Vec3;

    auto& pool = registry.pool_for<AIWander>();
    const auto& entities = pool.dense_entities();

    for (lcu::usize i = 0; i < pool.dense().size(); ++i) {
        AIWander& ai = pool.dense()[i];
        Position* pos = registry.get_component<Position>(entities[i]);
        if (pos == nullptr) {
            continue;  // no Position to move - nothing for this system to do.
        }

        if (ai.wait_seconds > 0.0f) {
            ai.wait_seconds -= dt;
            continue;
        }

        const Vec3 to_target = ai.target - pos->value;
        const lcu::f32 distance = lcu::math::length(to_target);

        if (distance <= config.arrival_distance) {
            std::uniform_real_distribution<lcu::f32> offset_dist(-config.wander_radius, config.wander_radius);
            ai.target = pos->value + Vec3{offset_dist(rng), 0.0f, offset_dist(rng)};
            std::uniform_real_distribution<lcu::f32> idle_dist(config.idle_seconds_min, config.idle_seconds_max);
            ai.wait_seconds = idle_dist(rng);
            continue;
        }

        const lcu::f32 step_distance = std::min(ai.speed * dt, distance);
        pos->value += lcu::math::normalize(to_target) * step_distance;
    }
}

}  // namespace game::systems
