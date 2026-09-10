#pragma once

#include <optional>
#include <queue>
#include <utility>

#include "lcu/lighting/light_storage.h"
#include "lcu/lighting/world_light.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"

namespace lcu::lighting {

namespace detail {

struct VoxelPos {
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

constexpr i32 kNeighborOffsets[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

template <u32 EdgeLength>
bool is_opaque(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry, u32 x, u32 y,
               u32 z) {
    return !registry.definition_of(chunk.block_at(x, y, z)).is_transparent;
}

// Breadth-first flood of block light outward from every position already
// in `queue`, assuming each has already had its own light level set by
// the caller (a fresh emitter, or a boundary cell being re-flooded after
// removal). Only steps within the chunk - see DECISIONS.md "Lighting is
// single-chunk scoped".
template <u32 EdgeLength>
void flood_block_light(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry,
                        LightStorage<EdgeLength>& light, std::queue<VoxelPos> queue) {
    constexpr i32 N = static_cast<i32>(EdgeLength);

    while (!queue.empty()) {
        const VoxelPos pos = queue.front();
        queue.pop();

        const u8 level = light.block_light(pos.x, pos.y, pos.z);
        if (level <= 1) {
            continue;  // level - 1 would be 0: nothing left to spread.
        }

        for (const auto& offset : kNeighborOffsets) {
            const i32 nx = static_cast<i32>(pos.x) + offset[0];
            const i32 ny = static_cast<i32>(pos.y) + offset[1];
            const i32 nz = static_cast<i32>(pos.z) + offset[2];
            if (nx < 0 || ny < 0 || nz < 0 || nx >= N || ny >= N || nz >= N) {
                continue;
            }
            const u32 ux = static_cast<u32>(nx);
            const u32 uy = static_cast<u32>(ny);
            const u32 uz = static_cast<u32>(nz);
            if (is_opaque(chunk, registry, ux, uy, uz)) {
                continue;
            }

            const u8 next_level = static_cast<u8>(level - 1);
            if (next_level > light.block_light(ux, uy, uz)) {
                light.set_block_light(ux, uy, uz, next_level);
                queue.push({ux, uy, uz});
            }
        }
    }
}

}  // namespace detail

// Full initial block-light computation for `chunk`: clears all block
// light, then floods outward from every block with
// BlockDefinition::light_emission > 0 (brief section 24). This is the
// one-time "what does a freshly generated/loaded chunk's lighting look
// like" pass - a per-chunk operation, not a whole-world recompute.
// Editing a single block afterward should use
// propagate_added_block_light/unpropagate_block_light below instead of
// calling this again, so a single edit stays a local update rather than
// re-flooding the entire chunk.
template <u32 EdgeLength>
void compute_block_light(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry,
                          LightStorage<EdgeLength>& light) {
    for (u32 z = 0; z < EdgeLength; ++z) {
        for (u32 y = 0; y < EdgeLength; ++y) {
            for (u32 x = 0; x < EdgeLength; ++x) {
                light.set_block_light(x, y, z, 0);
            }
        }
    }

    std::queue<detail::VoxelPos> seeds;
    for (u32 z = 0; z < EdgeLength; ++z) {
        for (u32 y = 0; y < EdgeLength; ++y) {
            for (u32 x = 0; x < EdgeLength; ++x) {
                const u8 emission = registry.definition_of(chunk.block_at(x, y, z)).light_emission;
                if (emission > 0) {
                    light.set_block_light(x, y, z, emission);
                    seeds.push({x, y, z});
                }
            }
        }
    }

    detail::flood_block_light(chunk, registry, light, std::move(seeds));
}

// Incremental local update for a single newly-placed light-emitting
// block at (x,y,z): the caller must already have called
// light.set_block_light(x, y, z, <the block's emission>) - this only
// handles flooding that light outward to already-dimmer neighbors. Does
// not touch anything outside the BFS frontier this one source actually
// reaches, unlike compute_block_light's full-chunk pass.
template <u32 EdgeLength>
void propagate_added_block_light(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry,
                                  LightStorage<EdgeLength>& light, u32 x, u32 y, u32 z) {
    std::queue<detail::VoxelPos> queue;
    queue.push({x, y, z});
    detail::flood_block_light(chunk, registry, light, std::move(queue));
}

// Incremental local update for removing a light source (or an opaque
// block that was blocking light) at (x,y,z), which held `old_level`
// block light immediately before this call. Standard two-phase BFS
// light removal: first darken every neighbor whose light is strictly
// less than the level being retracted (it could only have arrived via
// propagation traceable back through this position), collecting any
// boundary cell whose light is >= the retracted level (independently,
// validly lit by something else) into a refill set; then re-flood block
// light outward from that refill set so light correctly flows back in
// from those other sources instead of leaving a permanently dark hole.
//
// Known limitation shared with every engine using this classic
// algorithm: in rare symmetric configurations where two sources
// coincidentally produce the exact same light level at the same cell,
// the darken phase can walk slightly further than strictly necessary
// before the refill phase heals it back - a full recompute would not
// have this artifact, but is also not a "local update" (see
// DECISIONS.md).
template <u32 EdgeLength>
void unpropagate_block_light(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry,
                              LightStorage<EdgeLength>& light, u32 x, u32 y, u32 z, u8 old_level) {
    constexpr i32 N = static_cast<i32>(EdgeLength);

    struct RemovalNode {
        detail::VoxelPos pos;
        u8 level;
    };

    std::queue<RemovalNode> removal_queue;
    std::queue<detail::VoxelPos> refill_queue;

    light.set_block_light(x, y, z, 0);
    removal_queue.push({{x, y, z}, old_level});

    while (!removal_queue.empty()) {
        const RemovalNode node = removal_queue.front();
        removal_queue.pop();
        if (node.level == 0) {
            continue;
        }

        for (const auto& offset : detail::kNeighborOffsets) {
            const i32 nx = static_cast<i32>(node.pos.x) + offset[0];
            const i32 ny = static_cast<i32>(node.pos.y) + offset[1];
            const i32 nz = static_cast<i32>(node.pos.z) + offset[2];
            if (nx < 0 || ny < 0 || nz < 0 || nx >= N || ny >= N || nz >= N) {
                continue;
            }
            const u32 ux = static_cast<u32>(nx);
            const u32 uy = static_cast<u32>(ny);
            const u32 uz = static_cast<u32>(nz);

            const u8 neighbor_level = light.block_light(ux, uy, uz);
            if (neighbor_level == 0) {
                continue;
            }
            if (neighbor_level < node.level) {
                light.set_block_light(ux, uy, uz, 0);
                removal_queue.push({{ux, uy, uz}, neighbor_level});
            } else {
                refill_queue.push({ux, uy, uz});
            }
        }
    }

    detail::flood_block_light(chunk, registry, light, std::move(refill_queue));
}

// Fills sky light straight down one column (x,z) of `chunk`: full
// brightness (kMaxLightLevel) until the first opaque block, 0 at and
// below it. `sky_open_above` (Phase 30) seeds whether sky is still open
// by the time it reaches this chunk's own top layer - default `true`
// preserves this function's original single-chunk-scoped behavior (the
// chunk's own top is always treated as open) for every existing caller;
// `compute_sky_light_column_cross_chunk` below is what actually passes
// a real value, queried from the chunk directly above via WorldLight.
// Genuinely local otherwise - after a single block edit, call this
// again for just that block's (x,z) column (16 cells) rather than
// recomputing the whole chunk; compute_sky_light below is only the
// initial full-chunk pass, built by calling this once per column.
//
// Known simplification: no lateral spreading under overhangs (real
// sunlight leaks a little sideways beneath a ledge; block light spreads
// in all 6 directions but sky light here only ever travels straight
// down) - see DECISIONS.md. Vertical cross-chunk awareness (a chunk
// with something solid directly above it, in the chunk above,
// correctly darkening this chunk's own top layer) exists as of Phase
// 30, via `sky_open_above`/compute_sky_light_column_cross_chunk - but
// only when the neighbor chunk is loaded and its own light has already
// been computed; an unloaded/not-yet-lit neighbor still means "assume
// open" (see WorldLight's own doc comment), same as before this phase.
template <u32 EdgeLength>
void compute_sky_light_column(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry,
                               LightStorage<EdgeLength>& light, u32 x, u32 z, bool sky_open_above = true) {
    bool blocked = !sky_open_above;
    for (i32 y = static_cast<i32>(EdgeLength) - 1; y >= 0; --y) {
        const u32 uy = static_cast<u32>(y);
        if (!blocked && detail::is_opaque(chunk, registry, x, uy, z)) {
            blocked = true;
        }
        light.set_sky_light(x, uy, z, blocked ? 0 : LightStorage<EdgeLength>::kMaxLightLevel);
    }
}

// Cross-chunk-aware sky light for one column (Phase 30): queries the
// chunk directly above (coord.y + 1) via `world_light` and seeds this
// column's `sky_open_above` from whether that neighbor's own column is
// still open by its bottom cell (sky_light(x, 0, z) > 0) - if the
// neighbor isn't loaded, or hasn't been lit yet, assumes open sky
// (WorldLight's own "unknown -> best case, not guessed dark"
// convention), which is also exactly this function's behavior for a
// chunk at the top of the currently-loaded world (nothing above it to
// query), identical to compute_sky_light_column's original default.
// Writes into `world_light`'s own LightStorage for `coord`
// (get-or-create, same as WorldLight::chunk_light).
template <u32 EdgeLength>
void compute_sky_light_column_cross_chunk(const voxel::ChunkStorage<EdgeLength>& chunk,
                                           const voxel::BlockRegistry& registry, WorldLight<EdgeLength>& world_light,
                                           voxel::ChunkCoord coord, u32 x, u32 z) {
    const voxel::ChunkCoord above{coord.x, coord.y + 1, coord.z};
    const std::optional<u8> above_bottom_light = world_light.sky_light_at(above, static_cast<i32>(x), 0, static_cast<i32>(z));
    const bool sky_open_above = !above_bottom_light.has_value() || *above_bottom_light > 0;
    compute_sky_light_column(chunk, registry, world_light.chunk_light(coord), x, z, sky_open_above);
}

// Whole-chunk cross-chunk-aware sky light (Phase 30): the vertical-
// cross-chunk-aware counterpart to compute_sky_light below. Callers
// computing an entire loaded world's sky light must still process
// chunks top-down within each (x,z) column stack (highest chunk_y
// first) for this to actually cascade correctly - this function only
// computes one chunk correctly *given* that the chunk above (if any)
// was already computed; it doesn't itself enforce an evaluation order
// across multiple chunks.
template <u32 EdgeLength>
void compute_sky_light_cross_chunk(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry,
                                    WorldLight<EdgeLength>& world_light, voxel::ChunkCoord coord) {
    for (u32 z = 0; z < EdgeLength; ++z) {
        for (u32 x = 0; x < EdgeLength; ++x) {
            compute_sky_light_column_cross_chunk(chunk, registry, world_light, coord, x, z);
        }
    }
}

template <u32 EdgeLength>
void compute_sky_light(const voxel::ChunkStorage<EdgeLength>& chunk, const voxel::BlockRegistry& registry,
                        LightStorage<EdgeLength>& light) {
    for (u32 z = 0; z < EdgeLength; ++z) {
        for (u32 x = 0; x < EdgeLength; ++x) {
            compute_sky_light_column(chunk, registry, light, x, z);
        }
    }
}

}  // namespace lcu::lighting
