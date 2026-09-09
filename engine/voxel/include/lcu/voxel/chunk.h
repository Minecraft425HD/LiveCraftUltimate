#pragma once

#include <array>

#include "lcu/core/assert.h"
#include "lcu/core/types.h"
#include "lcu/voxel/block_id.h"

namespace lcu::voxel {

// Chunk storage is templated on edge length so alternative chunk sizes
// stay possible (brief section 15) even though the default is 16^3. Data
// is one contiguous, trivially-copyable array of BlockId - no per-block
// C++ instance, no pointer chasing (brief section 13/15).
//
// This intentionally does NOT yet do palette compression or run-length
// encoding: a flat array is already cache-friendly and simple, and
// compressing it is a memory optimization with no correctness benefit -
// added later if profiling (Phase 11) actually shows chunk storage memory
// is a problem, not speculatively now (brief section 76/98).
template <u32 EdgeLength>
class ChunkStorage {
   public:
    static constexpr u32 kEdgeLength = EdgeLength;
    static constexpr u32 kVolume = EdgeLength * EdgeLength * EdgeLength;

    ChunkStorage() { blocks_.fill(kAirBlockId); }

    BlockId block_at(u32 x, u32 y, u32 z) const {
        LCU_ASSERT(in_bounds(x, y, z));
        return blocks_[index_of(x, y, z)];
    }

    void set_block(u32 x, u32 y, u32 z, BlockId id) {
        LCU_ASSERT(in_bounds(x, y, z));
        blocks_[index_of(x, y, z)] = id;
    }

    // O(volume) scan. Fine for now (called from meshing/streaming
    // decisions, not a hot per-block loop); if profiling ever shows this
    // matters, track an incrementally-maintained non-air count instead of
    // rescanning here (Phase 11 territory, not now).
    bool is_empty() const {
        for (BlockId id : blocks_) {
            if (id != kAirBlockId) {
                return false;
            }
        }
        return true;
    }

    static constexpr bool in_bounds(u32 x, u32 y, u32 z) {
        return x < EdgeLength && y < EdgeLength && z < EdgeLength;
    }

    // x-major-to-z layout: x varies fastest, then y, then z. Arbitrary but
    // fixed - meshing (Phase 2) and lighting (Phase 6) must agree with
    // this, so it's centralized here rather than reimplemented per
    // consumer.
    static constexpr usize index_of(u32 x, u32 y, u32 z) {
        return static_cast<usize>(x) + static_cast<usize>(y) * EdgeLength +
               static_cast<usize>(z) * EdgeLength * EdgeLength;
    }

   private:
    std::array<BlockId, kVolume> blocks_;
};

// Default chunk size per brief section 15.
using Chunk = ChunkStorage<16>;

}  // namespace lcu::voxel
