#include "game/systems/player_vitals_system.h"

#include <algorithm>

namespace game::systems {

bool apply_damage(components::PlayerHealth& health, lcu::f32 amount) {
    const bool was_alive = health.current > 0.0f;
    health.current = std::clamp(health.current - amount, 0.0f, health.max);
    return was_alive && health.current <= 0.0f;
}

lcu::f32 fall_damage_for_distance(lcu::f32 fall_distance_blocks) {
    return fall_distance_blocks > kFallDamageSafeBlocks ? fall_distance_blocks - kFallDamageSafeBlocks : 0.0f;
}

lcu::f32 update_fall_tracking(FallTracker& tracker, components::PlayerHealth& health, lcu::f32 delta_y,
                               bool grounded) {
    lcu::f32 damage = 0.0f;
    if (!grounded) {
        if (delta_y > 0.0f) {
            tracker.fall_distance += delta_y;
        }
    } else {
        if (!tracker.was_grounded) {
            damage = fall_damage_for_distance(tracker.fall_distance);
            if (damage > 0.0f) {
                apply_damage(health, damage);
            }
        }
        tracker.fall_distance = 0.0f;
    }
    tracker.was_grounded = grounded;
    return damage;
}

void update_health_regen(components::PlayerHealth& health, const components::PlayerHunger& hunger,
                          lcu::f32& regen_accumulator, lcu::f32 dt) {
    if (hunger.current < kHealthRegenMinHunger || health.current >= health.max) {
        regen_accumulator = 0.0f;
        return;
    }
    regen_accumulator += dt;
    while (regen_accumulator >= kHealthRegenIntervalSeconds && health.current < health.max) {
        regen_accumulator -= kHealthRegenIntervalSeconds;
        apply_damage(health, -1.0f);
    }
}

bool update_starvation(components::PlayerHealth& health, const components::PlayerHunger& hunger,
                        lcu::f32& starvation_accumulator, lcu::f32 dt) {
    if (hunger.current > 0.0f) {
        starvation_accumulator = 0.0f;
        return false;
    }
    starvation_accumulator += dt;
    bool died = false;
    while (starvation_accumulator >= kStarvationDamageIntervalSeconds && health.current > 0.0f) {
        starvation_accumulator -= kStarvationDamageIntervalSeconds;
        died = apply_damage(health, kStarvationDamagePerTick) || died;
    }
    return died;
}

void update_hunger_drain(components::PlayerHunger& hunger, bool sprinting, lcu::f32& drain_accumulator, lcu::f32 dt) {
    if (hunger.current <= 0.0f) {
        drain_accumulator = 0.0f;
        return;
    }
    drain_accumulator += dt * (sprinting ? kHungerSprintDrainMultiplier : 1.0f);
    while (drain_accumulator >= kHungerDrainIntervalSeconds && hunger.current > 0.0f) {
        drain_accumulator -= kHungerDrainIntervalSeconds;
        hunger.current = std::max(0.0f, hunger.current - 1.0f);
    }
}

void apply_jump_hunger_cost(components::PlayerHunger& hunger) {
    hunger.current = std::max(0.0f, hunger.current - kHungerPerJump);
}

void eat(components::PlayerHunger& hunger, lcu::f32 restore_amount) {
    hunger.current = std::min(hunger.max, hunger.current + restore_amount);
}

}  // namespace game::systems
