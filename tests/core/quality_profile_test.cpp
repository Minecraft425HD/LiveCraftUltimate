#include "lcu/core/quality_profile.h"

#include <gtest/gtest.h>

namespace lcu::core {
namespace {

TEST(QualityProfile, DesktopStraddlesSeaLevelWithTheSameTotalChunkCountAsBefore) {
    // Phase 37 re-centered this around worldgen::kSeaLevel (0) - was
    // 1/0/3 (this project's old client/server hardcoded
    // kLoadRadiusXZ/kMinChunkY/kMaxChunkY, entirely above y=0, back
    // when terrain itself was always positive too), now 1/-1/2: same
    // radius, same total layer count (4), just shifted down by one
    // chunk_y so the loaded volume actually straddles sea level
    // instead of sitting entirely above where terrain/water can now
    // go. "Loaded 36 chunks" (radius 1 -> 3x3=9 columns, 4 layers)
    // still holds - the shift doesn't change the total.
    const ChunkLoadSettings settings = chunk_load_settings_for(QualityProfile::Desktop);
    EXPECT_EQ(settings.radius_xz, 1);
    EXPECT_EQ(settings.min_chunk_y, -1);
    EXPECT_EQ(settings.max_chunk_y, 2);
}

TEST(QualityProfile, MobileTiersLoadStrictlyFewerChunksThanDesktop) {
    auto chunk_count = [](const ChunkLoadSettings& s) {
        const i32 columns = (2 * s.radius_xz + 1) * (2 * s.radius_xz + 1);
        const i32 layers = s.max_chunk_y - s.min_chunk_y + 1;
        return columns * layers;
    };

    const i32 desktop_chunks = chunk_count(chunk_load_settings_for(QualityProfile::Desktop));
    const i32 high_chunks = chunk_count(chunk_load_settings_for(QualityProfile::MobileHigh));
    const i32 medium_chunks = chunk_count(chunk_load_settings_for(QualityProfile::MobileMedium));
    const i32 low_chunks = chunk_count(chunk_load_settings_for(QualityProfile::MobileLow));

    EXPECT_LT(high_chunks, desktop_chunks);
    EXPECT_LT(medium_chunks, high_chunks);
    EXPECT_LT(low_chunks, medium_chunks);
    EXPECT_EQ(low_chunks, 1);  // MobileLow: a single column, single layer.
}

TEST(ParseQualityProfile, ParsesAllFourAcceptedNames) {
    EXPECT_EQ(parse_quality_profile("mobile_low"), QualityProfile::MobileLow);
    EXPECT_EQ(parse_quality_profile("mobile_medium"), QualityProfile::MobileMedium);
    EXPECT_EQ(parse_quality_profile("mobile_high"), QualityProfile::MobileHigh);
    EXPECT_EQ(parse_quality_profile("desktop"), QualityProfile::Desktop);
}

TEST(ParseQualityProfile, RejectsUnrecognizedOrMistypedNames) {
    EXPECT_EQ(parse_quality_profile("Desktop"), std::nullopt);  // wrong case
    EXPECT_EQ(parse_quality_profile("ultra"), std::nullopt);
    EXPECT_EQ(parse_quality_profile(""), std::nullopt);
}

}  // namespace
}  // namespace lcu::core
