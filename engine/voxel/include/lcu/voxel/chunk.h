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

    ChunkStorage() {
        blocks_.fill(kAirBlockId);
        states_.fill(0);
    }

    BlockId block_at(u32 x, u32 y, u32 z) const {
        LCU_ASSERT(in_bounds(x, y, z));
        return blocks_[index_of(x, y, z)];
    }

    // Sets a block's real type, implicitly resetting its state to 0 -
    // matches every pre-Phase-63 caller's own real expectation (placing
    // a fresh block has no state history), so no existing call site
    // needs to change. Real, explicit state control lives in
    // set_block_with_state below.
    void set_block(u32 x, u32 y, u32 z, BlockId id) {
        LCU_ASSERT(in_bounds(x, y, z));
        const usize index = index_of(x, y, z);
        blocks_[index] = id;
        states_[index] = 0;
    }

    // Real block-state system (Phase 63, brief section 63's own farming
    // foundation: growth stages, farmland-vs-dirt, etc. all need a real
    // per-block sub-value beyond just "which BlockId"). A second,
    // parallel `std::array<u8, kVolume>` (~4KB/chunk for the default
    // 16^3 edge length - a real, accepted memory cost, not optimized
    // away, see DECISIONS.md) rather than widening BlockId itself or
    // packing state into spare BlockId bits: every existing BlockId
    // value (serialized chunks, network packets, mod-registered ids)
    // stays meaningful unchanged, and `state_at`'s own real default (0)
    // for anything only ever touched via the plain `set_block`/`block_
    // at` pair above means every pre-Phase-63 caller needs zero changes
    // to keep behaving exactly as it did.
    u8 state_at(u32 x, u32 y, u32 z) const {
        LCU_ASSERT(in_bounds(x, y, z));
        return states_[index_of(x, y, z)];
    }

    void set_block_with_state(u32 x, u32 y, u32 z, BlockId id, u8 state) {
        LCU_ASSERT(in_bounds(x, y, z));
        const usize index = index_of(x, y, z);
        blocks_[index] = id;
        states_[index] = state;
    }

    // Real, standalone state mutation - a real farming-growth tick
    // (Phase 64) advances a wheat block's own state without touching
    // its BlockId at all, and shouldn't need to re-read+rewrite the
    // BlockId it already knows just to change the state alongside it.
    void set_state(u32 x, u32 y, u32 z, u8 state) {
        LCU_ASSERT(in_bounds(x, y, z));
        states_[index_of(x, y, z)] = state;
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

    // Real, direct read-only access to the backing state array - used
    // by ChunkSerializer/network ChunkData (Phase 63) to serialize the
    // whole array at once rather than looping x/y/z through state_at.
    const std::array<u8, kVolume>& states() const { return states_; }

    // Real bulk state load (ChunkSerializer/network ChunkData, Phase
    // 63) - replaces every state at once (e.g. from a deserialized
    // buffer), leaving blocks_ untouched. LCU_ASSERTs on a mismatched
    // size at the call site instead of here (a std::array reference
    // parameter already can't be the wrong size).
    void set_states(const std::array<u8, kVolume>& states) { states_ = states; }

   private:
    std::array<BlockId, kVolume> blocks_;
    std::array<u8, kVolume> states_;
};

// Default chunk size per brief section 15.
using Chunk = ChunkStorage<16>;

}  // namespace lcu::voxel
