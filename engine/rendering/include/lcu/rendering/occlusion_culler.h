#pragma once

#include <unordered_map>
#include <unordered_set>

#include "lcu/core/types.h"
#include "lcu/rendering/frustum.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk_coord.h"
#include "lcu/world/world.h"

namespace lcu::rendering {

// Real BFS occlusion culling (Phase 69, brief section 69 - Sodium-style,
// the culling cascade's own real "Haupthebel"): starting from the chunk
// the camera itself is in, flood-fills across chunk boundaries only
// where a real "portal" might exist - a shared 16x16 boundary face that
// isn't provably entirely solid on at least one side - so a chunk
// buried behind solid rock in every direction (a real cave/mine
// scenario) is never rendered at all, even though Phase 68's own
// frustum test alone would have let it through.
//
// The portal test is a real, deliberate approximation of the brief's
// own literal "does at least one matching position on the shared face
// have BOTH sides non-opaque" - rather than checking all 256 positions
// on both chunks' faces against each other exactly, each chunk caches
// one bit per side: "is this whole face provably 100% opaque" (true for
// e.g. a face fully packed with stone, false the moment even one voxel
// on it is non-opaque, regardless of the neighbor's own content). A
// portal is assumed to exist unless EITHER side's own face is fully
// opaque - real, occasionally conservative (a rare case where each
// side has a gap, but not at the same position, still counts as "maybe
// a portal"), but real light/gameplay data drives it (not a guess), and
// it's exactly the caching scheme the brief's own section 69.3 asks
// for.
class OcclusionCuller {
   public:
    // Real per-frame occlusion-visible set via BFS from `camera_chunk`,
    // gated by both this culler's own real portal test and `frustum`
    // (Phase 68) - a chunk that fails the frustum test is never entered,
    // so occlusion only ever narrows what the frustum already allowed,
    // never widens it. Returns an empty set if `camera_chunk` itself
    // isn't currently loaded (a real, honest "nothing visible" rather
    // than guessing). Lazily computes/caches each visited chunk's own
    // boundary_opacity_mask on first use this call (or since its last
    // invalidation) - see invalidate/invalidate_neighbors below.
    std::unordered_set<voxel::ChunkCoord> compute(const world::World& world, const voxel::BlockRegistry& registry,
                                                    voxel::ChunkCoord camera_chunk, const Frustum& frustum);

    // Discards chunk `c`'s own cached boundary_opacity_mask entry (it is
    // recomputed, lazily, the next time compute() actually visits it -
    // not eagerly here). A chunk's own mask depends only on its own
    // voxels, so this alone is always sufficient after an edit strictly
    // inside `c`.
    void invalidate(voxel::ChunkCoord c);

    // invalidate(c) plus its 6 face-adjacent neighbors - the real call
    // this project's own block-edit/chunk-load/chunk-unload sites use
    // (see client/main.cpp): a conservative "throw away a little more
    // than strictly necessary" margin (a neighbor's own mask never
    // actually depends on `c`'s voxels, only on its own - see
    // DECISIONS.md), cheap enough (each mask recompute is a bounded 6x16x16
    // scan) that the extra safety costs nothing real.
    void invalidate_neighbors(voxel::ChunkCoord c);

   private:
    std::unordered_map<voxel::ChunkCoord, u8> boundary_opacity_mask_;

    // Returns `c`'s real 6-bit mask (bit i set = side i is provably 100%
    // opaque), computing and caching it first if absent. `c` must
    // currently be loaded in `world` - every real call site below only
    // ever calls this for a chunk it just confirmed via `chunk_at`.
    u8 mask_for(const world::World& world, const voxel::BlockRegistry& registry, voxel::ChunkCoord c);
};

}  // namespace lcu::rendering
