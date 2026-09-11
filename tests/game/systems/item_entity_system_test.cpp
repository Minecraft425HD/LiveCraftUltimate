#include "game/systems/item_entity_system.h"

#include <gtest/gtest.h>

#include "game/components/item_entity.h"
#include "game/components/position.h"
#include "lcu/ecs/registry.h"
#include "lcu/items/inventory.h"
#include "lcu/items/item_registry.h"

using game::components::ItemEntity;
using game::components::Position;
using game::systems::pickup_item_entities;
using game::systems::update_item_entities;
using lcu::ecs::Registry;
using lcu::items::Inventory;
using lcu::items::ItemDefinition;
using lcu::items::ItemRegistry;
using lcu::items::ItemStack;
using lcu::math::Vec3;
using lcu::physics::AABB;
using lcu::voxel::BlockId;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::World;

namespace {

constexpr BlockId kSolid = 5;
bool is_solid_predicate(BlockId id) { return id == kSolid; }

World make_empty_world() {
    World world(1, [](Chunk&, ChunkCoord) {});
    world.load_chunk({0, 0, 0});
    return world;
}

// A full floor plane at y=0 (occupying y in [0,1)).
World make_floor_world() {
    World world(1, [](Chunk& chunk, ChunkCoord) {
        for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
            for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                chunk.set_block(x, 0, z, kSolid);
            }
        }
    });
    world.load_chunk({0, 0, 0});
    return world;
}

ItemRegistry make_registry_with_stone(lcu::items::ItemId& out_stone) {
    ItemRegistry registry;
    ItemDefinition stone;
    stone.namespaced_id = "game:stone";
    stone.max_stack_size = 64;
    out_stone = registry.register_item(stone);
    return registry;
}

}  // namespace

TEST(UpdateItemEntities, FallsUnderGravityInEmptyWorld) {
    Registry registry;
    World world = make_empty_world();
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{5.0f, 10.0f, 5.0f}});
    registry.add_component<ItemEntity>(entity, ItemEntity{{1, 1}});

    update_item_entities(registry, world, 0.5f, is_solid_predicate);

    const auto* pos = registry.get_component<Position>(entity);
    const auto* item = registry.get_component<ItemEntity>(entity);
    EXPECT_LT(pos->value.y, 10.0f);       // fell.
    EXPECT_LT(item->vertical_velocity, 0.0f);  // gained real downward velocity.
}

TEST(UpdateItemEntities, LandsAndStopsOnGround) {
    Registry registry;
    World world = make_floor_world();
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{5.0f, 3.0f, 5.0f}});
    registry.add_component<ItemEntity>(entity, ItemEntity{{1, 1}});

    // Enough real steps for it to fall from y=3 onto the floor (top at
    // y=1) and settle.
    for (int i = 0; i < 60; ++i) {
        update_item_entities(registry, world, 1.0f / 60.0f, is_solid_predicate);
    }

    const auto* pos = registry.get_component<Position>(entity);
    const auto* item = registry.get_component<ItemEntity>(entity);
    EXPECT_FLOAT_EQ(item->vertical_velocity, 0.0f);
    EXPECT_NEAR(pos->value.y, 1.0f + game::components::kItemEntityHalfExtent, 1e-3f);
}

TEST(UpdateItemEntities, SpinAngleAccumulatesAndWrapsToLessThanTwoPi) {
    Registry registry;
    World world = make_empty_world();
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 50.0f, 0.0f}});
    registry.add_component<ItemEntity>(entity, ItemEntity{{1, 1}});

    for (int i = 0; i < 100; ++i) {
        update_item_entities(registry, world, 0.1f, is_solid_predicate);
    }

    const auto* item = registry.get_component<ItemEntity>(entity);
    EXPECT_GE(item->spin_angle, 0.0f);
    EXPECT_LT(item->spin_angle, 6.28318530717958647692f);
}

TEST(UpdateItemEntities, DespawnsAfterFiveMinutes) {
    Registry registry;
    World world = make_empty_world();
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 50.0f, 0.0f}});
    ItemEntity item{{1, 1}};
    item.age_seconds = game::systems::kItemEntityDespawnSeconds - 0.1f;
    registry.add_component<ItemEntity>(entity, item);

    update_item_entities(registry, world, 0.2f, is_solid_predicate);

    EXPECT_FALSE(registry.is_alive(entity));
}

TEST(UpdateItemEntities, SkipsEntityWithNoPositionWithoutCrashing) {
    Registry registry;
    World world = make_empty_world();
    const auto entity = registry.create_entity();
    registry.add_component<ItemEntity>(entity, ItemEntity{{1, 1}});

    EXPECT_NO_FATAL_FAILURE(update_item_entities(registry, world, 0.1f, is_solid_predicate));
    EXPECT_TRUE(registry.is_alive(entity));
}

TEST(PickupItemEntities, PicksUpOverlappingItemWithNoPickupDelay) {
    lcu::items::ItemId stone_id = 0;
    const ItemRegistry item_registry = make_registry_with_stone(stone_id);
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 0.0f, 0.0f}});
    registry.add_component<ItemEntity>(entity, ItemEntity{{stone_id, 5}, 0.0f, 0.0f, 0.0f, 0.0f});

    Inventory inventory(9);
    const AABB player_aabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};

    const lcu::u32 touched = pickup_item_entities(registry, player_aabb, item_registry, inventory);

    EXPECT_EQ(touched, 1u);
    EXPECT_FALSE(registry.is_alive(entity));
    EXPECT_EQ(inventory.count_item(stone_id), 5u);
}

TEST(PickupItemEntities, SkipsWhilePickupDelayIsStillActive) {
    lcu::items::ItemId stone_id = 0;
    const ItemRegistry item_registry = make_registry_with_stone(stone_id);
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 0.0f, 0.0f}});
    registry.add_component<ItemEntity>(entity, ItemEntity{{stone_id, 5}, 0.0f, 0.0f, 0.3f, 0.0f});

    Inventory inventory(9);
    const AABB player_aabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};

    const lcu::u32 touched = pickup_item_entities(registry, player_aabb, item_registry, inventory);

    EXPECT_EQ(touched, 0u);
    EXPECT_TRUE(registry.is_alive(entity));
    EXPECT_EQ(inventory.count_item(stone_id), 0u);
}

TEST(PickupItemEntities, SkipsNonOverlappingEntity) {
    lcu::items::ItemId stone_id = 0;
    const ItemRegistry item_registry = make_registry_with_stone(stone_id);
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{100.0f, 0.0f, 0.0f}});
    registry.add_component<ItemEntity>(entity, ItemEntity{{stone_id, 5}, 0.0f, 0.0f, 0.0f, 0.0f});

    Inventory inventory(9);
    const AABB player_aabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};

    const lcu::u32 touched = pickup_item_entities(registry, player_aabb, item_registry, inventory);

    EXPECT_EQ(touched, 0u);
    EXPECT_TRUE(registry.is_alive(entity));
}

TEST(PickupItemEntities, PartialPickupWhenInventoryDoesNotHaveRoomForTheWholeStack) {
    lcu::items::ItemId stone_id = 0;
    const ItemRegistry item_registry = make_registry_with_stone(stone_id);
    Registry registry;
    const auto entity = registry.create_entity();
    registry.add_component<Position>(entity, Position{{0.0f, 0.0f, 0.0f}});
    registry.add_component<ItemEntity>(entity, ItemEntity{{stone_id, 10}, 0.0f, 0.0f, 0.0f, 0.0f});

    Inventory inventory(1);
    inventory.set_slot(0, ItemStack{stone_id, 60});  // room for only 4 more before max_stack_size=64.
    const AABB player_aabb{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};

    const lcu::u32 touched = pickup_item_entities(registry, player_aabb, item_registry, inventory);

    EXPECT_EQ(touched, 1u);
    EXPECT_TRUE(registry.is_alive(entity));  // not fully picked up - stays alive.
    EXPECT_EQ(registry.get_component<ItemEntity>(entity)->stack.count, 6u);
    EXPECT_EQ(inventory.count_item(stone_id), 64u);
}
