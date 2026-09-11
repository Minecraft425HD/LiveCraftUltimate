#include "lcu/assets/procedural_textures.h"

#include <gtest/gtest.h>

using lcu::assets::build_block_atlas_pixels;
using lcu::assets::generate_tile;
using lcu::assets::kAtlasSize;
using lcu::assets::kTileSize;
using lcu::assets::TileId;
using lcu::assets::TilePixels;

namespace {

// Every real generate_* function under test, by TileId - used to
// iterate "every real texture" without a 17-way copy/paste block.
constexpr TileId kAllTiles[] = {
    TileId::GrassTop,  TileId::GrassSide,         TileId::Dirt,    TileId::Stone,   TileId::Sand,
    TileId::Snow,      TileId::Water,             TileId::WoodSide, TileId::WoodTop, TileId::Leaves,
    TileId::CoalOre,   TileId::IronOre,           TileId::Torch,   TileId::CraftingTableTop,
    TileId::Cactus,    TileId::Compost,           TileId::Planks,
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
