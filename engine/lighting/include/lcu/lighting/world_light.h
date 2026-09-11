#pragma once

#include <optional>
#include <unordered_map>

#include "lcu/core/types.h"
#include "lcu/lighting/light_storage.h"
#include "lcu/voxel/chunk_coord.h"

namespace lcu::lighting {

// Multi-chunk light storage (Phase 29): a plain per-chunk LightStorage
// only knows about its own chunk's [0, EdgeLength) cells - useful for
// meshing (Phase 28) but not for cross-chunk light propagation (Phase
// 30 sky / Phase 31 block: a BFS spreading light past a chunk boundary
// needs to look up light in a NEIGHBORING chunk given a local
// coordinate that has stepped outside [0, EdgeLength)). WorldLight is
// exactly that: an owning map of ChunkCoord -> LightStorage<EdgeLength>,
// plus boundary-aware sky_light_at/block_light_at queries that resolve
// an out-of-range local coordinate into its real owning chunk (reusing
// voxel::world_to_chunk_and_local's floor-division logic - the same one
// engine/world already uses for block edits - rather than every caller
// reimplementing that arithmetic itself).
//
// Does not itself propagate light across chunks - that's Phase 30/31's
// job. This type is deliberately just the data structure and query
// surface those phases build on, matching the brief's own phase split
// (WorldLight data structure first, cross-chunk sky/block propagation
// after).
template <u32 EdgeLength>
class WorldLight {
   public:
    using ChunkLight = LightStorage<EdgeLength>;

    // Get-or-create: mirrors client/main.cpp's existing Phase 6-era
    // std::unordered_map<ChunkCoord, Light>'s operator[] pattern (a
    // fresh all-zero LightStorage for a chunk the caller is about to
    // fill in via compute_block_light/compute_sky_light) - this type
    // replaces that ad hoc map with a real, reusable, testable one.
    ChunkLight& chunk_light(voxel::ChunkCoord coord) { return chunks_[coord]; }

    const ChunkLight* find_chunk_light(voxel::ChunkCoord coord) const {
        const auto it = chunks_.find(coord);
        return it != chunks_.end() ? &it->second : nullptr;
    }

    // Mutable counterpart, named like engine/world::World's own
    // chunk_at/chunk_at_mutable split - callers that need to write into
    // an already-loaded chunk's light (e.g. a single-block-edit update,
    // as opposed to chunk_light()'s get-or-create for a fresh chunk)
    // use this rather than a const_cast.
    ChunkLight* find_chunk_light_mutable(voxel::ChunkCoord coord) {
        const auto it = chunks_.find(coord);
        return it != chunks_.end() ? &it->second : nullptr;
    }

    bool has_chunk_light(voxel::ChunkCoord coord) const { return chunks_.find(coord) != chunks_.end(); }

    // Phase 35 territory ("unload marks neighbors dirty") will call
    // this when a chunk unloads - real now rather than bolted on later,
    // since it's plain map hygiene, not propagation logic.
    void remove_chunk_light(voxel::ChunkCoord coord) { chunks_.erase(coord); }

    usize loaded_chunk_count() const { return chunks_.size(); }

    // Resolves (coord, lx, ly, lz) into its real owning chunk - lx/ly/lz
    // may be outside [0, EdgeLength) in either direction (the arithmetic
    // doesn't assume "only ever one step past the boundary", even
    // though that's the only case any BFS built on this so far
    // produces). Returns std::nullopt if that owning chunk's light
    // hasn't been computed (not loaded, or loaded but never lit) -
    // never a guessed default; callers (the Phase 30/31 cross-chunk BFS)
    // decide what "unknown neighbor" means for their own algorithm, the
    // same honesty Phase 28's mesh_chunk_greedy already applies to its
    // own unresolvable boundary faces.
    std::optional<u8> sky_light_at(voxel::ChunkCoord coord, i32 lx, i32 ly, i32 lz) const {
        return sample(coord, lx, ly, lz,
                       [](const ChunkLight& light, u32 x, u32 y, u32 z) { return light.sky_light(x, y, z); });
    }

    std::optional<u8> block_light_at(voxel::ChunkCoord coord, i32 lx, i32 ly, i32 lz) const {
        return sample(coord, lx, ly, lz,
                       [](const ChunkLight& light, u32 x, u32 y, u32 z) { return light.block_light(x, y, z); });
    }

   private:
    template <typename Accessor>
    std::optional<u8> sample(voxel::ChunkCoord coord, i32 lx, i32 ly, i32 lz, Accessor accessor) const {
        const voxel::BlockWorldCoord world{
            static_cast<i64>(coord.x) * EdgeLength + lx,
            static_cast<i64>(coord.y) * EdgeLength + ly,
            static_cast<i64>(coord.z) * EdgeLength + lz,
        };
        const voxel::ChunkAndLocal resolved = voxel::world_to_chunk_and_local(world, EdgeLength);
        const ChunkLight* light = find_chunk_light(resolved.chunk);
        if (!light) {
            return std::nullopt;
        }
        return accessor(*light, resolved.local.x, resolved.local.y, resolved.local.z);
    }

    std::unordered_map<voxel::ChunkCoord, ChunkLight> chunks_;
};

// Default chunk size, matching light_storage.h's `Light` alias.
using DefaultWorldLight = WorldLight<16>;

}  // namespace lcu::lighting
