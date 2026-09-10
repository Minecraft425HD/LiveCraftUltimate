#include "lcu/physics/collision.h"

#include <cmath>

namespace lcu::physics {

namespace {

constexpr f32 kEpsilon = 1e-4f;

f32 axis_get(const math::Vec3& v, int axis) {
    switch (axis) {
        case 0:
            return v.x;
        case 1:
            return v.y;
        default:
            return v.z;
    }
}

bool block_is_solid(const world::World& world, i64 x, i64 y, i64 z,
                     const std::function<bool(voxel::BlockId)>& is_solid) {
    const voxel::ChunkAndLocal split = voxel::world_to_chunk_and_local({x, y, z}, voxel::Chunk::kEdgeLength);
    const voxel::Chunk* chunk = world.chunk_at(split.chunk);
    if (!chunk) {
        return false;
    }
    return is_solid(chunk->block_at(split.local.x, split.local.y, split.local.z));
}

// Returns the maximum movement along `axis` (same sign as `delta`,
// magnitude <= |delta|) that doesn't move `aabb` into a solid block.
f32 sweep_axis(const world::World& world, const AABB& aabb, int axis, f32 delta,
               const std::function<bool(voxel::BlockId)>& is_solid) {
    if (delta == 0.0f) {
        return 0.0f;
    }

    const int a0 = (axis + 1) % 3;
    const int a1 = (axis + 2) % 3;

    // The AABB's footprint on the two perpendicular axes, as inclusive
    // block-layer ranges. Epsilon-shrunk so an edge sitting exactly on
    // an integer boundary doesn't spuriously pull in the next layer.
    const i64 min0 = static_cast<i64>(std::floor(axis_get(aabb.min, a0) + kEpsilon));
    const i64 max0 = static_cast<i64>(std::floor(axis_get(aabb.max, a0) - kEpsilon));
    const i64 min1 = static_cast<i64>(std::floor(axis_get(aabb.min, a1) + kEpsilon));
    const i64 max1 = static_cast<i64>(std::floor(axis_get(aabb.max, a1) - kEpsilon));

    const f32 leading = delta > 0.0f ? axis_get(aabb.max, axis) : axis_get(aabb.min, axis);
    const f32 target = leading + delta;

    f32 allowed = delta;

    if (delta > 0.0f) {
        const i64 first_layer = static_cast<i64>(std::floor(leading));
        const i64 last_layer = static_cast<i64>(std::floor(target));
        for (i64 layer = first_layer; layer <= last_layer; ++layer) {
            for (i64 c0 = min0; c0 <= max0; ++c0) {
                for (i64 c1 = min1; c1 <= max1; ++c1) {
                    i64 coords[3];
                    coords[axis] = layer;
                    coords[a0] = c0;
                    coords[a1] = c1;
                    if (block_is_solid(world, coords[0], coords[1], coords[2], is_solid)) {
                        // Block `layer` occupies [layer, layer+1); moving
                        // in +axis, we stop right at its near face.
                        const f32 candidate = static_cast<f32>(layer) - leading - kEpsilon;
                        if (candidate < allowed) {
                            allowed = candidate;
                        }
                    }
                }
            }
        }
    } else {
        const i64 first_layer = static_cast<i64>(std::floor(leading - kEpsilon));
        const i64 last_layer = static_cast<i64>(std::floor(target));
        for (i64 layer = first_layer; layer >= last_layer; --layer) {
            for (i64 c0 = min0; c0 <= max0; ++c0) {
                for (i64 c1 = min1; c1 <= max1; ++c1) {
                    i64 coords[3];
                    coords[axis] = layer;
                    coords[a0] = c0;
                    coords[a1] = c1;
                    if (block_is_solid(world, coords[0], coords[1], coords[2], is_solid)) {
                        // Moving in -axis, we stop at the block's far
                        // (upper) face.
                        const f32 candidate = static_cast<f32>(layer + 1) - leading + kEpsilon;
                        if (candidate > allowed) {
                            allowed = candidate;
                        }
                    }
                }
            }
        }
    }

    if (delta > 0.0f && allowed < 0.0f) {
        allowed = 0.0f;
    }
    if (delta < 0.0f && allowed > 0.0f) {
        allowed = 0.0f;
    }
    return allowed;
}

// Small probe distance for grounded detection: whether *any* further
// downward movement (independent of this frame's own delta.y - a
// player who isn't currently falling and made no vertical move request
// is still grounded if there's floor right under them) is immediately
// blocked.
constexpr f32 kGroundProbeDistance = 0.05f;

bool probe_grounded(const world::World& world, const AABB& aabb, const std::function<bool(voxel::BlockId)>& is_solid) {
    const CollisionResult probe = move_and_collide(world, aabb, {0.0f, -kGroundProbeDistance, 0.0f}, is_solid);
    return probe.hit_y;
}

}  // namespace

CollisionResult move_and_collide(const world::World& world, const AABB& aabb, const math::Vec3& delta,
                                  const std::function<bool(voxel::BlockId)>& is_solid) {
    CollisionResult result;
    AABB current = aabb;

    const f32 allowed_y = sweep_axis(world, current, 1, delta.y, is_solid);
    current = current.translated({0.0f, allowed_y, 0.0f});
    result.hit_y = std::abs(allowed_y - delta.y) > kEpsilon;
    result.grounded = delta.y < 0.0f && result.hit_y;

    const f32 allowed_x = sweep_axis(world, current, 0, delta.x, is_solid);
    current = current.translated({allowed_x, 0.0f, 0.0f});
    result.hit_x = std::abs(allowed_x - delta.x) > kEpsilon;

    const f32 allowed_z = sweep_axis(world, current, 2, delta.z, is_solid);
    current = current.translated({0.0f, 0.0f, allowed_z});
    result.hit_z = std::abs(allowed_z - delta.z) > kEpsilon;

    result.resolved_delta = {allowed_x, allowed_y, allowed_z};
    return result;
}

void apply_gravity(PlayerPhysicsState& state, const PlayerPhysicsConfig& config, f32 dt, bool in_water) {
    const f32 gravity = in_water ? config.gravity * 0.3f : config.gravity;
    const f32 max_fall = in_water ? config.max_fall_speed * 0.3f : config.max_fall_speed;

    state.vertical_velocity += gravity * dt;
    if (state.vertical_velocity < max_fall) {
        state.vertical_velocity = max_fall;
    }
}

void try_jump(PlayerPhysicsState& state, const PlayerPhysicsConfig& config, bool in_water) {
    if (!state.grounded) {
        return;
    }
    state.vertical_velocity = in_water ? config.jump_speed * 0.4f : config.jump_speed;
}

void integrate_player(const world::World& world, PlayerPhysicsState& state, const math::Vec3& horizontal_delta,
                       const PlayerPhysicsConfig& config, f32 dt,
                       const std::function<bool(voxel::BlockId)>& is_solid) {
    const math::Vec3 delta{horizontal_delta.x, state.vertical_velocity * dt, horizontal_delta.z};
    const CollisionResult result = move_and_collide(world, state.aabb, delta, is_solid);

    const bool wants_horizontal_move = delta.x != 0.0f || delta.z != 0.0f;
    if (state.grounded && wants_horizontal_move && (result.hit_x || result.hit_z)) {
        // Auto-step: try rising by step_height, then the full horizontal
        // delta, then settling back down. Only commit if the rise itself
        // was unobstructed and the horizontal move at the raised height
        // is completely clear - otherwise fall through to the normal
        // (blocked) resolution below.
        const CollisionResult rise =
            move_and_collide(world, state.aabb, {0.0f, config.step_height, 0.0f}, is_solid);
        if (std::abs(rise.resolved_delta.y - config.step_height) <= kEpsilon) {
            const AABB raised = state.aabb.translated({0.0f, rise.resolved_delta.y, 0.0f});
            const CollisionResult horizontal =
                move_and_collide(world, raised, {delta.x, 0.0f, delta.z}, is_solid);
            if (!horizontal.hit_x && !horizontal.hit_z) {
                const AABB after_horizontal =
                    raised.translated({horizontal.resolved_delta.x, 0.0f, horizontal.resolved_delta.z});
                const CollisionResult settle =
                    move_and_collide(world, after_horizontal, {0.0f, -config.step_height, 0.0f}, is_solid);
                state.aabb = after_horizontal.translated({0.0f, settle.resolved_delta.y, 0.0f});
                state.grounded = probe_grounded(world, state.aabb, is_solid);
                return;
            }
        }
    }

    state.aabb = state.aabb.translated(result.resolved_delta);
    if (result.hit_y) {
        state.vertical_velocity = 0.0f;
    }
    state.grounded = probe_grounded(world, state.aabb, is_solid);
}

}  // namespace lcu::physics
