#pragma once

#include "lcu/items/item_stack.h"
#include "lcu/math/vec3.h"
#include "lcu/physics/aabb.h"

namespace game::components {

// A real, physically-simulated dropped item (Phase 50, closing brief
// section 55's "item pickup goes straight to inventory, no physical
// dropped-item world entity" gap - deferred since Phase 5, since it
// needs entities (Phase 6) to exist first). Paired with a
// `game::components::Position` (its world-space center) on the same
// entity - this struct only carries what a plain Position doesn't:
// the real item/count it represents, its own vertical physics state,
// and real visual/lifecycle timers.
struct ItemEntity {
    lcu::items::ItemStack stack;
    // Only vertical velocity is tracked - a dropped item gets a real
    // small upward toss on spawn and falls under real gravity/ground
    // collision (game/systems/item_entity_system.h), but never moves
    // horizontally once it exists. Minecraft's own real dropped items
    // also get a small random horizontal scatter; deliberately left out
    // here as a real, minor scope simplification (see DECISIONS.md) -
    // nothing about the vertical physics/pickup/despawn pipeline
    // depends on it.
    lcu::f32 vertical_velocity = 0.0f;
    // Counts up every frame; the entity despawns once this reaches
    // kItemEntityDespawnSeconds (game/systems/item_entity_system.h) -
    // Minecraft's own real "items disappear after 5 minutes" rule.
    lcu::f32 age_seconds = 0.0f;
    // Counts down from kItemEntityPickupDelaySeconds; while > 0 the
    // entity is real but not yet pickupable, even by an overlapping
    // player AABB - Minecraft's own real 0.5s "can't instantly re-grab
    // the block you just broke while still standing in it" delay.
    lcu::f32 pickup_delay_seconds = 0.0f;
    // Y-axis rotation, radians, wrapped to [0, 2*pi) - purely visual
    // (real per-frame spin so a dropped item is visibly distinct from a
    // static block even as a plain billboard quad), never read for
    // physics/collision.
    lcu::f32 spin_angle = 0.0f;
};

// Real half-extent of every item entity's own tiny AABB (Minecraft's
// own real dropped-item hitbox is a 0.25x0.25x0.25 cube) - shared by
// the physics/ground-collision update and the player-overlap pickup
// check, so they can never disagree about an item entity's real size.
constexpr lcu::f32 kItemEntityHalfExtent = 0.125f;

inline lcu::physics::AABB item_entity_aabb(const lcu::math::Vec3& center) {
    return lcu::physics::AABB{
        center - lcu::math::Vec3{kItemEntityHalfExtent, kItemEntityHalfExtent, kItemEntityHalfExtent},
        center + lcu::math::Vec3{kItemEntityHalfExtent, kItemEntityHalfExtent, kItemEntityHalfExtent},
    };
}

}  // namespace game::components
