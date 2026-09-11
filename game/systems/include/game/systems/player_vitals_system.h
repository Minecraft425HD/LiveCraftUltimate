#pragma once

#include "game/components/player_health.h"
#include "game/components/player_hunger.h"
#include "lcu/core/types.h"

namespace game::systems {

// Real player health/hunger rules (Phase 51). One file for both since
// regen/starvation genuinely cross-reference each other's state
// (health regen reads hunger, starvation damage reads hunger too) -
// splitting them into two headers would just mean each includes the
// other's component anyway.

// Applies `amount` of damage (a positive value reduces health; a
// negative one heals, used identically by fall damage and natural
// regen) to `health`, clamped to [0, health.max]. Returns true iff this
// call brought `health.current` from strictly above 0 down to <= 0 - a
// real death *transition*, not "was already dead" (calling this again
// on an already-dead PlayerHealth returns false, so a caller can safely
// call it every frame without re-triggering death handling).
bool apply_damage(components::PlayerHealth& health, lcu::f32 amount);

// Real fall-damage rule (Phase 51.1, this phase's own directive):
// the first kFallDamageSafeBlocks of any fall are free; every block
// beyond that deals 1 real damage. A fall of 3.0 blocks or less deals 0.
constexpr lcu::f32 kFallDamageSafeBlocks = 3.0f;
lcu::f32 fall_damage_for_distance(lcu::f32 fall_distance_blocks);

// Real per-frame fall-distance accumulator (Phase 51.1) - mirrors how
// Minecraft's own `fallDistance` works: grows while airborne and
// actually descending (an ascending jump doesn't add to it), resets to
// 0 once grounded. Owned per-player by the caller (client/main.cpp),
// not stored in the ECS registry (see PlayerHealth's own doc comment).
struct FallTracker {
    lcu::f32 fall_distance = 0.0f;
    bool was_grounded = true;
};

// Real per-frame fall tracking + damage application: `delta_y` is
// `previous_y - current_y` (positive means the player moved down this
// frame). Accumulates `tracker.fall_distance` while airborne and
// descending; on the exact frame the player transitions from airborne
// to grounded, applies real fall damage to `health` via apply_damage
// (using the accumulated distance, per fall_damage_for_distance above)
// and resets the accumulator - whether or not that frame's `grounded`
// is true is the only thing that matters, not how long the player has
// been falling. Returns the real damage dealt this call (0 if none, the
// common case - most frames aren't a landing frame).
lcu::f32 update_fall_tracking(FallTracker& tracker, components::PlayerHealth& health, lcu::f32 delta_y, bool grounded);

// Real natural regeneration (Phase 51.1, this phase's own directive):
// while `hunger.current >= kHealthRegenMinHunger` and
// `health.current < health.max`, heals 1 HP every real
// kHealthRegenIntervalSeconds. `regen_accumulator` is a real elapsed-
// time counter the caller owns (persists across calls, reset internally
// here whenever a tick actually fires) - the same "caller-owned real
// accumulator" pattern this project's other real per-frame timers
// (`hand_swing_elapsed`, break-hold accumulators) already use.
constexpr lcu::f32 kHealthRegenIntervalSeconds = 4.0f;
constexpr lcu::f32 kHealthRegenMinHunger = 18.0f;
void update_health_regen(components::PlayerHealth& health, const components::PlayerHunger& hunger,
                          lcu::f32& regen_accumulator, lcu::f32 dt);

// Real starvation damage (Phase 51.2, this phase's own directive):
// while `hunger.current <= 0`, deals kStarvationDamagePerTick every real
// kStarvationDamageIntervalSeconds - same real accumulator pattern
// update_health_regen above uses. Returns true iff this call caused a
// real death transition (see apply_damage).
constexpr lcu::f32 kStarvationDamageIntervalSeconds = 4.0f;
constexpr lcu::f32 kStarvationDamagePerTick = 1.0f;
bool update_starvation(components::PlayerHealth& health, const components::PlayerHunger& hunger,
                        lcu::f32& starvation_accumulator, lcu::f32 dt);

// Real hunger drain (Phase 51.2, this phase's own directive): 1 point
// every real kHungerDrainIntervalSeconds normally, kHungerSprintDrainMultiplier
// times faster while `sprinting` is true - same real accumulator
// pattern as above.
constexpr lcu::f32 kHungerDrainIntervalSeconds = 30.0f;
constexpr lcu::f32 kHungerSprintDrainMultiplier = 2.0f;
void update_hunger_drain(components::PlayerHunger& hunger, bool sprinting, lcu::f32& drain_accumulator, lcu::f32 dt);

// Real, instant per-jump hunger cost (Phase 51.2) - the caller applies
// this once per real jump event (edge-detected at the call site: a
// fresh Jump press while grounded), not every frame Jump is held.
constexpr lcu::f32 kHungerPerJump = 0.05f;
void apply_jump_hunger_cost(components::PlayerHunger& hunger);

// Real eating (Phase 51.2): restores `restore_amount` hunger, clamped
// to `hunger.max`. Item consumption (removing it from the inventory) is
// the caller's own responsibility - this only touches hunger.
void eat(components::PlayerHunger& hunger, lcu::f32 restore_amount);

}  // namespace game::systems
