#pragma once

#include <cstddef>
#include <functional>

#include "lcu/core/types.h"

namespace lcu::voxel {

// Integer chunk-space coordinates. See ARCHITECTURE.md "Coordinate
// spaces": world positions are chunk coordinate + local offset, never a
// single unbounded float/double, to avoid precision loss in large worlds
// (brief section 23).
struct ChunkCoord {
    i32 x = 0;
    i32 y = 0;
    i32 z = 0;

    constexpr bool operator==(const ChunkCoord& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }
    constexpr bool operator!=(const ChunkCoord& rhs) const { return !(*this == rhs); }
};

// A block position in world (block-unit) space. i64, not i32: at even a
// modest chunk edge length, a full i32 world coordinate range would let
// `x * edge_length`-style math overflow well within plausible world
// sizes - see brief section 23 "avoid floating point / large world
// problems".
struct BlockWorldCoord {
    i64 x = 0;
    i64 y = 0;
    i64 z = 0;
};

// A block position local to its owning chunk, always in [0, edge_length).
struct LocalBlockCoord {
    u32 x = 0;
    u32 y = 0;
    u32 z = 0;
};

struct ChunkAndLocal {
    ChunkCoord chunk;
    LocalBlockCoord local;
};

// Splits a world block coordinate into its owning chunk coordinate and
// local offset, for the given chunk edge length. Uses floor division, not
// truncation, so negative world coordinates land in the correct chunk:
// e.g. with edge_length=16, world x=-1 is chunk -1, local 15 (not chunk 0
// with an out-of-range local, which truncating division would give).
ChunkAndLocal world_to_chunk_and_local(BlockWorldCoord world, u32 edge_length);

}  // namespace lcu::voxel

namespace std {

// Lets ChunkCoord be used directly as an unordered_map/unordered_set key
// (engine/world::World keys its chunk table by ChunkCoord). A simple
// mix of the three axes via odd multipliers - good enough distribution
// for chunk coordinates clustered near the origin/player, not a
// cryptographic hash.
template <>
struct hash<lcu::voxel::ChunkCoord> {
    std::size_t operator()(const lcu::voxel::ChunkCoord& coord) const noexcept {
        std::size_t h = static_cast<std::size_t>(static_cast<lcu::u32>(coord.x));
        h = h * 486187739u + static_cast<std::size_t>(static_cast<lcu::u32>(coord.y));
        h = h * 486187739u + static_cast<std::size_t>(static_cast<lcu::u32>(coord.z));
        return h;
    }
};

}  // namespace std
