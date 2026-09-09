#include "lcu/world/worldgen.h"

#include <cstdlib>

#include <gtest/gtest.h>

using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::worldgen::generate_terrain_chunk;
using lcu::world::worldgen::terrain_height;

TEST(Worldgen, SameSeedAndCoordAlwaysProducesSameHeight) {
    const lcu::i32 h1 = terrain_height(42, 100, -50);
    const lcu::i32 h2 = terrain_height(42, 100, -50);
    const lcu::i32 h3 = terrain_height(42, 100, -50);

    EXPECT_EQ(h1, h2);
    EXPECT_EQ(h2, h3);
}

TEST(Worldgen, DifferentSeedsProduceDifferentTerrainSomewhere) {
    bool found_difference = false;
    for (lcu::i32 x = 0; x < 50 && !found_difference; ++x) {
        for (lcu::i32 z = 0; z < 50 && !found_difference; ++z) {
            if (terrain_height(1, x, z) != terrain_height(2, x, z)) {
                found_difference = true;
            }
        }
    }
    EXPECT_TRUE(found_difference) << "seeds 1 and 2 produced identical terrain over a 50x50 sample";
}

TEST(Worldgen, HeightStaysWithinAReasonableRange) {
    for (lcu::i32 x = -200; x <= 200; x += 37) {
        for (lcu::i32 z = -200; z <= 200; z += 41) {
            const lcu::i32 h = terrain_height(7, x, z);
            EXPECT_GE(h, -10) << "at (" << x << "," << z << ")";
            EXPECT_LE(h, 100) << "at (" << x << "," << z << ")";
        }
    }
}

TEST(Worldgen, AdjacentColumnsAreSmoothNotRandom) {
    // Neighboring world columns should differ gradually - if this noise
    // were literally per-cell random instead of a smoothed lattice, this
    // would fail constantly.
    for (lcu::i32 x = 0; x < 30; ++x) {
        const lcu::i32 h0 = terrain_height(3, x, 0);
        const lcu::i32 h1 = terrain_height(3, x + 1, 0);
        EXPECT_LE(std::abs(h1 - h0), 8) << "large jump between x=" << x << " and x=" << (x + 1);
    }
}

TEST(Worldgen, GenerateTerrainChunkMatchesTerrainHeightColumnByColumn) {
    constexpr lcu::voxel::BlockId kStone = 3;
    const ChunkCoord coord{0, 0, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/99, kStone);

    // Spot-check a handful of columns against terrain_height directly.
    for (lcu::u32 lx : {0u, 5u, 15u}) {
        for (lcu::u32 lz : {0u, 8u, 15u}) {
            const lcu::i32 height = terrain_height(99, static_cast<lcu::i32>(lx), static_cast<lcu::i32>(lz));
            for (lcu::u32 ly = 0; ly < Chunk::kEdgeLength; ++ly) {
                const bool expect_solid = static_cast<lcu::i32>(ly) <= height;
                const auto block = chunk.block_at(lx, ly, lz);
                if (expect_solid) {
                    EXPECT_EQ(block, kStone) << "(" << lx << "," << ly << "," << lz << ")";
                } else {
                    EXPECT_EQ(block, lcu::voxel::kAirBlockId) << "(" << lx << "," << ly << "," << lz << ")";
                }
            }
        }
    }
}

TEST(Worldgen, ChunkFarAboveTerrainIsEntirelyAir) {
    constexpr lcu::voxel::BlockId kStone = 3;
    // world_y range [1600, 1616) - far above any plausible terrain height.
    const ChunkCoord coord{0, 100, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/1, kStone);

    EXPECT_TRUE(chunk.is_empty());
}

TEST(Worldgen, ChunkFarBelowTerrainIsEntirelySolid) {
    constexpr lcu::voxel::BlockId kStone = 3;
    // world_y range [-1600, -1584) - far below any plausible terrain height.
    const ChunkCoord coord{0, -100, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/1, kStone);

    EXPECT_EQ(chunk.block_at(0, 0, 0), kStone);
    EXPECT_EQ(chunk.block_at(15, 15, 15), kStone);
    EXPECT_EQ(chunk.block_at(7, 3, 12), kStone);
    EXPECT_FALSE(chunk.is_empty());
}
