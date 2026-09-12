#include "lcu/assets/procedural_textures.h"

#include <gtest/gtest.h>

using lcu::assets::build_block_atlas_pixels;
using lcu::assets::generate_crack;
using lcu::assets::generate_tile;
using lcu::assets::generate_wheat_stage;
using lcu::assets::kAtlasSize;
using lcu::assets::kTileSize;
using lcu::assets::TileId;
using lcu::assets::TilePixels;

namespace {

// Every real generate_* function under test, by TileId - used to
// iterate "every real texture" without a 27-way copy/paste block.
constexpr TileId kAllTiles[] = {
    TileId::GrassTop,  TileId::GrassSide, TileId::Dirt,  TileId::Stone,           TileId::Sand,
    TileId::Snow,      TileId::Water,     TileId::WoodSide, TileId::WoodTop,      TileId::Leaves,
    TileId::CoalOre,   TileId::IronOre,   TileId::Torch, TileId::CraftingTableTop,
    TileId::Cactus,    TileId::Compost,   TileId::Planks,
    TileId::Crack0,    TileId::Crack1,    TileId::Crack2, TileId::Crack3, TileId::Crack4,
    TileId::Crack5,    TileId::Crack6,    TileId::Crack7, TileId::Crack8, TileId::Crack9,
    TileId::WheatStage0, TileId::WheatStage1, TileId::WheatStage2, TileId::WheatStage3,
    TileId::WheatStage4, TileId::WheatStage5, TileId::WheatStage6, TileId::WheatStage7,
};

}  // namespace

TEST(ProceduralTextures, EveryTileHasTheRealExpected16x16RgbaSize) {
    for (const TileId tile : kAllTiles) {
        const TilePixels pixels = generate_tile(tile);
        EXPECT_EQ(pixels.size(), static_cast<std::size_t>(kTileSize) * kTileSize * 4);
    }
}

TEST(ProceduralTextures, GenerationIsDeterministicAcrossRepeatedCalls) {
    for (const TileId tile : kAllTiles) {
        const TilePixels first = generate_tile(tile);
        const TilePixels second = generate_tile(tile);
        EXPECT_EQ(first, second) << "tile " << static_cast<lcu::u32>(tile);
    }
}

TEST(ProceduralTextures, DifferentTilesProduceRealVisiblyDifferentPixels) {
    // A real, meaningful check that these aren't all secretly the same
    // texture under different names - grass top and stone should not
    // produce byte-identical output.
    const TilePixels grass = generate_tile(TileId::GrassTop);
    const TilePixels stone = generate_tile(TileId::Stone);
    EXPECT_NE(grass, stone);
}

TEST(ProceduralTextures, LeavesTextureHasRealTransparentHoles) {
    const TilePixels leaves = generate_tile(TileId::Leaves);
    bool found_transparent = false;
    for (std::size_t i = 3; i < leaves.size(); i += 4) {
        if (leaves[i] == 0) {
            found_transparent = true;
            break;
        }
    }
    EXPECT_TRUE(found_transparent);
}

TEST(ProceduralTextures, WaterTextureIsRealHalfTransparent) {
    const TilePixels water = generate_tile(TileId::Water);
    // Every pixel's alpha should be the real 180 this generator uses -
    // checked on a sample, not assuming a single hardcoded pixel.
    EXPECT_EQ(water[3], 180);
    EXPECT_EQ(water[7], 180);
}

TEST(ProceduralTextures, TorchStemAreaIsOpaqueAndCornersStayTransparentBackground) {
    const TilePixels torch = generate_tile(TileId::Torch);
    // Stem column 7, row 10 (well within the real 6..15 stem rows).
    const std::size_t stem_alpha_index = (static_cast<std::size_t>(10) * kTileSize + 7) * 4 + 3;
    EXPECT_EQ(torch[stem_alpha_index], 255);
    // Top-left corner (0,0) is outside both the stem and flame regions.
    EXPECT_EQ(torch[3], 0);
}

TEST(BuildBlockAtlasPixels, ProducesTheRealFixed256x256Rgba8Buffer) {
    const std::vector<lcu::u8> atlas = build_block_atlas_pixels();
    EXPECT_EQ(atlas.size(), static_cast<std::size_t>(kAtlasSize) * kAtlasSize * 4);
}

TEST(BuildBlockAtlasPixels, EveryRealTileIsReachableAtItsOwnFixedSlot) {
    const std::vector<lcu::u8> atlas = build_block_atlas_pixels();
    for (const TileId tile : kAllTiles) {
        const auto index = static_cast<lcu::u32>(tile);
        const TilePixels expected = generate_tile(tile);
        const lcu::u32 tx = index % 16;
        const lcu::u32 ty = index / 16;
        // Sample one real pixel (the tile's own top-left corner) from
        // the packed atlas and confirm it matches that tile's own
        // standalone generator output at the same local (0,0) offset -
        // a real proof the packing math lands each tile at its own
        // real, correct slot, not just "the buffer is the right size".
        const std::size_t atlas_x = static_cast<std::size_t>(tx) * kTileSize;
        const std::size_t atlas_y = static_cast<std::size_t>(ty) * kTileSize;
        const std::size_t atlas_index = (atlas_y * kAtlasSize + atlas_x) * 4;
        EXPECT_EQ(atlas[atlas_index + 0], expected[0]) << "tile " << index;
        EXPECT_EQ(atlas[atlas_index + 1], expected[1]) << "tile " << index;
        EXPECT_EQ(atlas[atlas_index + 2], expected[2]) << "tile " << index;
        EXPECT_EQ(atlas[atlas_index + 3], expected[3]) << "tile " << index;
    }
}

namespace {

lcu::u32 count_opaque_pixels(const TilePixels& px) {
    lcu::u32 count = 0;
    for (std::size_t i = 3; i < px.size(); i += 4) {
        if (px[i] != 0) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST(GenerateCrack, Stage0HasSomeRealCrackCoverageButStaysMostlyTransparent) {
    const TilePixels stage0 = generate_crack(0);
    const lcu::u32 opaque = count_opaque_pixels(stage0);
    EXPECT_GT(opaque, 0u);
    EXPECT_LT(opaque, static_cast<lcu::u32>(kTileSize) * kTileSize / 2);
}

TEST(GenerateCrack, CoverageGrowsMonotonicallyWithStage) {
    lcu::u32 previous_count = 0;
    for (lcu::u32 stage = 0; stage < 10; ++stage) {
        const lcu::u32 count = count_opaque_pixels(generate_crack(stage));
        EXPECT_GE(count, previous_count) << "stage " << stage;
        previous_count = count;
    }
}

TEST(GenerateCrack, EachStageIsARealSupersetOfTheStageBefore) {
    // Same underlying noise field, only the threshold changes - every
    // pixel cracked at stage N should still be cracked at stage N+1.
    for (lcu::u32 stage = 0; stage < 9; ++stage) {
        const TilePixels lower = generate_crack(stage);
        const TilePixels higher = generate_crack(stage + 1);
        for (std::size_t i = 3; i < lower.size(); i += 4) {
            if (lower[i] != 0) {
                EXPECT_NE(higher[i], 0) << "stage " << stage << " pixel " << i;
            }
        }
    }
}

TEST(GenerateCrack, DeterministicSameStageAlwaysProducesTheSameBytes) {
    const TilePixels first = generate_crack(5);
    const TilePixels second = generate_crack(5);
    EXPECT_EQ(first, second);
}

TEST(GenerateWheatStage, Stage0HasSomeRealCoverageButStaysMostlySparse) {
    const TilePixels stage0 = generate_wheat_stage(0);
    const lcu::u32 opaque = count_opaque_pixels(stage0);
    EXPECT_GT(opaque, 0u);
    EXPECT_LT(opaque, static_cast<lcu::u32>(kTileSize) * kTileSize / 2);
}

TEST(GenerateWheatStage, Stage7IsRealNearlyFullCoverage) {
    const TilePixels stage7 = generate_wheat_stage(7);
    const lcu::u32 opaque = count_opaque_pixels(stage7);
    EXPECT_GT(opaque, static_cast<lcu::u32>(kTileSize) * kTileSize / 2);
}

TEST(GenerateWheatStage, CoverageGrowsMonotonicallyWithStage) {
    lcu::u32 previous_count = 0;
    for (lcu::u32 stage = 0; stage < 8; ++stage) {
        const lcu::u32 count = count_opaque_pixels(generate_wheat_stage(stage));
        EXPECT_GE(count, previous_count) << "stage " << stage;
        previous_count = count;
    }
}

TEST(GenerateWheatStage, EachStageIsARealSupersetOfTheStageBefore) {
    for (lcu::u32 stage = 0; stage < 7; ++stage) {
        const TilePixels lower = generate_wheat_stage(stage);
        const TilePixels higher = generate_wheat_stage(stage + 1);
        for (std::size_t i = 3; i < lower.size(); i += 4) {
            if (lower[i] != 0) {
                EXPECT_NE(higher[i], 0) << "stage " << stage << " pixel " << i;
            }
        }
    }
}

TEST(GenerateWheatStage, MatureWheatIsRealVisiblyMoreGoldenThanYoungWheat) {
    // Real green-to-gold maturity blend: a mature (stage 7) opaque
    // pixel's own red channel should read higher than a young (stage
    // 0) opaque pixel's, on average, at the same coordinate.
    const TilePixels young = generate_wheat_stage(0);
    const TilePixels mature = generate_wheat_stage(7);
    lcu::u32 mature_wins = 0;
    lcu::u32 compared = 0;
    for (std::size_t i = 0; i + 3 < young.size(); i += 4) {
        if (young[i + 3] != 0 && mature[i + 3] != 0) {
            ++compared;
            if (mature[i] > young[i]) {
                ++mature_wins;
            }
        }
    }
    ASSERT_GT(compared, 0u);
    EXPECT_GT(mature_wins, compared / 2);
}

TEST(GenerateWheatStage, DeterministicSameStageAlwaysProducesTheSameBytes) {
    const TilePixels first = generate_wheat_stage(3);
    const TilePixels second = generate_wheat_stage(3);
    EXPECT_EQ(first, second);
}
