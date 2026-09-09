#pragma once

#include <functional>

#include "lcu/physics/aabb.h"
#include "lcu/voxel/block_id.h"
#include "lcu/world/world.h"

namespace lcu::physics {

struct CollisionResult {
    // The movement actually applied, after clamping each axis to the
    // nearest solid block boundary it would otherwise have penetrated.
    math::Vec3 resolved_delta{0.0f, 0.0f, 0.0f};
    bool hit_x = false;
    bool hit_y = false;
    bool hit_z = false;
    // True iff movement was blocked while moving downward (y < 0) -
    // i.e. the AABB is resting on something.
    bool grounded = false;
};

// Moves `aabb` by `delta` (brief section 25: AABB + voxel collision),
// resolving each axis independently and in Y -> X -> Z order against
// solid blocks in `world` (per `is_solid`). Resolving one axis at a time
// against the position already settled on the previous axis is the same
// simplification classic block-game engines use to avoid full swept-AABB
// vs. swept-AABB math - it's not perfectly physically accurate at very
// high speeds/low tick rates, but correct and cheap for block-scale
// movement deltas. Never allocates, never touches blocks outside the
// AABB's swept footprint - unloaded chunks are treated as empty (not
// solid), same as raycast().
CollisionResult move_and_collide(const world::World& world, const AABB& aabb, const math::Vec3& delta,
                                  const std::function<bool(voxel::BlockId)>& is_solid);

struct PlayerPhysicsConfig {
    f32 gravity = -32.0f;          // blocks/s^2
    f32 jump_speed = 9.0f;         // blocks/s, initial upward velocity
    f32 max_fall_speed = -50.0f;   // blocks/s, terminal velocity
    f32 step_height = 0.51f;       // tallest ledge auto-stepped over (brief section 25 "stepping")
};

struct PlayerPhysicsState {
    AABB aabb;
    // Only vertical velocity is tracked as persistent state; horizontal
    // movement is direct per-call input (see integrate_player) rather
    // than accumulated, which is a common and simpler choice for
    // block-game player controllers (no horizontal momentum/sliding).
    f32 vertical_velocity = 0.0f;
    bool grounded = false;
};

// Applies gravity to `state.vertical_velocity`, clamped to
// `config.max_fall_speed`. `in_water` scales both down (brief section 25
// "swimming") - there is no water block registered anywhere yet to
// detect this from internally, so callers decide and pass it in; that
// keeps engine/physics decoupled from BlockRegistry-specific block
// types.
void apply_gravity(PlayerPhysicsState& state, const PlayerPhysicsConfig& config, f32 dt, bool in_water = false);

// Sets an upward vertical_velocity if `state.grounded` (or a smaller
// "swim up" impulse if `in_water`); no-op otherwise - can't jump/swim-up
// from midair.
void try_jump(PlayerPhysicsState& state, const PlayerPhysicsConfig& config, bool in_water = false);

// Applies `horizontal_delta` (x/z; y ignored) together with vertical
// motion from `state.vertical_velocity * dt`, resolved against `world`
// via move_and_collide. If horizontal movement is blocked while grounded
// and there is headroom to rise by `config.step_height` and continue
// unobstructed, the AABB is stepped up onto the ledge instead of
// stopping (brief section 25's basic stepping - not full staircase
// pathing, just a single auto-step check). Updates `state.aabb` and
// zeroes `state.vertical_velocity` on a vertical hit. `state.grounded`
// is set via a small dedicated downward ground-probe after movement,
// not merely from whether this frame's own vertical delta was blocked -
// a stationary player standing on solid ground (no fall, no jump this
// frame) is still grounded.
//
// Crouching isn't separate state here: brief section 25 lists it
// alongside jump/step, but the only thing that actually differs is the
// AABB's height/movement speed - callers already control both (pass a
// shorter `state.aabb` and/or a smaller `horizontal_delta`), so there is
// no dedicated crouch flag to add until a caller needs behavior beyond
// that (e.g. edge-detection to prevent walking off ledges while
// crouched - no such caller exists yet).
void integrate_player(const world::World& world, PlayerPhysicsState& state, const math::Vec3& horizontal_delta,
                       const PlayerPhysicsConfig& config, f32 dt, const std::function<bool(voxel::BlockId)>& is_solid);

}  // namespace lcu::physics
