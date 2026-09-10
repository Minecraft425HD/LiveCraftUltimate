#include "game/systems/ai_wander_system.h"

#include <cmath>

#include <gtest/gtest.h>

#include "game/components/ai_wander.h"
#include "game/components/position.h"
#include "lcu/ecs/registry.h"

using game::components::AIWander;
using game::components::Position;
using game::systems::AIWanderConfig;
using game::systems::update_ai_wander;
using lcu::ecs::Registry;
using lcu::math::Vec3;

TEST(AIWanderSystem, IdlingEntityDoesNotMoveWhileWaitSecondsIsPositive) {
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 0.0f, 0.0f}});
    registry.add_component<AIWander>(entity, AIWander{{10.0f, 0.0f, 0.0f}, 1.0f, 2.0f});

    std::mt19937 rng(1234);
    update_ai_wander(registry, AIWanderConfig{}, rng, 0.5f);

    const Position* pos = registry.get_component<Position>(entity);
    EXPECT_FLOAT_EQ(pos->value.x, 0.0f);
    EXPECT_FLOAT_EQ(registry.get_component<AIWander>(entity)->wait_seconds, 1.5f);  // ticked down by dt
}

TEST(AIWanderSystem, WalkingEntityStepsTowardTargetAtItsSpeed) {
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 0.0f, 0.0f}});
    registry.add_component<AIWander>(entity, AIWander{{10.0f, 0.0f, 0.0f}, 2.0f, 0.0f});  // not idling

    std::mt19937 rng(1234);
    update_ai_wander(registry, AIWanderConfig{}, rng, 1.0f);

    const Position* pos = registry.get_component<Position>(entity);
    EXPECT_NEAR(pos->value.x, 2.0f, 1e-4f);  // speed(2) * dt(1) toward +x
    EXPECT_NEAR(pos->value.y, 0.0f, 1e-4f);
    EXPECT_NEAR(pos->value.z, 0.0f, 1e-4f);
}

TEST(AIWanderSystem, WalkingEntityDoesNotOvershootTheTarget) {
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 0.0f, 0.0f}});
    // Only 0.3 blocks from the target, but speed*dt would be 5.0 blocks.
    registry.add_component<AIWander>(entity, AIWander{{0.3f, 0.0f, 0.0f}, 5.0f, 0.0f});

    AIWanderConfig config;
    config.arrival_distance = 0.01f;  // small enough that it isn't "arrived" yet at 0.3 away
    std::mt19937 rng(1234);
    update_ai_wander(registry, config, rng, 1.0f);

    const Position* pos = registry.get_component<Position>(entity);
    EXPECT_NEAR(pos->value.x, 0.3f, 1e-4f);
}

TEST(AIWanderSystem, ArrivalPicksANewTargetWithinWanderRadiusAndStartsIdling) {
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{5.0f, 0.0f, 5.0f}});
    // Already at its target.
    registry.add_component<AIWander>(entity, AIWander{{5.0f, 0.0f, 5.0f}, 1.0f, 0.0f});

    AIWanderConfig config;
    config.wander_radius = 3.0f;
    config.idle_seconds_min = 2.0f;
    config.idle_seconds_max = 2.0f;  // pin the range so the result is deterministic
    std::mt19937 rng(42);
    update_ai_wander(registry, config, rng, 0.1f);

    const AIWander* ai = registry.get_component<AIWander>(entity);
    const Position* pos = registry.get_component<Position>(entity);

    EXPECT_FLOAT_EQ(ai->wait_seconds, 2.0f);
    const Vec3 delta = ai->target - pos->value;
    EXPECT_LE(std::abs(delta.x), config.wander_radius);
    EXPECT_LE(std::abs(delta.z), config.wander_radius);
    EXPECT_FLOAT_EQ(ai->target.y, 0.0f);
}

TEST(AIWanderSystem, EntityWithoutPositionIsSkippedWithoutCrashing) {
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<AIWander>(entity, AIWander{{1.0f, 0.0f, 0.0f}, 1.0f, 0.0f});

    std::mt19937 rng(1234);
    EXPECT_NO_FATAL_FAILURE(update_ai_wander(registry, AIWanderConfig{}, rng, 1.0f));
}

TEST(AIWanderSystem, MultipleEntitiesUpdateIndependently) {
    Registry registry;
    const auto a = registry.create_entity();
    const auto b = registry.create_entity();
    registry.add_component<Position>(a, Position{{0.0f, 0.0f, 0.0f}});
    registry.add_component<AIWander>(a, AIWander{{10.0f, 0.0f, 0.0f}, 1.0f, 0.0f});
    registry.add_component<Position>(b, Position{{0.0f, 0.0f, 0.0f}});
    registry.add_component<AIWander>(b, AIWander{{0.0f, 0.0f, 10.0f}, 1.0f, 0.0f});

    std::mt19937 rng(1234);
    update_ai_wander(registry, AIWanderConfig{}, rng, 1.0f);

    EXPECT_NEAR(registry.get_component<Position>(a)->value.x, 1.0f, 1e-4f);
    EXPECT_NEAR(registry.get_component<Position>(b)->value.z, 1.0f, 1e-4f);
}
