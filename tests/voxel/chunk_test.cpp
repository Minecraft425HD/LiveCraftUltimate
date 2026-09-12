#include "lcu/voxel/chunk.h"

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

using lcu::voxel::Chunk;
using lcu::voxel::ChunkStorage;
using lcu::voxel::kAirBlockId;

TEST(Chunk, DefaultSizeIs16Cubed) {
    EXPECT_EQ(Chunk::kEdgeLength, 16u);
    EXPECT_EQ(Chunk::kVolume, 16u * 16u * 16u);
}

TEST(Chunk, DefaultsToAllAir) {
    Chunk chunk;
    EXPECT_TRUE(chunk.is_empty());
    EXPECT_EQ(chunk.block_at(0, 0, 0), kAirBlockId);
    EXPECT_EQ(chunk.block_at(15, 15, 15), kAirBlockId);
}

TEST(Chunk, SetAndGetBlock) {
    Chunk chunk;
    chunk.set_block(3, 4, 5, 42);

    EXPECT_EQ(chunk.block_at(3, 4, 5), 42);
    EXPECT_FALSE(chunk.is_empty());

    // Neighbors are untouched.
    EXPECT_EQ(chunk.block_at(3, 4, 4), kAirBlockId);
    EXPECT_EQ(chunk.block_at(2, 4, 5), kAirBlockId);
}

TEST(Chunk, ClearingLastBlockMakesItEmptyAgain) {
    Chunk chunk;
    chunk.set_block(0, 0, 0, 7);
    ASSERT_FALSE(chunk.is_empty());

    chunk.set_block(0, 0, 0, kAirBlockId);
    EXPECT_TRUE(chunk.is_empty());
}

TEST(Chunk, IndexOfIsInjectiveOverFullRange) {
    // Every (x, y, z) in-bounds must map to a distinct index in
    // [0, kVolume) - meshing/lighting correctness depends on this.
    std::vector<bool> seen(Chunk::kVolume, false);
    for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
                const lcu::usize index = Chunk::index_of(x, y, z);
                ASSERT_LT(index, Chunk::kVolume);
                ASSERT_FALSE(seen[index]) << "duplicate index at (" << x << "," << y << "," << z << ")";
                seen[index] = true;
            }
        }
    }
    EXPECT_TRUE(std::all_of(seen.begin(), seen.end(), [](bool v) { return v; }));
}

TEST(Chunk, InBoundsRejectsOutOfRangeCoordinates) {
    EXPECT_TRUE(Chunk::in_bounds(0, 0, 0));
    EXPECT_TRUE(Chunk::in_bounds(15, 15, 15));
    EXPECT_FALSE(Chunk::in_bounds(16, 0, 0));
    EXPECT_FALSE(Chunk::in_bounds(0, 16, 0));
    EXPECT_FALSE(Chunk::in_bounds(0, 0, 16));
}

TEST(Chunk, OutOfBoundsAccessAsserts) {
    Chunk chunk;
    EXPECT_DEATH(chunk.block_at(16, 0, 0), "");
    EXPECT_DEATH(chunk.set_block(0, 0, 16, 1), "");
}

TEST(ChunkStorage, SupportsAlternativeEdgeLength) {
    // Brief section 15: architecture must allow alternative chunk sizes.
    ChunkStorage<8> small_chunk;
    EXPECT_EQ(ChunkStorage<8>::kVolume, 8u * 8u * 8u);
    small_chunk.set_block(7, 7, 7, 1);
    EXPECT_EQ(small_chunk.block_at(7, 7, 7), 1);
    EXPECT_FALSE(ChunkStorage<8>::in_bounds(8, 0, 0));
}

TEST(ChunkStorage, DefaultsToStateZeroEverywhere) {
    Chunk chunk;
    EXPECT_EQ(chunk.state_at(0, 0, 0), 0);
    EXPECT_EQ(chunk.state_at(15, 15, 15), 0);
}

TEST(ChunkStorage, SetBlockWithStateSetsBothRealFields) {
    Chunk chunk;
    chunk.set_block_with_state(3, 4, 5, 42, 7);
    EXPECT_EQ(chunk.block_at(3, 4, 5), 42);
    EXPECT_EQ(chunk.state_at(3, 4, 5), 7);
}

TEST(ChunkStorage, PlainSetBlockResetsStateToZero) {
    // A fresh block placement has no real state history - set_block
    // (the pre-Phase-63 API every existing caller still uses) must not
    // leave a stale state behind from whatever used to occupy this
    // voxel.
    Chunk chunk;
    chunk.set_block_with_state(1, 1, 1, 5, 6);
    ASSERT_EQ(chunk.state_at(1, 1, 1), 6);

    chunk.set_block(1, 1, 1, 9);
    EXPECT_EQ(chunk.block_at(1, 1, 1), 9);
    EXPECT_EQ(chunk.state_at(1, 1, 1), 0);
}

TEST(ChunkStorage, SetStateAloneLeavesTheBlockIdUntouched) {
    Chunk chunk;
    chunk.set_block(2, 2, 2, 11);
    chunk.set_state(2, 2, 2, 3);
    EXPECT_EQ(chunk.block_at(2, 2, 2), 11);
    EXPECT_EQ(chunk.state_at(2, 2, 2), 3);
}

TEST(ChunkStorage, StatesAndSetStatesRoundTripTheWholeArray) {
    Chunk chunk;
    chunk.set_block_with_state(0, 0, 0, 1, 200);
    chunk.set_block_with_state(15, 15, 15, 2, 100);

    Chunk other;
    other.set_states(chunk.states());
    EXPECT_EQ(other.state_at(0, 0, 0), 200);
    EXPECT_EQ(other.state_at(15, 15, 15), 100);
    // set_states never touches block ids.
    EXPECT_EQ(other.block_at(0, 0, 0), lcu::voxel::kAirBlockId);
}

TEST(ChunkStorage, OutOfBoundsStateAccessAsserts) {
    Chunk chunk;
    EXPECT_DEATH(chunk.state_at(16, 0, 0), "");
    EXPECT_DEATH(chunk.set_block_with_state(0, 0, 16, 1, 1), "");
    EXPECT_DEATH(chunk.set_state(0, 16, 0, 1), "");
}
