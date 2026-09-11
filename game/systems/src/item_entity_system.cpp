#include "game/systems/item_entity_system.h"

#include <cmath>
#include <vector>

#include "game/components/item_entity.h"
#include "game/components/position.h"
#include "lcu/physics/collision.h"

namespace game::systems {

namespace {
constexpr lcu::f32 kTwoPi = 6.28318530717958647692f;
// Real player-identical gravity/fall-speed clamp (see
// PlayerPhysicsConfig's own defaults) - a dropped item falls exactly as
// fast as the player would, not a separately-tuned value.
constexpr lcu::physics::PlayerPhysicsConfig kItemPhysicsConfig{};
}  // namespace

void update_item_entities(lcu::ecs::Registry& registry, const lcu::world::World& world, lcu::f32 dt,
                           const std::function<bool(lcu::voxel::BlockId)>& is_solid) {
    using components::ItemEntity;
    using components::Position;

    auto& pool = registry.pool_for<ItemEntity>();
    const auto& entities = pool.dense_entities();

    std::vector<lcu::ecs::EntityId> to_despawn;
    for (lcu::usize i = 0; i < pool.dense().size(); ++i) {
        ItemEntity& item = pool.dense()[i];
        Position* pos = registry.get_component<Position>(entities[i]);
        if (pos == nullptr) {
            continue;  // no Position - nothing for this system to move.
        }

        item.age_seconds += dt;
        if (item.pickup_delay_seconds > 0.0f) {
            item.pickup_delay_seconds = std::max(0.0f, item.pickup_delay_seconds - dt);
        }
        item.spin_angle = std::fmod(item.spin_angle + kItemEntitySpinSpeedRadians * dt, kTwoPi);

        lcu::physics::PlayerPhysicsState state;
        state.aabb = components::item_entity_aabb(pos->value);
        state.vertical_velocity = item.vertical_velocity;
        lcu::physics::apply_gravity(state, kItemPhysicsConfig, dt);
        const lcu::physics::CollisionResult result =
            lcu::physics::move_and_collide(world, state.aabb, {0.0f, state.vertical_velocity * dt, 0.0f}, is_solid);
        state.aabb = state.aabb.translated(result.resolved_delta);
        if (result.hit_y) {
            state.vertical_velocity = 0.0f;
        }
        pos->value = state.aabb.center();
        item.vertical_velocity = state.vertical_velocity;

        if (item.age_seconds >= kItemEntityDespawnSeconds) {
            to_despawn.push_back(entities[i]);
        }
    }

    for (const lcu::ecs::EntityId& id : to_despawn) {
        registry.destroy_entity(id);
    }
}

lcu::u32 pickup_item_entities(lcu::ecs::Registry& registry, const lcu::physics::AABB& player_aabb,
                               const lcu::items::ItemRegistry& item_registry, lcu::items::Inventory& inventory) {
    using components::ItemEntity;
    using components::Position;

    auto& pool = registry.pool_for<ItemEntity>();
    const auto& entities = pool.dense_entities();

    std::vector<lcu::ecs::EntityId> to_remove;
    lcu::u32 touched_count = 0;
    for (lcu::usize i = 0; i < pool.dense().size(); ++i) {
        ItemEntity& item = pool.dense()[i];
        if (item.pickup_delay_seconds > 0.0f) {
            continue;
        }
        Position* pos = registry.get_component<Position>(entities[i]);
        if (pos == nullptr) {
            continue;
        }
        if (!components::item_entity_aabb(pos->value).intersects(player_aabb)) {
            continue;
        }

        const lcu::u32 leftover = inventory.add_item(item_registry, item.stack);
        if (leftover == 0) {
            to_remove.push_back(entities[i]);
            ++touched_count;
        } else if (leftover < item.stack.count) {
            item.stack.count = leftover;
            ++touched_count;
        }
    }

    for (const lcu::ecs::EntityId& id : to_remove) {
        registry.destroy_entity(id);
    }
    return touched_count;
}

}  // namespace game::systems
