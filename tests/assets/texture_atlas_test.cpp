#include "lcu/assets/texture_atlas.h"

#include <gtest/gtest.h>

using lcu::assets::kAtlasSize;
using lcu::assets::kMaxTiles;
using lcu::assets::kTileInsetTexels;
using lcu::assets::kTilesPerRow;
using lcu::assets::kTileSize;
using lcu::assets::tile_uv_range;

TEST(TextureAtlasConstants, RealSizesMatchA256x256AtlasOf16x16Tiles) {
    EXPECT_EQ(kAtlasSize, 256u);
    EXPECT_EQ(kTileSize, 16u);
    EXPECT_EQ(kTilesPerRow, 16u);
    EXPECT_EQ(kMaxTiles, 256u);
}

TEST(TileUvRange, Tile0StartsAtTheAtlasTopLeftInsetByHalfATexel) {
    const auto range = tile_uv_range(0);
    const float expected_u0 = kTileInsetTexels / static_cast<float>(kAtlasSize);
    const float expected_u1 = (static_cast<float>(kTileSize) - kTileInsetTexels) / static_cast<float>(kAtlasSize);
    EXPECT_FLOAT_EQ(range.u0, expected_u0);
    EXPECT_FLOAT_EQ(range.v0, expected_u0);
    EXPECT_FLOAT_EQ(range.u1, expected_u1);
    EXPECT_FLOAT_EQ(range.v1, expected_u1);
}

TEST(TileUvRange, Tile1IsOneTileStepToTheRightOfTile0) {
    const auto tile0 = tile_uv_range(0);
    const auto tile1 = tile_uv_range(1);
    const float step = static_cast<float>(kTileSize) / static_cast<float>(kAtlasSize);
    EXPECT_FLOAT_EQ(tile1.u0, tile0.u0 + step);
    EXPECT_FLOAT_EQ(tile1.u1, tile0.u1 + step);
    // Same row: v unchanged.
    EXPECT_FLOAT_EQ(tile1.v0, tile0.v0);
    EXPECT_FLOAT_EQ(tile1.v1, tile0.v1);
}

TEST(TileUvRange, Tile100IsOnItsRealRowAndColumn) {
    // 100 = 6*16 + 4 -> row 6, column 4 (0-indexed, kTilesPerRow == 16).
    const auto range = tile_uv_range(100);
    const float step = static_cast<float>(kTileSize) / static_cast<float>(kAtlasSize);
    const float inset = kTileInsetTexels / static_cast<float>(kAtlasSize);
    const float expected_u0 = 4.0f * step + inset;
    const float expected_v0 = 6.0f * step + inset;
    EXPECT_FLOAT_EQ(range.u0, expected_u0);
    EXPECT_FLOAT_EQ(range.v0, expected_v0);
}

TEST(TileUvRange, LastTileStaysFullyWithinTheAtlas) {
    const auto range = tile_uv_range(kMaxTiles - 1);
    EXPECT_GT(range.u0, 0.0f);
    EXPECT_GT(range.v0, 0.0f);
    EXPECT_LT(range.u1, 1.0f);
    EXPECT_LT(range.v1, 1.0f);
    // Real, specific expected values - row 15, column 15.
    const float step = static_cast<float>(kTileSize) / static_cast<float>(kAtlasSize);
    const float inset = kTileInsetTexels / static_cast<float>(kAtlasSize);
    EXPECT_FLOAT_EQ(range.u0, 15.0f * step + inset);
    EXPECT_FLOAT_EQ(range.v0, 15.0f * step + inset);
}

TEST(TileUvRange, EveryTileHasARealNonEmptyInsetArea) {
    for (lcu::u32 i = 0; i < kMaxTiles; ++i) {
        const auto range = tile_uv_range(i);
        EXPECT_LT(range.u0, range.u1) << "tile " << i;
        EXPECT_LT(range.v0, range.v1) << "tile " << i;
    }
}
