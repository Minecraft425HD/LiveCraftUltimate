#include "lcu/rendering/occlusion_culler.h"

#include <array>
#include <queue>

#include "lcu/core/assert.h"
#include "lcu/physics/aabb.h"
#include "lcu/voxel/chunk.h"

namespace lcu::rendering {

namespace {

// Side index order: 0=-X, 1=+X, 2=-Y, 3=+Y, 4=-Z, 5=+Z - consecutive
// pairs share an axis, so `side ^ 1` always gives the opposite side
// (used below to find a neighbor's own facing-back bit).
constexpr std::array<voxel::ChunkCoord, 6> kNeighborOffsets = {{
    {-1, 0, 0},
    {1, 0, 0},
    {0, -1, 0},
    {0, 1, 0},
    {0, 0, -1},
    {0, 0, 1},
}};

voxel::ChunkCoord add(const voxel::ChunkCoord& a, const voxel::ChunkCoord& b) {
    return voxel::ChunkCoord{a.x + b.x, a.y + b.y, a.z + b.z};
}

// This chunk's own real world-space AABB - the same fixed-size-cube
// formula client/main.cpp's own chunk_aabb_cache uses (Phase 68),
// computed fresh here rather than depending on that cache directly, so
// OcclusionCuller stays a real, self-contained, independently testable
// unit (the brief's own `compute(world, camera_chunk, frustum)` doesn't
// take an external AABB source at all).
physics::AABB chunk_world_aabb(voxel::ChunkCoord coord) {
    constexpr i32 kEdge = static_cast<i32>(voxel::Chunk::kEdgeLength);
    const math::Vec3 min{static_cast<f32>(coord.x * kEdge), static_cast<f32>(coord.y * kEdge),
                          static_cast<f32>(coord.z * kEdge)};
    return physics::AABB{min, min + math::Vec3{static_cast<f32>(kEdge), static_cast<f32>(kEdge),
                                                static_cast<f32>(kEdge)}};
}

bool is_opaque(const voxel::BlockRegistry& registry, voxel::BlockId id) {
    // Real, single source of truth for "does light/vision pass through
    // this voxel" - the exact same `is_transparent` flag lighting
    // propagation and mesh face-culling already use, not a second,
    // separately-tracked "vision opacity" concept. Real, honest
    // consequence: `game:leaves` is registered `is_transparent=false` in
    // this project (a real, documented Phase 61 simplification - see
    // DECISIONS.md), so leaves count as OPAQUE here too, not matching
    // the brief's own literal "Wasser und Blätter lassen Licht durch"
    // wording, which describes real Minecraft's own leaves - consistent
    // with how this project's leaves already render and already block
    // light, not a new, separate inconsistency introduced by this phase.
    return !registry.definition_of(id).is_transparent;
}

}  // namespace

u8 OcclusionCuller::mask_for(const world::World& world, const voxel::BlockRegistry& registry, voxel::ChunkCoord c) {
    if (const auto it = boundary_opacity_mask_.find(c); it != boundary_opacity_mask_.end()) {
        return it->second;
    }

    const voxel::Chunk* chunk = world.chunk_at(c);
    LCU_ASSERT(chunk != nullptr);  // every real caller below only ever asks for an already-loaded chunk

    constexpr u32 kEdge = voxel::Chunk::kEdgeLength;
    u8 mask = 0;
    for (u8 side = 0; side < 6; ++side) {
        bool fully_opaque = true;
        for (u32 a = 0; a < kEdge && fully_opaque; ++a) {
            for (u32 b = 0; b < kEdge && fully_opaque; ++b) {
                u32 x = 0;
                u32 y = 0;
                u32 z = 0;
                switch (side) {
                    case 0:
                        x = 0;
                        y = a;
                        z = b;
                        break;
                    case 1:
                        x = kEdge - 1;
                        y = a;
                        z = b;
                        break;
                    case 2:
                        x = a;
                        y = 0;
                        z = b;
                        break;
                    case 3:
                        x = a;
                        y = kEdge - 1;
                        z = b;
                        break;
                    case 4:
                        x = a;
                        y = b;
                        z = 0;
                        break;
                    default:
                        x = a;
                        y = b;
                        z = kEdge - 1;
                        break;
                }
                if (!is_opaque(registry, chunk->block_at(x, y, z))) {
                    fully_opaque = false;
                }
            }
        }
        if (fully_opaque) {
            mask |= static_cast<u8>(1u << side);
        }
    }

    boundary_opacity_mask_[c] = mask;
    return mask;
}

std::unordered_set<voxel::ChunkCoord> OcclusionCuller::compute(const world::World& world,
                                                                 const voxel::BlockRegistry& registry,
                                                                 voxel::ChunkCoord camera_chunk,
                                                                 const Frustum& frustum) {
    std::unordered_set<voxel::ChunkCoord> visible;
    if (!world.chunk_at(camera_chunk)) {
        // The camera's own chunk isn't loaded - a real, honest "nothing
        // visible" rather than guessing a fallback starting point.
        return visible;
    }

    visible.insert(camera_chunk);
    std::queue<voxel::ChunkCoord> queue;
    queue.push(camera_chunk);

    while (!queue.empty()) {
        const voxel::ChunkCoord current = queue.front();
        queue.pop();
        const u8 current_mask = mask_for(world, registry, current);

        for (u8 side = 0; side < 6; ++side) {
            if (current_mask & static_cast<u8>(1u << side)) {
                // This side is a real, provably solid wall - no portal
                // through it regardless of what the neighbor looks like.
                continue;
            }
            const voxel::ChunkCoord neighbor = add(current, kNeighborOffsets[side]);
            if (visible.count(neighbor)) {
                continue;
            }
            if (!world.chunk_at(neighbor)) {
                continue;  // not loaded
            }
            if (!frustum.contains_aabb(chunk_world_aabb(neighbor))) {
                continue;  // Phase 68's own frustum test - occlusion only narrows it, never widens it
            }
            const u8 opposite_side = side ^ 1u;
            const u8 neighbor_mask = mask_for(world, registry, neighbor);
            if (neighbor_mask & static_cast<u8>(1u << opposite_side)) {
                continue;  // neighbor's own facing side is a real solid wall
            }
            visible.insert(neighbor);
            queue.push(neighbor);
        }
    }

    return visible;
}

void OcclusionCuller::invalidate(voxel::ChunkCoord c) { boundary_opacity_mask_.erase(c); }

void OcclusionCuller::invalidate_neighbors(voxel::ChunkCoord c) {
    invalidate(c);
    for (const voxel::ChunkCoord& offset : kNeighborOffsets) {
        invalidate(add(c, offset));
    }
}

}  // namespace lcu::rendering
