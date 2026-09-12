#include "lcu/assets/font_atlas.h"

#include <gtest/gtest.h>

using lcu::assets::build_font_atlas_pixels;
using lcu::assets::generate_glyph_pixels;
using lcu::assets::glyph_uv_range;
using lcu::assets::kFirstChar;
using lcu::assets::kFontAtlasHeight;
using lcu::assets::kFontAtlasWidth;
using lcu::assets::kGlyphCount;
using lcu::assets::kGlyphHeight;
using lcu::assets::kGlyphInsetTexels;
using lcu::assets::kGlyphsPerRow;
using lcu::assets::kGlyphWidth;
using lcu::assets::kLastChar;

TEST(FontAtlasConstants, RealSizesMatchA95CharacterMonospaceFont) {
    EXPECT_EQ(kFirstChar, 32u);
    EXPECT_EQ(kLastChar, 126u);
    EXPECT_EQ(kGlyphCount, 95u);
    EXPECT_EQ(kGlyphWidth, 6u);
    EXPECT_EQ(kGlyphHeight, 8u);
    EXPECT_EQ(kGlyphsPerRow, 16u);
    EXPECT_EQ(kFontAtlasWidth, 96u);
    EXPECT_EQ(kFontAtlasHeight, 48u);
}

TEST(GlyphUvRange, SpaceStartsAtTheAtlasTopLeftInsetByHalfATexel) {
    const auto range = glyph_uv_range(' ');
    const float expected_u0 = kGlyphInsetTexels / static_cast<float>(kFontAtlasWidth);
    const float expected_v0 = kGlyphInsetTexels / static_cast<float>(kFontAtlasHeight);
    EXPECT_FLOAT_EQ(range.u0, expected_u0);
    EXPECT_FLOAT_EQ(range.v0, expected_v0);
}

TEST(GlyphUvRange, NextCharIsOneGlyphStepToTheRight) {
    const auto space = glyph_uv_range(' ');
    const auto bang = glyph_uv_range('!');
    const float step = static_cast<float>(kGlyphWidth) / static_cast<float>(kFontAtlasWidth);
    EXPECT_FLOAT_EQ(bang.u0, space.u0 + step);
    // Same row: v unchanged.
    EXPECT_FLOAT_EQ(bang.v0, space.v0);
}

TEST(GlyphUvRange, LastCharTildeStaysFullyWithinTheAtlas) {
    const auto range = glyph_uv_range('~');
    EXPECT_GT(range.u0, 0.0f);
    EXPECT_GT(range.v0, 0.0f);
    EXPECT_LT(range.u1, 1.0f);
    EXPECT_LT(range.v1, 1.0f);
    // '~' is character 126, index 94 -> row 5 (94/16), column 14 (94%16).
    const float step_u = static_cast<float>(kGlyphWidth) / static_cast<float>(kFontAtlasWidth);
    const float step_v = static_cast<float>(kGlyphHeight) / static_cast<float>(kFontAtlasHeight);
    const float inset_u = kGlyphInsetTexels / static_cast<float>(kFontAtlasWidth);
    const float inset_v = kGlyphInsetTexels / static_cast<float>(kFontAtlasHeight);
    EXPECT_FLOAT_EQ(range.u0, 14.0f * step_u + inset_u);
    EXPECT_FLOAT_EQ(range.v0, 5.0f * step_v + inset_v);
}

TEST(GlyphUvRange, EveryRealCharacterHasARealNonEmptyInsetArea) {
    for (lcu::u32 code = kFirstChar; code <= kLastChar; ++code) {
        const auto range = glyph_uv_range(static_cast<char>(code));
        EXPECT_LT(range.u0, range.u1) << "char " << code;
        EXPECT_LT(range.v0, range.v1) << "char " << code;
    }
}

TEST(GlyphUvRange, OutOfRangeCharacterFallsBackToQuestionMark) {
    const auto question_mark = glyph_uv_range('?');
    const auto control_char = glyph_uv_range('\n');
    EXPECT_FLOAT_EQ(control_char.u0, question_mark.u0);
    EXPECT_FLOAT_EQ(control_char.v0, question_mark.v0);
}

TEST(GenerateGlyphPixels, SpaceIsFullyTransparent) {
    const auto pixels = generate_glyph_pixels(' ');
    for (lcu::usize i = 3; i < pixels.size(); i += 4) {
        EXPECT_EQ(pixels[i], 0) << "alpha byte " << i;
    }
}

TEST(GenerateGlyphPixels, ARealLetterHasAtLeastOneOpaquePixel) {
    const auto pixels = generate_glyph_pixels('A');
    bool found_opaque = false;
    for (lcu::usize i = 3; i < pixels.size(); i += 4) {
        if (pixels[i] != 0) {
            found_opaque = true;
            break;
        }
    }
    EXPECT_TRUE(found_opaque);
}

TEST(GenerateGlyphPixels, RightColumnAndBottomRowStaySpacingEvenForAFullLetter) {
    // 'H' fills most of its 5x7 art - still leaves the 6th column/8th row
    // (index 5 and 7) as real transparent spacing, per font_atlas.h's
    // own cell layout.
    const auto pixels = generate_glyph_pixels('H');
    for (lcu::u32 y = 0; y < kGlyphHeight; ++y) {
        const lcu::usize right_col_alpha = (static_cast<lcu::usize>(y) * kGlyphWidth + 5) * 4 + 3;
        EXPECT_EQ(pixels[right_col_alpha], 0) << "y=" << y;
    }
    for (lcu::u32 x = 0; x < kGlyphWidth; ++x) {
        const lcu::usize bottom_row_alpha = (7u * kGlyphWidth + x) * 4 + 3;
        EXPECT_EQ(pixels[bottom_row_alpha], 0) << "x=" << x;
    }
}

TEST(GenerateGlyphPixels, DeterministicSameCharacterAlwaysProducesTheSameBytes) {
    const auto first = generate_glyph_pixels('%');
    const auto second = generate_glyph_pixels('%');
    EXPECT_EQ(first, second);
}

TEST(BuildFontAtlasPixels, ProducesTheRealFixedSizeRgba8Buffer) {
    const std::vector<lcu::u8> atlas = build_font_atlas_pixels();
    EXPECT_EQ(atlas.size(), static_cast<lcu::usize>(kFontAtlasWidth) * kFontAtlasHeight * 4);
}

TEST(BuildFontAtlasPixels, EveryRealGlyphIsReachableAtItsOwnFixedSlot) {
    const std::vector<lcu::u8> atlas = build_font_atlas_pixels();

    for (lcu::u32 code = kFirstChar; code <= kLastChar; ++code) {
        const lcu::u32 index = code - kFirstChar;
        const lcu::u32 tx = index % kGlyphsPerRow;
        const lcu::u32 ty = index / kGlyphsPerRow;
        const auto glyph = generate_glyph_pixels(static_cast<char>(code));

        for (lcu::u32 y = 0; y < kGlyphHeight; ++y) {
            for (lcu::u32 x = 0; x < kGlyphWidth; ++x) {
                const lcu::usize glyph_offset = (static_cast<lcu::usize>(y) * kGlyphWidth + x) * 4;
                const lcu::usize atlas_x = tx * kGlyphWidth + x;
                const lcu::usize atlas_y = ty * kGlyphHeight + y;
                const lcu::usize atlas_offset = (atlas_y * kFontAtlasWidth + atlas_x) * 4;
                ASSERT_EQ(atlas[atlas_offset + 0], glyph[glyph_offset + 0]) << "char " << code;
                ASSERT_EQ(atlas[atlas_offset + 3], glyph[glyph_offset + 3]) << "char " << code;
            }
        }
    }
}
