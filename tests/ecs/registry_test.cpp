#include "lcu/ecs/registry.h"

#include <gtest/gtest.h>

using lcu::ecs::EntityId;
using lcu::ecs::kInvalidEntityId;
using lcu::ecs::Registry;

namespace {

struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

struct Health {
    int value = 0;
};

}  // namespace

TEST(Registry, CreateEntityReturnsDistinctLiveIds) {
    Registry registry;
    const EntityId a = registry.create_entity();
    const EntityId b = registry.create_entity();

    EXPECT_NE(a, b);
    EXPECT_TRUE(registry.is_alive(a));
    EXPECT_TRUE(registry.is_alive(b));
    EXPECT_EQ(registry.entity_count(), 2u);
}

TEST(Registry, CreateEntityNeverReturnsTheInvalidId) {
    Registry registry;
    for (int i = 0; i < 5; ++i) {
        EXPECT_NE(registry.create_entity(), kInvalidEntityId);
    }
    EXPECT_FALSE(registry.is_alive(kInvalidEntityId));
}

TEST(Registry, DestroyEntityMakesItNotAlive) {
    Registry registry;
    const EntityId entity = registry.create_entity();
    registry.destroy_entity(entity);

    EXPECT_FALSE(registry.is_alive(entity));
    EXPECT_EQ(registry.entity_count(), 0u);
}

TEST(Registry, RecycledSlotGetsANewGenerationInvalidatingTheOldHandle) {
    Registry registry;
    const EntityId first = registry.create_entity();
    registry.destroy_entity(first);
    const EntityId second = registry.create_entity();

    // Same slot index reused, but a different generation - the stale
    // handle from before destruction must not alias the new entity.
    EXPECT_EQ(first.index, second.index);
    EXPECT_NE(first.generation, second.generation);
    EXPECT_FALSE(registry.is_alive(first));
    EXPECT_TRUE(registry.is_alive(second));
}

TEST(Registry, AddAndGetComponentRoundTrips) {
    Registry registry;
    const EntityId entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{1.0f, 2.0f});

    Position* pos = registry.get_component<Position>(entity);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
    EXPECT_FLOAT_EQ(pos->y, 2.0f);
}

TEST(Registry, GetComponentReturnsNullForEntityWithoutIt) {
    Registry registry;
    const EntityId entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{});

    EXPECT_EQ(registry.get_component<Health>(entity), nullptr);
}

TEST(Registry, GetComponentReturnsNullForUnknownComponentTypeEntirely) {
    // Health is never added to any entity in this registry at all - the
    // pool for it doesn't even exist yet.
    Registry registry;
    const EntityId entity = registry.create_entity();
    EXPECT_EQ(registry.get_component<Health>(entity), nullptr);
}

TEST(Registry, HasComponentReflectsPresence) {
    Registry registry;
    const EntityId entity = registry.create_entity();
    EXPECT_FALSE(registry.has_component<Position>(entity));

    registry.add_component<Position>(entity, Position{});
    EXPECT_TRUE(registry.has_component<Position>(entity));
}

TEST(Registry, RemoveComponentDropsIt) {
    Registry registry;
    const EntityId entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{5.0f, 6.0f});
    registry.remove_component<Position>(entity);

    EXPECT_FALSE(registry.has_component<Position>(entity));
    EXPECT_EQ(registry.get_component<Position>(entity), nullptr);
}

TEST(Registry, DestroyEntityRemovesAllItsComponentsAcrossEveryType) {
    Registry registry;
    const EntityId entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{});
    registry.add_component<Health>(entity, Health{10});

    registry.destroy_entity(entity);

    // Re-querying with the (now stale) handle must not find anything -
    // has_component itself doesn't check is_alive, so this specifically
    // exercises the sparse-set cleanup, not just liveness.
    EXPECT_EQ(registry.get_component<Position>(entity), nullptr);
    EXPECT_EQ(registry.get_component<Health>(entity), nullptr);
}

TEST(Registry, SwapAndPopRemovalKeepsOtherComponentsIntact) {
    Registry registry;
    const EntityId a = registry.create_entity();
    const EntityId b = registry.create_entity();
    const EntityId c = registry.create_entity();
    registry.add_component<Position>(a, Position{1.0f, 1.0f});
    registry.add_component<Position>(b, Position{2.0f, 2.0f});
    registry.add_component<Position>(c, Position{3.0f, 3.0f});

    // Removes the middle entry, forcing a swap-and-pop that moves `c`'s
    // component into `b`'s old dense slot.
    registry.remove_component<Position>(b);

    EXPECT_EQ(registry.get_component<Position>(a)->x, 1.0f);
    EXPECT_EQ(registry.get_component<Position>(b), nullptr);
    ASSERT_NE(registry.get_component<Position>(c), nullptr);
    EXPECT_EQ(registry.get_component<Position>(c)->x, 3.0f);
}

TEST(Registry, PoolForAllowsDenseIterationOverEveryLiveComponent) {
    Registry registry;
    const EntityId a = registry.create_entity();
    const EntityId b = registry.create_entity();
    const EntityId c = registry.create_entity();
    registry.add_component<Position>(a, Position{1.0f, 0.0f});
    registry.add_component<Position>(b, Position{2.0f, 0.0f});
    // c deliberately has no Position.

    auto& pool = registry.pool_for<Position>();
    EXPECT_EQ(pool.dense().size(), 2u);

    float sum = 0.0f;
    for (const Position& pos : pool.dense()) {
        sum += pos.x;
    }
    EXPECT_FLOAT_EQ(sum, 3.0f);
    (void)c;
}

TEST(Registry, ReAddingComponentAfterRemovalWorks) {
    Registry registry;
    const EntityId entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{1.0f, 1.0f});
    registry.remove_component<Position>(entity);
    registry.add_component<Position>(entity, Position{9.0f, 9.0f});

    Position* pos = registry.get_component<Position>(entity);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 9.0f);
}
