#include "lcu/world/worldgen.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <vector>

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
    // Phase 38: continental-amplitude-modulated terrain - the
    // analytical worst case (deepest ocean base minus its own amplitude,
    // or highest highland base plus its own amplitude, see worldgen.cpp's
    // kDeepOceanBase/kHighlandBase/kMin-/kMaxMountainAmplitude) is wider
    // than what the smoothed multi-octave noise actually reaches in
    // practice (both stages would have to hit their own extreme
    // simultaneously) - a real 20-seed, wide-area sweep measured
    // [-15, 20], so these bounds keep real headroom without being loose
    // enough to miss an actual regression.
    for (lcu::i32 x = -200; x <= 200; x += 37) {
        for (lcu::i32 z = -200; z <= 200; z += 41) {
            const lcu::i32 h = terrain_height(7, x, z);
            EXPECT_GE(h, -20) << "at (" << x << "," << z << ")";
            EXPECT_LE(h, 30) << "at (" << x << "," << z << ")";
        }
    }
}

TEST(Worldgen, LocalRoughnessVariesAcrossRegions) {
    // Phase 38's real, observable new behavior: terrain is no longer
    // uniformly bumpy everywhere the way Phase 37's single fixed-
    // amplitude noise was - a low-continentalness (coastal/oceanic)
    // region should be comparatively flat, a high-continentalness
    // (highland) region comparatively rugged. Real proxy, without
    // needing to reach into the anonymous-namespace continental noise
    // directly: sample many widely-spaced local neighborhoods' own
    // height range (max-min within an 8x8 grid of nearby columns) and
    // confirm that range itself varies meaningfully from window to
    // window - under the old uniform-amplitude model this would stay
    // roughly constant (only small per-sample noise), never swinging
    // between a genuinely flat window and a genuinely rugged one.
    constexpr lcu::u32 kSeed = 99;
    std::vector<lcu::i32> local_ranges;
    for (lcu::i32 cx = -15; cx <= 15; ++cx) {
        for (lcu::i32 cz = -15; cz <= 15; ++cz) {
            const lcu::i32 base_x = cx * 40;
            const lcu::i32 base_z = cz * 40;
            lcu::i32 local_min = std::numeric_limits<lcu::i32>::max();
            lcu::i32 local_max = std::numeric_limits<lcu::i32>::min();
            for (lcu::i32 dx = 0; dx < 8; ++dx) {
                for (lcu::i32 dz = 0; dz < 8; ++dz) {
                    const lcu::i32 h = terrain_height(kSeed, base_x + dx * 4, base_z + dz * 4);
                    local_min = std::min(local_min, h);
                    local_max = std::max(local_max, h);
                }
            }
            local_ranges.push_back(local_max - local_min);
        }
    }
    const lcu::i32 flattest = *std::min_element(local_ranges.begin(), local_ranges.end());
    const lcu::i32 roughest = *std::max_element(local_ranges.begin(), local_ranges.end());
    EXPECT_GT(roughest - flattest, 8) << "flattest local window range=" << flattest
                                       << ", roughest=" << roughest
                                       << " - local terrain roughness should vary meaningfully across regions";
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
    constexpr lcu::voxel::BlockId kGrass = 2;
    constexpr lcu::voxel::BlockId kDirt = 4;
    constexpr lcu::voxel::BlockId kStone = 3;
    constexpr lcu::voxel::BlockId kWater = 6;
    constexpr lcu::i32 kSubsurfaceDepth = 3;  // matches worldgen.cpp's own kSubsurfaceDepth.
    const ChunkCoord coord{0, 0, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/99, kGrass, kDirt, kStone, kWater);

    // Spot-check a handful of columns against terrain_height directly -
    // grass at the surface, dirt for kSubsurfaceDepth layers beneath it,
    // stone deeper, water (Phase 37) between the surface and sea level
    // for a below-sea-level column, air above sea level.
    for (lcu::u32 lx : {0u, 5u, 15u}) {
        for (lcu::u32 lz : {0u, 8u, 15u}) {
            const lcu::i32 height = terrain_height(99, static_cast<lcu::i32>(lx), static_cast<lcu::i32>(lz));
            for (lcu::u32 ly = 0; ly < Chunk::kEdgeLength; ++ly) {
                const lcu::i32 world_y = static_cast<lcu::i32>(ly);
                const auto block = chunk.block_at(lx, ly, lz);
                if (world_y > height) {
                    if (world_y <= lcu::world::worldgen::kSeaLevel) {
                        EXPECT_EQ(block, kWater) << "(" << lx << "," << ly << "," << lz << ")";
                    } else {
                        EXPECT_EQ(block, lcu::voxel::kAirBlockId) << "(" << lx << "," << ly << "," << lz << ")";
                    }
                } else if (world_y == height) {
                    EXPECT_EQ(block, kGrass) << "(" << lx << "," << ly << "," << lz << ")";
                } else if (world_y > height - kSubsurfaceDepth) {
                    EXPECT_EQ(block, kDirt) << "(" << lx << "," << ly << "," << lz << ")";
                } else {
                    EXPECT_EQ(block, kStone) << "(" << lx << "," << ly << "," << lz << ")";
                }
            }
        }
    }
}

TEST(Worldgen, ChunkFarAboveTerrainIsEntirelyAir) {
    constexpr lcu::voxel::BlockId kGrass = 2;
    constexpr lcu::voxel::BlockId kDirt = 4;
    constexpr lcu::voxel::BlockId kStone = 3;
    constexpr lcu::voxel::BlockId kWater = 6;
    // world_y range [1600, 1616) - far above any plausible terrain
    // height AND far above sea level, so no water fill applies either.
    const ChunkCoord coord{0, 100, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/1, kGrass, kDirt, kStone, kWater);

    EXPECT_TRUE(chunk.is_empty());
}

TEST(Worldgen, ChunkFarBelowTerrainIsEntirelyStone) {
    constexpr lcu::voxel::BlockId kGrass = 2;
    constexpr lcu::voxel::BlockId kDirt = 4;
    constexpr lcu::voxel::BlockId kStone = 3;
    constexpr lcu::voxel::BlockId kWater = 6;
    // world_y range [-1600, -1584) - far below any plausible terrain
    // height (and far below the surface/subsurface layers near it), so
    // every block should be stone, never grass, dirt, or water.
    const ChunkCoord coord{0, -100, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/1, kGrass, kDirt, kStone, kWater);

    EXPECT_EQ(chunk.block_at(0, 0, 0), kStone);
    EXPECT_EQ(chunk.block_at(15, 15, 15), kStone);
    EXPECT_EQ(chunk.block_at(7, 3, 12), kStone);
    EXPECT_FALSE(chunk.is_empty());
}

TEST(Worldgen, SurfaceLayerIsExactlyOneBlockThickAtTheHeight) {
    constexpr lcu::voxel::BlockId kGrass = 2;
    constexpr lcu::voxel::BlockId kDirt = 4;
    constexpr lcu::voxel::BlockId kStone = 3;
    constexpr lcu::voxel::BlockId kWater = 6;
    constexpr lcu::u32 kSeed = 5;

    // Locate the actual chunk containing world (0, height, 0)'s surface
    // - the terrain height isn't guaranteed to land inside chunk
    // coordinate 0 (Phase 37: terrain_height is centered on sea level,
    // so it can even be negative), so generate whichever chunk really
    // contains it instead of assuming a hardcoded coordinate does.
    const lcu::i32 height = terrain_height(kSeed, 0, 0);
    const auto split =
        lcu::voxel::world_to_chunk_and_local({0, height, 0}, Chunk::kEdgeLength);
    const auto split_below =
        lcu::voxel::world_to_chunk_and_local({0, height - 1, 0}, Chunk::kEdgeLength);
    ASSERT_EQ(split.chunk, split_below.chunk) << "height and height-1 landed in different chunks - pick a "
                                                  "different seed/column, or generate both chunks";

    Chunk chunk;
    generate_terrain_chunk(chunk, split.chunk, kSeed, kGrass, kDirt, kStone, kWater);

    EXPECT_EQ(chunk.block_at(split.local.x, split.local.y, split.local.z), kGrass);
    EXPECT_EQ(chunk.block_at(split_below.local.x, split_below.local.y, split_below.local.z), kDirt)
        << "block directly beneath the surface should be dirt, not grass or stone";
}

TEST(Worldgen, BelowSeaLevelColumnIsFilledWithWaterUpToSeaLevel) {
    // Phase 37's real new behavior: find a column whose terrain height
    // lands below sea level (a real "ocean" column, not a hypothetical
    // one) and confirm water actually fills the gap up to (and
    // including) sea level, with air strictly above it.
    constexpr lcu::voxel::BlockId kGrass = 2;
    constexpr lcu::voxel::BlockId kDirt = 4;
    constexpr lcu::voxel::BlockId kStone = 3;
    constexpr lcu::voxel::BlockId kWater = 6;
    constexpr lcu::u32 kSeed = 1;

    lcu::i32 found_x = 0;
    lcu::i32 found_z = 0;
    bool found = false;
    for (lcu::i32 x = 0; x < 200 && !found; ++x) {
        for (lcu::i32 z = 0; z < 200 && !found; ++z) {
            if (terrain_height(kSeed, x, z) < lcu::world::worldgen::kSeaLevel) {
                found_x = x;
                found_z = z;
                found = true;
            }
        }
    }
    ASSERT_TRUE(found) << "no below-sea-level column found in a 200x200 sample - worldgen's height range may have "
                           "changed";

    const lcu::i32 height = terrain_height(kSeed, found_x, found_z);

    // Sea level (world_y=0) always lands in chunk_y=0, but the water
    // range can start in chunk_y=-1 too (any negative world_y does) -
    // generate every chunk the [height+1, kSeaLevel] range actually
    // touches, keyed by chunk_y, rather than assuming it's just one.
    std::unordered_map<lcu::i32, Chunk> chunks_by_y;
    const auto chunk_for = [&](lcu::i32 world_y) -> Chunk& {
        const auto split = lcu::voxel::world_to_chunk_and_local({found_x, world_y, found_z}, Chunk::kEdgeLength);
        auto it = chunks_by_y.find(split.chunk.y);
        if (it == chunks_by_y.end()) {
            Chunk chunk;
            generate_terrain_chunk(chunk, split.chunk, kSeed, kGrass, kDirt, kStone, kWater);
            it = chunks_by_y.emplace(split.chunk.y, std::move(chunk)).first;
        }
        return it->second;
    };

    for (lcu::i32 world_y = height + 1; world_y <= lcu::world::worldgen::kSeaLevel; ++world_y) {
        const auto local = lcu::voxel::world_to_chunk_and_local({found_x, world_y, found_z}, Chunk::kEdgeLength);
        Chunk& chunk = chunk_for(world_y);
        EXPECT_EQ(chunk.block_at(local.local.x, local.local.y, local.local.z), kWater) << "world_y=" << world_y;
    }
    {
        const auto local = lcu::voxel::world_to_chunk_and_local(
            {found_x, lcu::world::worldgen::kSeaLevel + 1, found_z}, Chunk::kEdgeLength);
        Chunk& chunk = chunk_for(lcu::world::worldgen::kSeaLevel + 1);
        EXPECT_EQ(chunk.block_at(local.local.x, local.local.y, local.local.z), lcu::voxel::kAirBlockId)
            << "strictly above sea level should be air, not water";
    }
}
