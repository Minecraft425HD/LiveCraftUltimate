#pragma once

#include <algorithm>
#include <array>

#include "lcu/core/assert.h"
#include "lcu/core/types.h"

namespace lcu::lighting {

// Packed per-voxel light levels (brief section 24): 4 bits sky light +
// 4 bits block light per voxel, each in [0, kMaxLightLevel]. One byte
// per voxel, flat contiguous array - the same data-oriented layout as
// lcu::voxel::ChunkStorage (no per-voxel C++ instance), templated on
// edge length so it always matches whatever chunk size it lights.
template <u32 EdgeLength>
class LightStorage {
   public:
    static constexpr u32 kEdgeLength = EdgeLength;
    static constexpr u32 kVolume = EdgeLength * EdgeLength * EdgeLength;
    static constexpr u8 kMaxLightLevel = 15;

    LightStorage() { levels_.fill(0); }

    u8 sky_light(u32 x, u32 y, u32 z) const { return static_cast<u8>(packed_at(x, y, z) & 0x0Fu); }
    u8 block_light(u32 x, u32 y, u32 z) const { return static_cast<u8>(packed_at(x, y, z) >> 4); }

    void set_sky_light(u32 x, u32 y, u32 z, u8 level) {
        LCU_ASSERT(level <= kMaxLightLevel);
        u8& cell = packed_at_mutable(x, y, z);
        cell = static_cast<u8>((cell & 0xF0u) | level);
    }

    void set_block_light(u32 x, u32 y, u32 z, u8 level) {
        LCU_ASSERT(level <= kMaxLightLevel);
        u8& cell = packed_at_mutable(x, y, z);
        cell = static_cast<u8>((cell & 0x0Fu) | static_cast<u8>(level << 4));
    }

    // The level a renderer would actually shade by - the brighter of the
    // two channels, matching how sky and block light combine in every
    // engine that separates them (a lit torch under a roof is still lit;
    // open sky is still bright even with no torches nearby).
    u8 combined_light(u32 x, u32 y, u32 z) const {
        return std::max(sky_light(x, y, z), block_light(x, y, z));
    }

    static constexpr bool in_bounds(u32 x, u32 y, u32 z) {
        return x < EdgeLength && y < EdgeLength && z < EdgeLength;
    }

    // Matches lcu::voxel::ChunkStorage::index_of exactly (x-major-to-z) -
    // lighting code cross-references block data at the same coordinates,
    // so the two layouts are kept in lockstep deliberately, not just by
    // coincidence.
    static constexpr usize index_of(u32 x, u32 y, u32 z) {
        return static_cast<usize>(x) + static_cast<usize>(y) * EdgeLength +
               static_cast<usize>(z) * EdgeLength * EdgeLength;
    }

   private:
    u8 packed_at(u32 x, u32 y, u32 z) const {
        LCU_ASSERT(in_bounds(x, y, z));
        return levels_[index_of(x, y, z)];
    }
    u8& packed_at_mutable(u32 x, u32 y, u32 z) {
        LCU_ASSERT(in_bounds(x, y, z));
        return levels_[index_of(x, y, z)];
    }

    std::array<u8, kVolume> levels_;
};

// Default chunk size per brief section 15/lcu::voxel::Chunk.
using Light = LightStorage<16>;

}  // namespace lcu::lighting
