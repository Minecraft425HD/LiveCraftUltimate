#include "lcu/assets/skin_texture.h"

#include <gtest/gtest.h>

using lcu::assets::generate_default_skin_pixels;
using lcu::assets::kSkinHeight;
using lcu::assets::kSkinInsetTexels;
using lcu::assets::kSkinWidth;
using lcu::assets::SkinRegion;
using lcu::assets::skin_uv_range;

TEST(SkinTextureConstants, RealSizeMatchesTheMinecraftModernSkinFormat) {
    EXPECT_EQ(kSkinWidth, 64u);
    EXPECT_EQ(kSkinHeight, 64u);
}

TEST(SkinUvRange, HeadFrontMatchesTheRealMinecraftUvLayout) {
    // Real MC layout: head front is an 8x8 rect at (8,8).
    const auto range = skin_uv_range(SkinRegion::HeadFront);
    const float expected_u0 = (8.0f + kSkinInsetTexels) / 64.0f;
    const float expected_v0 = (8.0f + kSkinInsetTexels) / 64.0f;
    const float expected_u1 = (16.0f - kSkinInsetTexels) / 64.0f;
    const float expected_v1 = (16.0f - kSkinInsetTexels) / 64.0f;
    EXPECT_FLOAT_EQ(range.u0, expected_u0);
    EXPECT_FLOAT_EQ(range.v0, expected_v0);
    EXPECT_FLOAT_EQ(range.u1, expected_u1);
    EXPECT_FLOAT_EQ(range.v1, expected_v1);
}

TEST(SkinUvRange, TorsoFrontMatchesTheRealMinecraftUvLayout) {
    // Real MC layout: torso front is an 8x12 rect at (20,20) - matches
    // the brief's own explicit example coordinate.
    const auto range = skin_uv_range(SkinRegion::TorsoFront);
    const float expected_u0 = (20.0f + kSkinInsetTexels) / 64.0f;
    const float expected_v0 = (20.0f + kSkinInsetTexels) / 64.0f;
    const float expected_u1 = (28.0f - kSkinInsetTexels) / 64.0f;
    const float expected_v1 = (32.0f - kSkinInsetTexels) / 64.0f;
    EXPECT_FLOAT_EQ(range.u0, expected_u0);
    EXPECT_FLOAT_EQ(range.v0, expected_v0);
    EXPECT_FLOAT_EQ(range.u1, expected_u1);
    EXPECT_FLOAT_EQ(range.v1, expected_v1);
}

TEST(SkinUvRange, RightArmFrontMatchesTheRealMinecraftUvLayout) {
    // Real MC layout: right-arm front is a 4x12 rect at (44,20).
    const auto range = skin_uv_range(SkinRegion::RightArmFront);
    EXPECT_FLOAT_EQ(range.u0, (44.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v0, (20.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.u1, (48.0f - kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v1, (32.0f - kSkinInsetTexels) / 64.0f);
}

TEST(SkinUvRange, LeftArmFrontMatchesTheRealMinecraftUvLayout) {
    // Real MC layout: left-arm front is a 4x12 rect at (36,52).
    const auto range = skin_uv_range(SkinRegion::LeftArmFront);
    EXPECT_FLOAT_EQ(range.u0, (36.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v0, (52.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.u1, (40.0f - kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v1, (64.0f - kSkinInsetTexels) / 64.0f);
}

TEST(SkinUvRange, RightLegFrontMatchesTheRealMinecraftUvLayout) {
    // Real MC layout: right-leg front is a 4x12 rect at (4,20).
    const auto range = skin_uv_range(SkinRegion::RightLegFront);
    EXPECT_FLOAT_EQ(range.u0, (4.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v0, (20.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.u1, (8.0f - kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v1, (32.0f - kSkinInsetTexels) / 64.0f);
}

TEST(SkinUvRange, LeftLegFrontMatchesTheRealMinecraftUvLayout) {
    // Real MC layout: left-leg front is a 4x12 rect at (20,52).
    const auto range = skin_uv_range(SkinRegion::LeftLegFront);
    EXPECT_FLOAT_EQ(range.u0, (20.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v0, (52.0f + kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.u1, (24.0f - kSkinInsetTexels) / 64.0f);
    EXPECT_FLOAT_EQ(range.v1, (64.0f - kSkinInsetTexels) / 64.0f);
}

TEST(SkinUvRange, EveryRealRegionHasARealNonEmptyInsetArea) {
    for (lcu::u32 i = 0; i < static_cast<lcu::u32>(SkinRegion::Count); ++i) {
        const auto region = static_cast<SkinRegion>(i);
        const auto range = skin_uv_range(region);
        EXPECT_LT(range.u0, range.u1) << "region " << i;
        EXPECT_LT(range.v0, range.v1) << "region " << i;
    }
}

TEST(SkinUvRange, EveryRealRegionStaysFullyWithinTheAtlas) {
    for (lcu::u32 i = 0; i < static_cast<lcu::u32>(SkinRegion::Count); ++i) {
        const auto region = static_cast<SkinRegion>(i);
        const auto range = skin_uv_range(region);
        EXPECT_GE(range.u0, 0.0f) << "region " << i;
        EXPECT_GE(range.v0, 0.0f) << "region " << i;
        EXPECT_LE(range.u1, 1.0f) << "region " << i;
        EXPECT_LE(range.v1, 1.0f) << "region " << i;
    }
}

TEST(GenerateDefaultSkinPixels, ProducesTheRealFixed64x64Rgba8Buffer) {
    const auto pixels = generate_default_skin_pixels();
    EXPECT_EQ(pixels.size(), static_cast<lcu::usize>(64) * 64 * 4);
}

TEST(GenerateDefaultSkinPixels, DeterministicSameCallAlwaysProducesTheSameBytes) {
    const auto first = generate_default_skin_pixels();
    const auto second = generate_default_skin_pixels();
    EXPECT_EQ(first, second);
}

TEST(GenerateDefaultSkinPixels, EveryRealRegionIsFullyOpaqueAndNonBlack) {
    // Every one of the 36 real named regions should have been painted
    // by generate_default_skin_pixels - a real, deliberate choice (a
    // fully procedural skin has no "undefined" pixels the way an
    // uploaded PNG's transparent background might, see Phase 62).
    const auto pixels = generate_default_skin_pixels();
    for (lcu::u32 i = 0; i < static_cast<lcu::u32>(SkinRegion::Count); ++i) {
        const auto region = static_cast<SkinRegion>(i);
        const auto range = skin_uv_range(region);
        // Sample the region's own real center pixel.
        const auto x = static_cast<lcu::u32>((range.u0 + range.u1) * 0.5f * 64.0f);
        const auto y = static_cast<lcu::u32>((range.v0 + range.v1) * 0.5f * 64.0f);
        const lcu::usize offset = (static_cast<lcu::usize>(y) * 64 + x) * 4;
        EXPECT_EQ(pixels[offset + 3], 255) << "region " << i << " alpha";
        const bool non_black = pixels[offset + 0] != 0 || pixels[offset + 1] != 0 || pixels[offset + 2] != 0;
        EXPECT_TRUE(non_black) << "region " << i;
    }
}
