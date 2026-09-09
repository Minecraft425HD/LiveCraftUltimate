#include "lcu/voxel/chunk_coord.h"

#include <gtest/gtest.h>

using lcu::voxel::BlockWorldCoord;
using lcu::voxel::ChunkCoord;
using lcu::voxel::LocalBlockCoord;
using lcu::voxel::world_to_chunk_and_local;

namespace {

constexpr lcu::u32 kEdge = 16;

}  // namespace

TEST(ChunkCoord, OriginIsChunkZeroLocalZero) {
    const auto result = world_to_chunk_and_local({0, 0, 0}, kEdge);
    EXPECT_EQ(result.chunk, (ChunkCoord{0, 0, 0}));
    EXPECT_EQ(result.local.x, 0u);
    EXPECT_EQ(result.local.y, 0u);
    EXPECT_EQ(result.local.z, 0u);
}

TEST(ChunkCoord, WithinFirstPositiveChunk) {
    const auto result = world_to_chunk_and_local({5, 10, 15}, kEdge);
    EXPECT_EQ(result.chunk, (ChunkCoord{0, 0, 0}));
    EXPECT_EQ(result.local.x, 5u);
    EXPECT_EQ(result.local.y, 10u);
    EXPECT_EQ(result.local.z, 15u);
}

TEST(ChunkCoord, ExactlyOnChunkBoundaryRollsOverToNextChunk) {
    const auto result = world_to_chunk_and_local({16, 32, 48}, kEdge);
    EXPECT_EQ(result.chunk, (ChunkCoord{1, 2, 3}));
    EXPECT_EQ(result.local.x, 0u);
    EXPECT_EQ(result.local.y, 0u);
    EXPECT_EQ(result.local.z, 0u);
}

TEST(ChunkCoord, NegativeOneIsChunkNegativeOneLocalFifteen) {
    // The critical floor-division case: truncating division would give
    // chunk 0 with an out-of-range local coordinate.
    const auto result = world_to_chunk_and_local({-1, -1, -1}, kEdge);
    EXPECT_EQ(result.chunk, (ChunkCoord{-1, -1, -1}));
    EXPECT_EQ(result.local.x, 15u);
    EXPECT_EQ(result.local.y, 15u);
    EXPECT_EQ(result.local.z, 15u);
}

TEST(ChunkCoord, NegativeSixteenIsChunkNegativeOneLocalZero) {
    const auto result = world_to_chunk_and_local({-16, -16, -16}, kEdge);
    EXPECT_EQ(result.chunk, (ChunkCoord{-1, -1, -1}));
    EXPECT_EQ(result.local.x, 0u);
    EXPECT_EQ(result.local.y, 0u);
    EXPECT_EQ(result.local.z, 0u);
}

TEST(ChunkCoord, NegativeSeventeenIsChunkNegativeTwoLocalFifteen) {
    const auto result = world_to_chunk_and_local({-17, -17, -17}, kEdge);
    EXPECT_EQ(result.chunk, (ChunkCoord{-2, -2, -2}));
    EXPECT_EQ(result.local.x, 15u);
    EXPECT_EQ(result.local.y, 15u);
    EXPECT_EQ(result.local.z, 15u);
}

TEST(ChunkCoord, MixedSignAxesAreIndependent) {
    const auto result = world_to_chunk_and_local({-5, 20, -20}, kEdge);
    EXPECT_EQ(result.chunk, (ChunkCoord{-1, 1, -2}));
    EXPECT_EQ(result.local.x, 11u);
    EXPECT_EQ(result.local.y, 4u);
    EXPECT_EQ(result.local.z, 12u);
}

TEST(ChunkCoord, RoundTripIsConsistentAcrossManyValues) {
    for (lcu::i64 x = -50; x <= 50; x += 7) {
        for (lcu::i64 y = -50; y <= 50; y += 11) {
            const auto result = world_to_chunk_and_local({x, y, 0}, kEdge);
            const lcu::i64 reconstructed_x =
                static_cast<lcu::i64>(result.chunk.x) * kEdge + result.local.x;
            const lcu::i64 reconstructed_y =
                static_cast<lcu::i64>(result.chunk.y) * kEdge + result.local.y;
            EXPECT_EQ(reconstructed_x, x);
            EXPECT_EQ(reconstructed_y, y);
            EXPECT_LT(result.local.x, kEdge);
            EXPECT_LT(result.local.y, kEdge);
        }
    }
}
