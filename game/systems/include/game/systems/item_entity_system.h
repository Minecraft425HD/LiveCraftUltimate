#pragma once

#include <functional>

#include "lcu/ecs/registry.h"
#include "lcu/items/inventory.h"
#include "lcu/items/item_registry.h"
#include "lcu/physics/aabb.h"
#include "lcu/voxel/block_id.h"
#include "lcu/world/world.h"

namespace game::systems {

// Real Minecraft dropped-item timings (Phase 50) - shared constants so
// the spawn call site, the physics update, and any future consumer all
// agree on the same real numbers.
constexpr lcu::f32 kItemEntityDespawnSeconds = 300.0f;      // 5 minutes.
constexpr lcu::f32 kItemEntityPickupDelaySeconds = 0.5f;
constexpr lcu::f32 kItemEntitySpawnUpSpeed = 3.0f;          // blocks/s, initial upward toss.
constexpr lcu::f32 kItemEntitySpinSpeedRadians = 3.0f;      // radians/s, purely visual.

// Advances every entity with both a `Position` and an `ItemEntity`
// component by one real physics step: gravity + real AABB/voxel ground
// collision (reusing `lcu::physics::apply_gravity`/`move_and_collide` -
// the exact same primitives the player's own controller already uses,
// not a reimplementation), a real per-frame visual spin, and age
// tracking. An entity whose `age_seconds` reaches
// `kItemEntityDespawnSeconds` is destroyed (real entity removal, not
// merely hidden) before this returns.
void update_item_entities(lcu::ecs::Registry& registry, const lcu::world::World& world, lcu::f32 dt,
                           const std::function<bool(lcu::voxel::BlockId)>& is_solid);

// Real player pickup (Phase 50.2): every item entity whose
// `pickup_delay_seconds` has fully counted down and whose own real AABB
// (`components::item_entity_aabb`) overlaps `player_aabb` gets added to
// `inventory` via the same `Inventory::add_item` fill order every other
// real pickup path already uses. A stack that fits entirely is removed
// (real entity destruction); a stack that only partially fits keeps the
// entity alive with its own count reduced to the real leftover, exactly
// like Minecraft's own "inventory almost full" behavior. Returns how
// many entities were touched (fully or partially picked up), for real
// log output at the call site.
lcu::u32 pickup_item_entities(lcu::ecs::Registry& registry, const lcu::physics::AABB& player_aabb,
                               const lcu::items::ItemRegistry& item_registry, lcu::items::Inventory& inventory);

}  // namespace game::systems
