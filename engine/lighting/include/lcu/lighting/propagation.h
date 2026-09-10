#pragma once

#include <optional>
#include <queue>
#include <unordered_set>
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

namespace detail {

// Cross-chunk analog of VoxelPos (Phase 31): a block light BFS that
// crosses a chunk boundary needs to carry which chunk each frontier
// cell actually belongs to, not just its local coordinates.
struct WorldVoxelPos {
    voxel::ChunkCoord chunk;
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

// Steps one voxel along `offset` from `pos`, resolving into whichever
// chunk that lands in - reuses voxel::world_to_chunk_and_local (the
// same floor-division helper WorldLight's own cross-chunk queries
// already reuse) rather than hand-rolling the "did this step leave
// [0, EdgeLength)" arithmetic a third time in this codebase. Always
// resolves to *some* chunk+local pair; the caller still has to check
// whether that chunk is actually loaded before touching it.
template <u32 EdgeLength>
WorldVoxelPos step_cross_chunk(const WorldVoxelPos& pos, const i32 offset[3]) {
    const voxel::BlockWorldCoord world{
        static_cast<i64>(pos.chunk.x) * EdgeLength + static_cast<i64>(pos.x) + offset[0],
        static_cast<i64>(pos.chunk.y) * EdgeLength + static_cast<i64>(pos.y) + offset[1],
        static_cast<i64>(pos.chunk.z) * EdgeLength + static_cast<i64>(pos.z) + offset[2],
    };
    const voxel::ChunkAndLocal resolved = voxel::world_to_chunk_and_local(world, EdgeLength);
    return WorldVoxelPos{resolved.chunk, resolved.local.x, resolved.local.y, resolved.local.z};
}

// Cross-chunk breadth-first flood (Phase 31): the real generalization
// of flood_block_light above - identical decrement-and-spread logic,
// except a step that would leave the current chunk resolves into its
// real neighbor (via step_cross_chunk) instead of being clipped at the
// boundary. `ChunkProviderT` is duck-typed against exactly
// lcu::world::World's own `const ChunkStorage<EdgeLength>*
// chunk_at(ChunkCoord) const` (same reasoning as mesh_chunk_greedy's
// LightStorageT in engine/voxel - engine/world doesn't depend on
// engine/lighting, so a concrete #include would be fine dependency-
// wise, but the template keeps this header usable in isolation, e.g.
// from tests that only construct bare ChunkStorage instances, not a
// full World). A neighbor chunk that isn't loaded is simply not
// crossed into - nothing to write into, not a guess (see WorldLight's
// own "unknown -> honestly can't say" convention); a chunk that loads
// *later* doesn't retroactively receive light from a BFS that already
// finished, an honestly-scoped gap Phase 32/35 close.
//
// Terminates in at most kMaxLightLevel (15) steps from any seed in any
// direction, same as the single-chunk version - satisfies "BFS queue
// only runs over the radius actually affected by a change" without a
// separate hard cap, since the level-decrements-to-zero termination
// already bounds it.
//
// `touched_chunks` (Phase 33), when non-null, collects every distinct
// chunk coordinate this call actually wrote a light value into (never
// the seed's own chunk, unless the BFS wraps back into it) - the real
// answer to "which chunks besides the one I edited need remeshing",
// replacing a guess at a fixed neighbor radius with the BFS's own
// ground truth. Optional and defaulted so every caller that doesn't
// care (most unit tests, the single-chunk-equivalent call sites) isn't
// forced to pass one.
//
// Performance (Phase 34): the vast majority of steps in a real flood
// stay within the popped cell's own chunk - only a step landing right
// at a chunk's edge actually crosses one. An in-bounds fast path
// avoids step_cross_chunk's floor-division arithmetic and, more
// importantly, both of the two unordered_map lookups
// (`chunks.chunk_at`/`world_light.chunk_light`) the general
// cross-chunk-resolving path needs, for every in-chunk step - measured
// via tools/benchmark's BM_Lighting_PlaceTorchAtChunkEdge/
// BM_Lighting_UnplaceTorchAtChunkEdge to matter: this fast path is what
// brought both under the brief's explicit 0.5ms budget (see
// BUILD_STATUS.md for the real before/after numbers). The slow,
// general path (unchanged) still handles every actual boundary
// crossing.
template <u32 EdgeLength, typename ChunkProviderT>
void flood_block_light_cross_chunk(const ChunkProviderT& chunks, const voxel::BlockRegistry& registry,
                                    WorldLight<EdgeLength>& world_light, std::queue<WorldVoxelPos> queue,
                                    std::unordered_set<voxel::ChunkCoord>* touched_chunks = nullptr) {
    constexpr i32 N = static_cast<i32>(EdgeLength);

    while (!queue.empty()) {
        const WorldVoxelPos pos = queue.front();
        queue.pop();

        LightStorage<EdgeLength>* light_here = world_light.find_chunk_light_mutable(pos.chunk);
        if (!light_here) {
            continue;
        }
        const u8 level = light_here->block_light(pos.x, pos.y, pos.z);
        if (level <= 1) {
            continue;
        }
        const u8 next_level = static_cast<u8>(level - 1);

        // Fetched lazily (only the first time an in-chunk step actually
        // needs it) - a seed cell whose every neighbor happens to cross
        // a boundary never pays for this at all.
        const voxel::ChunkStorage<EdgeLength>* pos_chunk_storage = nullptr;

        for (const auto& offset : kNeighborOffsets) {
            const i32 nx = static_cast<i32>(pos.x) + offset[0];
            const i32 ny = static_cast<i32>(pos.y) + offset[1];
            const i32 nz = static_cast<i32>(pos.z) + offset[2];

            if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz >= 0 && nz < N) {
                // Fast path: stays within pos.chunk.
                if (!pos_chunk_storage) {
                    pos_chunk_storage = chunks.chunk_at(pos.chunk);
                    if (!pos_chunk_storage) {
                        break;  // Shouldn't happen (we already have real light data for this chunk) - stay honest.
                    }
                }
                const u32 ux = static_cast<u32>(nx);
                const u32 uy = static_cast<u32>(ny);
                const u32 uz = static_cast<u32>(nz);
                if (is_opaque(*pos_chunk_storage, registry, ux, uy, uz)) {
                    continue;
                }
                if (next_level > light_here->block_light(ux, uy, uz)) {
                    light_here->set_block_light(ux, uy, uz, next_level);
                    queue.push({pos.chunk, ux, uy, uz});
                }
            } else {
                // Slow path: this step genuinely crosses a chunk
                // boundary - step_cross_chunk always resolves to a
                // *different* chunk coordinate here (a real
                // out-of-[0,N) local coordinate always shifts the
                // owning chunk by the floor-division it applies).
                const WorldVoxelPos next = step_cross_chunk<EdgeLength>(pos, offset);
                const voxel::ChunkStorage<EdgeLength>* neighbor_chunk = chunks.chunk_at(next.chunk);
                if (!neighbor_chunk) {
                    continue;
                }
                if (is_opaque(*neighbor_chunk, registry, next.x, next.y, next.z)) {
                    continue;
                }

                LightStorage<EdgeLength>& neighbor_light = world_light.chunk_light(next.chunk);
                if (next_level > neighbor_light.block_light(next.x, next.y, next.z)) {
                    neighbor_light.set_block_light(next.x, next.y, next.z, next_level);
                    queue.push(next);
                    if (touched_chunks) {
                        touched_chunks->insert(next.chunk);
                    }
                }
            }
        }
    }
}

}  // namespace detail

// Cross-chunk counterpart to propagate_added_block_light (Phase 31):
// the caller must already have called
// world_light.chunk_light(coord).set_block_light(x, y, z, <emission>)
// - this only floods it outward, potentially into neighboring chunks.
// See flood_block_light_cross_chunk's doc comment for `ChunkProviderT`,
// the "chunk not loaded" honesty guarantee, and `touched_chunks`
// (Phase 33).
template <u32 EdgeLength, typename ChunkProviderT>
void propagate_added_block_light_cross_chunk(const ChunkProviderT& chunks, const voxel::BlockRegistry& registry,
                                              WorldLight<EdgeLength>& world_light, voxel::ChunkCoord coord, u32 x,
                                              u32 y, u32 z,
                                              std::unordered_set<voxel::ChunkCoord>* touched_chunks = nullptr) {
    std::queue<detail::WorldVoxelPos> queue;
    queue.push({coord, x, y, z});
    detail::flood_block_light_cross_chunk(chunks, registry, world_light, std::move(queue), touched_chunks);
}

// Cross-chunk counterpart to unpropagate_block_light (Phase 31): same
// two-phase darken-then-refill algorithm (see that function's doc
// comment for the algorithm itself and its known symmetric-source
// limitation), generalized to cross chunk boundaries the same way
// flood_block_light_cross_chunk does. A neighbor chunk that isn't
// loaded is simply not visited by either phase - it never received
// this source's light in the first place (propagation into it would
// have hit the same "not loaded" wall), so there's nothing there to
// darken or refill. `touched_chunks` (Phase 33) collects every chunk
// (besides `coord`) either phase actually wrote a light value into -
// see flood_block_light_cross_chunk's doc comment.
// Performance (Phase 34): same in-chunk fast path as
// flood_block_light_cross_chunk - see that function's doc comment.
// Here it also means the darken phase's common step never touches
// `chunks` (the block-data provider) at all, only `world_light`: an
// in-bounds neighbor's "is there light data here" is answered directly
// via the current node's own already-fetched LightStorage rather than
// a second lookup into a different map.
template <u32 EdgeLength, typename ChunkProviderT>
void unpropagate_block_light_cross_chunk(const ChunkProviderT& chunks, const voxel::BlockRegistry& registry,
                                          WorldLight<EdgeLength>& world_light, voxel::ChunkCoord coord, u32 x, u32 y,
                                          u32 z, u8 old_level,
                                          std::unordered_set<voxel::ChunkCoord>* touched_chunks = nullptr) {
    constexpr i32 N = static_cast<i32>(EdgeLength);

    struct RemovalNode {
        detail::WorldVoxelPos pos;
        u8 level;
    };

    std::queue<RemovalNode> removal_queue;
    std::queue<detail::WorldVoxelPos> refill_queue;

    world_light.chunk_light(coord).set_block_light(x, y, z, 0);
    removal_queue.push({{coord, x, y, z}, old_level});

    while (!removal_queue.empty()) {
        const RemovalNode node = removal_queue.front();
        removal_queue.pop();
        if (node.level == 0) {
            continue;
        }

        // node.pos.chunk always has real light data here: it's either
        // `coord` itself (seeded above) or a chunk a prior darken step
        // in this same call already wrote into - checked defensively
        // rather than assumed.
        LightStorage<EdgeLength>* node_light = world_light.find_chunk_light_mutable(node.pos.chunk);
        if (!node_light) {
            continue;
        }

        for (const auto& offset : detail::kNeighborOffsets) {
            const i32 nx = static_cast<i32>(node.pos.x) + offset[0];
            const i32 ny = static_cast<i32>(node.pos.y) + offset[1];
            const i32 nz = static_cast<i32>(node.pos.z) + offset[2];

            if (nx >= 0 && nx < N && ny >= 0 && ny < N && nz >= 0 && nz < N) {
                // Fast path: stays within node.pos.chunk.
                const u32 ux = static_cast<u32>(nx);
                const u32 uy = static_cast<u32>(ny);
                const u32 uz = static_cast<u32>(nz);
                const u8 neighbor_level = node_light->block_light(ux, uy, uz);
                if (neighbor_level == 0) {
                    continue;
                }
                if (neighbor_level < node.level) {
                    node_light->set_block_light(ux, uy, uz, 0);
                    removal_queue.push({{node.pos.chunk, ux, uy, uz}, neighbor_level});
                } else {
                    refill_queue.push({node.pos.chunk, ux, uy, uz});
                }
            } else {
                // Slow path: this step genuinely crosses a chunk
                // boundary - see flood_block_light_cross_chunk's
                // matching comment.
                const detail::WorldVoxelPos next = detail::step_cross_chunk<EdgeLength>(node.pos, offset);
                const voxel::ChunkStorage<EdgeLength>* neighbor_chunk = chunks.chunk_at(next.chunk);
                if (!neighbor_chunk) {
                    continue;
                }

                LightStorage<EdgeLength>& neighbor_light = world_light.chunk_light(next.chunk);
                const u8 neighbor_level = neighbor_light.block_light(next.x, next.y, next.z);
                if (neighbor_level == 0) {
                    continue;
                }
                if (neighbor_level < node.level) {
                    neighbor_light.set_block_light(next.x, next.y, next.z, 0);
                    removal_queue.push({next, neighbor_level});
                    if (touched_chunks) {
                        touched_chunks->insert(next.chunk);
                    }
                } else {
                    refill_queue.push(next);
                }
            }
        }
    }

    detail::flood_block_light_cross_chunk(chunks, registry, world_light, std::move(refill_queue), touched_chunks);
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
