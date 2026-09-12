#include "lcu/world/worldgen.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::worldgen::biome_at;
using lcu::world::worldgen::Biome;
using lcu::world::worldgen::BiomeBlocks;
using lcu::world::worldgen::generate_terrain_chunk;
using lcu::world::worldgen::is_cave;
using lcu::world::worldgen::ore_at;
using lcu::world::worldgen::OreBlocks;
using lcu::world::worldgen::OreType;
using lcu::world::worldgen::terrain_height;
using lcu::world::worldgen::vegetation_at;
using lcu::world::worldgen::VegetationBlocks;
using lcu::world::worldgen::VegetationType;

namespace {

constexpr lcu::voxel::BlockId kGrass = 2;
constexpr lcu::voxel::BlockId kDirt = 4;
constexpr lcu::voxel::BlockId kStone = 3;
constexpr lcu::voxel::BlockId kWater = 6;
constexpr lcu::voxel::BlockId kSand = 7;
constexpr lcu::voxel::BlockId kSnow = 8;
constexpr lcu::voxel::BlockId kCoalOre = 9;
constexpr lcu::voxel::BlockId kIronOre = 10;
constexpr lcu::voxel::BlockId kWood = 11;
constexpr lcu::voxel::BlockId kLeaves = 12;
constexpr lcu::voxel::BlockId kCactus = 13;

// Matches worldgen.cpp's own kTreeTrunkHeight/kTreeCanopyHeight/
// kCactusHeight - same "duplicate the private constant, comment where
// it comes from" pattern kSubsurfaceDepth already used below, since
// these aren't (and don't need to be) exposed in worldgen.h.
constexpr lcu::i32 kTreeTrunkHeight = 4;
constexpr lcu::i32 kTreeCanopyHeight = 3;
constexpr lcu::i32 kCactusHeight = 3;

// Distinct ids per biome (unlike production code, which reuses kSand
// for both Desert's surface and subsurface) so a test can tell exactly
// which biome/layer produced a given block from its id alone.
const BiomeBlocks kTestBiomeBlocks{
    /*plains_surface=*/kGrass, /*plains_subsurface=*/kDirt,
    /*desert_surface=*/kSand,  /*desert_subsurface=*/kSand,
    /*snowy_surface=*/kSnow,   /*snowy_subsurface=*/kDirt,
};

// Distinct ids per ore (Phase 40), same "distinct per category" reasoning
// as kTestBiomeBlocks above.
const OreBlocks kTestOreBlocks{
    /*coal_ore=*/kCoalOre,
    /*iron_ore=*/kIronOre,
};

// Distinct ids per vegetation block (Phase 41), same "distinct per
// category" reasoning as kTestBiomeBlocks/kTestOreBlocks above.
const VegetationBlocks kTestVegetationBlocks{
    /*wood=*/kWood,
    /*leaves=*/kLeaves,
    /*cactus=*/kCactus,
};

// Mirrors generate_terrain_chunk's own stone-band logic exactly (worldgen.cpp:
// is_cave carved -> air, else ore_at -> that ore's block, else stone) via
// the same public is_cave/ore_at functions worldgen.cpp itself calls, so a
// test can compute the expected block for any cell at or below a column's
// subsurface layer without duplicating worldgen.cpp's private internals.
lcu::voxel::BlockId expected_stone_band_block(lcu::u32 seed, lcu::i32 world_x, lcu::i32 world_y, lcu::i32 world_z,
                                               lcu::i32 surface_height) {
    if (is_cave(seed, world_x, world_y, world_z, surface_height)) {
        return lcu::voxel::kAirBlockId;
    }
    switch (ore_at(seed, world_x, world_y, world_z)) {
        case OreType::Coal:
            return kCoalOre;
        case OreType::Iron:
            return kIronOre;
        case OreType::None:
            return kStone;
    }
    return kStone;
}

// Mirrors generate_terrain_chunk's own above-terrain logic exactly
// (worldgen.cpp: water at/below sea level; above sea level, a dry
// column's own vegetation_at result places a Tree's trunk/canopy or a
// Cactus' stack; anything else stays air) via the same public
// vegetation_at function worldgen.cpp itself calls, so a test can
// compute the expected block for any above-terrain cell without
// duplicating worldgen.cpp's private internals (beyond the trunk/
// canopy/cactus heights themselves - see kTreeTrunkHeight's own
// comment above for why those are duplicated, not exposed).
lcu::voxel::BlockId expected_above_terrain_block(lcu::u32 seed, lcu::i32 world_x, lcu::i32 world_y,
                                                  lcu::i32 world_z, lcu::i32 height, Biome biome) {
    if (world_y <= lcu::world::worldgen::kSeaLevel) {
        return kWater;
    }
    if (height > lcu::world::worldgen::kSeaLevel) {
        switch (vegetation_at(seed, world_x, world_z, biome)) {
            case VegetationType::Tree:
                if (world_y <= height + kTreeTrunkHeight) {
                    return kWood;
                }
                if (world_y <= height + kTreeTrunkHeight + kTreeCanopyHeight) {
                    return kLeaves;
                }
                break;
            case VegetationType::Cactus:
                if (world_y <= height + kCactusHeight) {
                    return kCactus;
                }
                break;
            case VegetationType::None:
                break;
        }
    }
    return lcu::voxel::kAirBlockId;
}

// `height` (Phase 73.4's own real fix: a submerged column - one whose
// own terrain height sits at or below kSeaLevel - gets real sand for
// both its surface and subsurface, regardless of biome, mirroring
// generate_terrain_chunk's own override exactly) - every real caller
// below already has the column's own height on hand.
lcu::voxel::BlockId expected_surface_for(Biome biome, lcu::i32 height) {
    if (height <= lcu::world::worldgen::kSeaLevel) {
        return kSand;
    }
    switch (biome) {
        case Biome::Plains:
            return kGrass;
        case Biome::Desert:
            return kSand;
        case Biome::Snowy:
            return kSnow;
    }
    return kGrass;
}

lcu::voxel::BlockId expected_subsurface_for(Biome biome, lcu::i32 height) {
    if (height <= lcu::world::worldgen::kSeaLevel) {
        return kSand;
    }
    switch (biome) {
        case Biome::Plains:
            return kDirt;
        case Biome::Desert:
            return kSand;
        case Biome::Snowy:
            return kDirt;
    }
    return kDirt;
}

}  // namespace

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

TEST(Worldgen, BiomeAtIsDeterministic) {
    const Biome b1 = biome_at(42, 100, -50);
    const Biome b2 = biome_at(42, 100, -50);
    const Biome b3 = biome_at(42, 100, -50);

    EXPECT_EQ(b1, b2);
    EXPECT_EQ(b2, b3);
}

TEST(Worldgen, BiomeAtProducesAllThreeCategoriesOverARealArea) {
    // Real proxy for "the climate model actually produces variety, not
    // always the same biome": scan a wide area and confirm all three
    // Biome values genuinely occur, not just Plains (the widest band,
    // see worldgen.cpp's kSnowyThreshold/kDesertThreshold comment).
    bool saw_snowy = false;
    bool saw_plains = false;
    bool saw_desert = false;
    for (lcu::i32 x = -1500; x <= 1500 && !(saw_snowy && saw_plains && saw_desert); x += 13) {
        for (lcu::i32 z = -1500; z <= 1500 && !(saw_snowy && saw_plains && saw_desert); z += 17) {
            switch (biome_at(7, x, z)) {
                case Biome::Snowy:
                    saw_snowy = true;
                    break;
                case Biome::Plains:
                    saw_plains = true;
                    break;
                case Biome::Desert:
                    saw_desert = true;
                    break;
            }
        }
    }
    EXPECT_TRUE(saw_snowy) << "no Snowy column found in a wide area sample";
    EXPECT_TRUE(saw_plains) << "no Plains column found in a wide area sample";
    EXPECT_TRUE(saw_desert) << "no Desert column found in a wide area sample";
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
    constexpr lcu::i32 kSubsurfaceDepth = 3;  // matches worldgen.cpp's own kSubsurfaceDepth.
    const ChunkCoord coord{0, 0, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/99, kTestBiomeBlocks, kStone, kWater, kTestOreBlocks,
                            kTestVegetationBlocks);

    // Spot-check a handful of columns against terrain_height/biome_at
    // directly - that column's biome surface block at the height, that
    // biome's subsurface block for kSubsurfaceDepth layers beneath it,
    // stone/cave/ore deeper (Phase 40 - is_cave/ore_at can turn a stone-
    // band cell into air or an ore block, so the expected value there is
    // computed the same way generate_terrain_chunk itself computes it,
    // not assumed to always be kStone), water (Phase 37) between the
    // surface and sea level for a below-sea-level column, air above sea
    // level.
    for (lcu::u32 lx : {0u, 5u, 15u}) {
        for (lcu::u32 lz : {0u, 8u, 15u}) {
            const lcu::i32 world_x = static_cast<lcu::i32>(lx);
            const lcu::i32 world_z = static_cast<lcu::i32>(lz);
            const lcu::i32 height = terrain_height(99, world_x, world_z);
            const Biome biome = biome_at(99, world_x, world_z);
            for (lcu::u32 ly = 0; ly < Chunk::kEdgeLength; ++ly) {
                const lcu::i32 world_y = static_cast<lcu::i32>(ly);
                const auto block = chunk.block_at(lx, ly, lz);
                if (world_y > height) {
                    EXPECT_EQ(block, expected_above_terrain_block(99, world_x, world_y, world_z, height, biome))
                        << "(" << lx << "," << ly << "," << lz << ")";
                } else if (world_y == height) {
                    EXPECT_EQ(block, expected_surface_for(biome, height)) << "(" << lx << "," << ly << "," << lz << ")";
                } else if (world_y > height - kSubsurfaceDepth) {
                    EXPECT_EQ(block, expected_subsurface_for(biome, height)) << "(" << lx << "," << ly << "," << lz << ")";
                } else {
                    EXPECT_EQ(block, expected_stone_band_block(99, world_x, world_y, world_z, height))
                        << "(" << lx << "," << ly << "," << lz << ")";
                }
            }
        }
    }
}

TEST(Worldgen, ChunkFarAboveTerrainIsEntirelyAir) {
    // world_y range [1600, 1616) - far above any plausible terrain
    // height AND far above sea level, so no water fill applies either.
    const ChunkCoord coord{0, 100, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/1, kTestBiomeBlocks, kStone, kWater, kTestOreBlocks,
                            kTestVegetationBlocks);

    EXPECT_TRUE(chunk.is_empty());
}

TEST(Worldgen, ChunkFarBelowTerrainIsStoneCaveOrOre) {
    // world_y range [-1600, -1584) - far below any plausible terrain
    // height (and far below the surface/subsurface layers near it), so
    // every block here comes from generate_terrain_chunk's stone-band
    // branch: real stone, unless is_cave/ore_at (Phase 40 - noise fields
    // with no artificial depth ceiling) carve it into air or substitute
    // an ore block, the same as any other stone-band cell. Never a
    // surface/subsurface block or water either way.
    const ChunkCoord coord{0, -100, 0};

    Chunk chunk;
    generate_terrain_chunk(chunk, coord, /*seed=*/1, kTestBiomeBlocks, kStone, kWater, kTestOreBlocks,
                            kTestVegetationBlocks);

    const auto check_cell = [&](lcu::u32 lx, lcu::u32 ly, lcu::u32 lz) {
        const lcu::i32 world_x = coord.x * static_cast<lcu::i32>(Chunk::kEdgeLength) + static_cast<lcu::i32>(lx);
        const lcu::i32 world_y = coord.y * static_cast<lcu::i32>(Chunk::kEdgeLength) + static_cast<lcu::i32>(ly);
        const lcu::i32 world_z = coord.z * static_cast<lcu::i32>(Chunk::kEdgeLength) + static_cast<lcu::i32>(lz);
        const lcu::i32 height = terrain_height(1, world_x, world_z);
        EXPECT_EQ(chunk.block_at(lx, ly, lz), expected_stone_band_block(1, world_x, world_y, world_z, height))
            << "(" << lx << "," << ly << "," << lz << ")";
    };
    check_cell(0, 0, 0);
    check_cell(15, 15, 15);
    check_cell(7, 3, 12);
}

TEST(Worldgen, SurfaceLayerIsExactlyOneBlockThickAtTheHeight) {
    constexpr lcu::u32 kSeed = 5;

    // Locate the actual chunk containing world (0, height, 0)'s surface
    // - the terrain height isn't guaranteed to land inside chunk
    // coordinate 0 (Phase 37: terrain_height is centered on sea level,
    // so it can even be negative), so generate whichever chunk really
    // contains it instead of assuming a hardcoded coordinate does.
    const lcu::i32 height = terrain_height(kSeed, 0, 0);
    const Biome biome = biome_at(kSeed, 0, 0);
    const auto split =
        lcu::voxel::world_to_chunk_and_local({0, height, 0}, Chunk::kEdgeLength);
    const auto split_below =
        lcu::voxel::world_to_chunk_and_local({0, height - 1, 0}, Chunk::kEdgeLength);
    ASSERT_EQ(split.chunk, split_below.chunk) << "height and height-1 landed in different chunks - pick a "
                                                  "different seed/column, or generate both chunks";

    Chunk chunk;
    generate_terrain_chunk(chunk, split.chunk, kSeed, kTestBiomeBlocks, kStone, kWater, kTestOreBlocks,
                            kTestVegetationBlocks);

    EXPECT_EQ(chunk.block_at(split.local.x, split.local.y, split.local.z), expected_surface_for(biome, height));
    EXPECT_EQ(chunk.block_at(split_below.local.x, split_below.local.y, split_below.local.z),
              expected_subsurface_for(biome, height))
        << "block directly beneath the surface should be that biome's subsurface block, not its surface block or "
           "stone";
}

TEST(Worldgen, BelowSeaLevelColumnIsFilledWithWaterUpToSeaLevel) {
    // Phase 37's real new behavior: find a column whose terrain height
    // lands below sea level (a real "ocean" column, not a hypothetical
    // one) and confirm water actually fills the gap up to (and
    // including) sea level, with air strictly above it.
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
            generate_terrain_chunk(chunk, split.chunk, kSeed, kTestBiomeBlocks, kStone, kWater, kTestOreBlocks,
                                    kTestVegetationBlocks);
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

TEST(Worldgen, BelowSeaLevelColumnGetsRealSandInsteadOfItsBiomesNormalSurfaceSubsurface) {
    // Phase 73.4's real fix: a submerged column's own surface/subsurface
    // should be real sand (reusing desert_surface/desert_subsurface,
    // the same real sand block every biome's own desert already uses),
    // not that biome's normal grass/dirt/snow - a real, previously
    // undocumented gap (no prior phase ever implemented this; Phase
    // 37's own DECISIONS.md entry explicitly notes "no sand shoreline
    // transition" as a known, deferred gap at the time).
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
    const auto split = lcu::voxel::world_to_chunk_and_local({found_x, height, found_z}, Chunk::kEdgeLength);
    const auto split_below = lcu::voxel::world_to_chunk_and_local({found_x, height - 1, found_z}, Chunk::kEdgeLength);
    ASSERT_EQ(split.chunk, split_below.chunk) << "height and height-1 landed in different chunks - pick a "
                                                  "different seed/column, or generate both chunks";

    Chunk chunk;
    generate_terrain_chunk(chunk, split.chunk, kSeed, kTestBiomeBlocks, kStone, kWater, kTestOreBlocks,
                            kTestVegetationBlocks);

    EXPECT_EQ(chunk.block_at(split.local.x, split.local.y, split.local.z), kSand)
        << "a submerged column's own surface block should be sand regardless of its biome";
    EXPECT_EQ(chunk.block_at(split_below.local.x, split_below.local.y, split_below.local.z), kSand)
        << "a submerged column's own subsurface block should be sand too, not that biome's normal subsurface";
}

TEST(Worldgen, IsCaveIsDeterministic) {
    const bool a = is_cave(42, 100, -50, 30, /*surface_height=*/40);
    const bool b = is_cave(42, 100, -50, 30, /*surface_height=*/40);
    const bool c = is_cave(42, 100, -50, 30, /*surface_height=*/40);

    EXPECT_EQ(a, b);
    EXPECT_EQ(b, c);
}

TEST(Worldgen, IsCaveNeverFiresExactlyAtTheSurface) {
    // Real proxy for the header's own "keeps tunnels from ever punching
    // a hole right at ground level" guarantee: at world_y == surface_height
    // itself, is_cave must always be false, for any minimum depth > 0
    // worldgen.cpp's own kCaveMinDepthBelowSurface might use.
    for (lcu::i32 x = -300; x <= 300; x += 23) {
        for (lcu::i32 z = -300; z <= 300; z += 29) {
            const lcu::i32 height = terrain_height(11, x, z);
            EXPECT_FALSE(is_cave(11, x, height, z, height)) << "at (" << x << "," << height << "," << z << ")";
        }
    }
}

TEST(Worldgen, IsCaveProducesSomeCarvedCellsWellBelowTheSurface) {
    // Real proxy for "actually carves tunnels, not always false": scan a
    // real, wide, comfortably-deep volume beneath several columns' own
    // terrain height and confirm at least one cell comes back true.
    constexpr lcu::u32 kSeed = 21;
    bool found_cave = false;
    for (lcu::i32 x = 0; x < 400 && !found_cave; x += 5) {
        for (lcu::i32 z = 0; z < 400 && !found_cave; z += 5) {
            const lcu::i32 height = terrain_height(kSeed, x, z);
            for (lcu::i32 depth = 8; depth < 60 && !found_cave; depth += 2) {
                if (is_cave(kSeed, x, height - depth, z, height)) {
                    found_cave = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_cave) << "no carved cell found in a real, wide, sufficiently-deep sample";
}

TEST(Worldgen, OreAtIsDeterministic) {
    const OreType a = ore_at(42, 100, -20, -50);
    const OreType b = ore_at(42, 100, -20, -50);
    const OreType c = ore_at(42, 100, -20, -50);

    EXPECT_EQ(a, b);
    EXPECT_EQ(b, c);
}

TEST(Worldgen, OreAtProducesBothOreTypesOverARealVolume) {
    // Real proxy for "both ores genuinely occur, not just None everywhere
    // (the overwhelmingly common case by design, see worldgen.h's own
    // comment) or just one of the two": scan a wide underground volume
    // and confirm both Coal and Iron occur somewhere in it.
    constexpr lcu::u32 kSeed = 5;
    bool saw_coal = false;
    bool saw_iron = false;
    for (lcu::i32 x = -160; x <= 160 && !(saw_coal && saw_iron); x += 4) {
        for (lcu::i32 y = -48; y <= 40 && !(saw_coal && saw_iron); y += 2) {
            for (lcu::i32 z = -160; z <= 160 && !(saw_coal && saw_iron); z += 4) {
                switch (ore_at(kSeed, x, y, z)) {
                    case OreType::Coal:
                        saw_coal = true;
                        break;
                    case OreType::Iron:
                        saw_iron = true;
                        break;
                    case OreType::None:
                        break;
                }
            }
        }
    }
    EXPECT_TRUE(saw_coal) << "no Coal found in a wide underground sample";
    EXPECT_TRUE(saw_iron) << "no Iron found in a wide underground sample";
}

TEST(Worldgen, VegetationAtIsDeterministic) {
    const VegetationType a = vegetation_at(42, 100, -50, Biome::Plains);
    const VegetationType b = vegetation_at(42, 100, -50, Biome::Plains);
    const VegetationType c = vegetation_at(42, 100, -50, Biome::Plains);

    EXPECT_EQ(a, b);
    EXPECT_EQ(b, c);
}

TEST(Worldgen, VegetationAtNeverReturnsTreeOrCactusForSnowy) {
    // Real proxy for worldgen.h's own "Snowy never gets vegetation"
    // guarantee: scan a wide area passing Biome::Snowy explicitly and
    // confirm every result is None, regardless of what the same column
    // would produce for a different biome.
    for (lcu::i32 x = -300; x <= 300; x += 11) {
        for (lcu::i32 z = -300; z <= 300; z += 13) {
            EXPECT_EQ(vegetation_at(9, x, z, Biome::Snowy), VegetationType::None)
                << "at (" << x << "," << z << ")";
        }
    }
}

TEST(Worldgen, VegetationAtNeverReturnsCactusForPlainsOrTreeForDesert) {
    // Real proxy for the header's own "Tree only for Plains, Cactus
    // only for Desert" guarantee, checked at the same columns against
    // both biomes so a real difference in outcome is exercised, not
    // just an absence of Cactus/Tree that could also be explained by
    // an always-None implementation.
    for (lcu::i32 x = -300; x <= 300; x += 11) {
        for (lcu::i32 z = -300; z <= 300; z += 13) {
            EXPECT_NE(vegetation_at(9, x, z, Biome::Plains), VegetationType::Cactus)
                << "at (" << x << "," << z << ")";
            EXPECT_NE(vegetation_at(9, x, z, Biome::Desert), VegetationType::Tree)
                << "at (" << x << "," << z << ")";
        }
    }
}

TEST(Worldgen, VegetationAtProducesBothTreeAndCactusOverARealArea) {
    // Real proxy for "vegetation genuinely occurs, not just None
    // everywhere (the common case by design, see worldgen.h)": scan a
    // wide area and confirm both a Tree (queried as Plains) and a
    // Cactus (queried as Desert) genuinely occur somewhere in it.
    constexpr lcu::u32 kSeed = 5;
    bool saw_tree = false;
    bool saw_cactus = false;
    for (lcu::i32 x = -300; x <= 300 && !(saw_tree && saw_cactus); x += 2) {
        for (lcu::i32 z = -300; z <= 300 && !(saw_tree && saw_cactus); z += 2) {
            if (vegetation_at(kSeed, x, z, Biome::Plains) == VegetationType::Tree) {
                saw_tree = true;
            }
            if (vegetation_at(kSeed, x, z, Biome::Desert) == VegetationType::Cactus) {
                saw_cactus = true;
            }
        }
    }
    EXPECT_TRUE(saw_tree) << "no Tree found in a wide area sample";
    EXPECT_TRUE(saw_cactus) << "no Cactus found in a wide area sample";
}

TEST(Worldgen, GenerateTerrainChunkPlacesARealTreeWhereVegetationAtSaysOneGrows) {
    // End-to-end check that generate_terrain_chunk actually places the
    // trunk/canopy generate_terrain_chunk's own doc comment describes,
    // not just that vegetation_at returns Tree in isolation: find a
    // real dry Plains column where vegetation_at says Tree, generate
    // its chunk, and confirm the real trunk/canopy shape appears.
    constexpr lcu::u32 kSeed = 5;
    lcu::i32 found_x = 0;
    lcu::i32 found_z = 0;
    bool found = false;
    for (lcu::i32 x = -300; x <= 300 && !found; x += 2) {
        for (lcu::i32 z = -300; z <= 300 && !found; z += 2) {
            if (biome_at(kSeed, x, z) != Biome::Plains) {
                continue;
            }
            const lcu::i32 height = terrain_height(kSeed, x, z);
            if (height <= lcu::world::worldgen::kSeaLevel) {
                continue;
            }
            if (vegetation_at(kSeed, x, z, Biome::Plains) != VegetationType::Tree) {
                continue;
            }
            // Only accept a column whose whole trunk+canopy lands in
            // one chunk - a tree straddling two chunks is a real,
            // separate case this deliberately single-column-scoped
            // feature doesn't need to handle (see worldgen.h), not
            // something this test should stumble into by chance.
            const auto trunk_start = lcu::voxel::world_to_chunk_and_local({x, height + 1, z}, Chunk::kEdgeLength);
            const auto canopy_end = lcu::voxel::world_to_chunk_and_local(
                {x, height + kTreeTrunkHeight + kTreeCanopyHeight, z}, Chunk::kEdgeLength);
            if (trunk_start.chunk != canopy_end.chunk) {
                continue;
            }
            found_x = x;
            found_z = z;
            found = true;
        }
    }
    ASSERT_TRUE(found) << "no dry Plains column with a single-chunk Tree found in a wide sample";

    const lcu::i32 height = terrain_height(kSeed, found_x, found_z);
    const auto split = lcu::voxel::world_to_chunk_and_local({found_x, height + 1, found_z}, Chunk::kEdgeLength);

    Chunk chunk;
    generate_terrain_chunk(chunk, split.chunk, kSeed, kTestBiomeBlocks, kStone, kWater, kTestOreBlocks,
                            kTestVegetationBlocks);

    for (lcu::i32 world_y = height + 1; world_y <= height + kTreeTrunkHeight; ++world_y) {
        const auto local = lcu::voxel::world_to_chunk_and_local({found_x, world_y, found_z}, Chunk::kEdgeLength);
        ASSERT_EQ(local.chunk, split.chunk) << "trunk spans more than one chunk - pick a different seed/column";
        EXPECT_EQ(chunk.block_at(local.local.x, local.local.y, local.local.z), kWood) << "world_y=" << world_y;
    }
    for (lcu::i32 world_y = height + kTreeTrunkHeight + 1; world_y <= height + kTreeTrunkHeight + kTreeCanopyHeight;
         ++world_y) {
        const auto local = lcu::voxel::world_to_chunk_and_local({found_x, world_y, found_z}, Chunk::kEdgeLength);
        ASSERT_EQ(local.chunk, split.chunk) << "canopy spans more than one chunk - pick a different seed/column";
        EXPECT_EQ(chunk.block_at(local.local.x, local.local.y, local.local.z), kLeaves) << "world_y=" << world_y;
    }
}
