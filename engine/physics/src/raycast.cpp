#include "lcu/physics/raycast.h"

#include <cmath>
#include <limits>

namespace lcu::physics {

namespace {

std::optional<voxel::BlockId> block_at_world(const world::World& world, voxel::BlockWorldCoord coord) {
    const voxel::ChunkAndLocal split = voxel::world_to_chunk_and_local(coord, voxel::Chunk::kEdgeLength);
    const voxel::Chunk* chunk = world.chunk_at(split.chunk);
    if (!chunk) {
        return std::nullopt;
    }
    return chunk->block_at(split.local.x, split.local.y, split.local.z);
}

RaycastHit make_hit(i64 voxel_x, i64 voxel_y, i64 voxel_z, const math::Vec3& normal, f32 distance,
                     voxel::BlockId block) {
    RaycastHit hit;
    hit.world = {voxel_x, voxel_y, voxel_z};
    const voxel::ChunkAndLocal split = voxel::world_to_chunk_and_local(hit.world, voxel::Chunk::kEdgeLength);
    hit.chunk = split.chunk;
    hit.local = split.local;
    hit.normal = normal;
    hit.distance = distance;
    hit.block = block;
    return hit;
}

f32 compute_t_max(f32 origin_axis, i64 voxel_axis, f32 dir_axis, i32 step_axis) {
    if (step_axis == 0) {
        return std::numeric_limits<f32>::infinity();
    }
    const f32 boundary = step_axis > 0 ? static_cast<f32>(voxel_axis + 1) : static_cast<f32>(voxel_axis);
    return (boundary - origin_axis) / dir_axis;
}

f32 compute_t_delta(f32 dir_axis, i32 step_axis) {
    if (step_axis == 0) {
        return std::numeric_limits<f32>::infinity();
    }
    return std::abs(1.0f / dir_axis);
}

}  // namespace

std::optional<RaycastHit> raycast(const world::World& world, math::Vec3 origin, math::Vec3 direction,
                                   f32 max_distance, const std::function<bool(voxel::BlockId)>& is_solid) {
    if (direction.x == 0.0f && direction.y == 0.0f && direction.z == 0.0f) {
        return std::nullopt;
    }
    direction = math::normalize(direction);

    i64 voxel_x = static_cast<i64>(std::floor(origin.x));
    i64 voxel_y = static_cast<i64>(std::floor(origin.y));
    i64 voxel_z = static_cast<i64>(std::floor(origin.z));

    // Origin already inside a solid block: no face was crossed to get
    // here, so there is no meaningful entry normal.
    if (const auto b = block_at_world(world, {voxel_x, voxel_y, voxel_z}); b && is_solid(*b)) {
        return make_hit(voxel_x, voxel_y, voxel_z, math::Vec3{0.0f, 0.0f, 0.0f}, 0.0f, *b);
    }

    const i32 step_x = direction.x > 0.0f ? 1 : (direction.x < 0.0f ? -1 : 0);
    const i32 step_y = direction.y > 0.0f ? 1 : (direction.y < 0.0f ? -1 : 0);
    const i32 step_z = direction.z > 0.0f ? 1 : (direction.z < 0.0f ? -1 : 0);

    f32 t_max_x = compute_t_max(origin.x, voxel_x, direction.x, step_x);
    f32 t_max_y = compute_t_max(origin.y, voxel_y, direction.y, step_y);
    f32 t_max_z = compute_t_max(origin.z, voxel_z, direction.z, step_z);
    const f32 t_delta_x = compute_t_delta(direction.x, step_x);
    const f32 t_delta_y = compute_t_delta(direction.y, step_y);
    const f32 t_delta_z = compute_t_delta(direction.z, step_z);

    f32 traveled = 0.0f;
    while (traveled <= max_distance) {
        math::Vec3 normal{0.0f, 0.0f, 0.0f};

        if (t_max_x < t_max_y && t_max_x < t_max_z) {
            voxel_x += step_x;
            traveled = t_max_x;
            t_max_x += t_delta_x;
            normal.x = static_cast<f32>(-step_x);
        } else if (t_max_y < t_max_z) {
            voxel_y += step_y;
            traveled = t_max_y;
            t_max_y += t_delta_y;
            normal.y = static_cast<f32>(-step_y);
        } else {
            voxel_z += step_z;
            traveled = t_max_z;
            t_max_z += t_delta_z;
            normal.z = static_cast<f32>(-step_z);
        }

        if (traveled > max_distance) {
            break;
        }

        if (const auto b = block_at_world(world, {voxel_x, voxel_y, voxel_z}); b && is_solid(*b)) {
            return make_hit(voxel_x, voxel_y, voxel_z, normal, traveled, *b);
        }
    }

    return std::nullopt;
}

}  // namespace lcu::physics
