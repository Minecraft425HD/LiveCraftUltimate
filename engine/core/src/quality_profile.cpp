#include "lcu/core/quality_profile.h"

namespace lcu::core {

// Phase 37: re-centered around worldgen::kSeaLevel (world Y 0) instead
// of the old always-positive-terrain ranges (min_chunk_y was never
// negative before this phase, back when terrain_height() itself was
// always positive too - see worldgen.cpp). Desktop keeps the exact
// same total chunk count as before (36 = 3x3 columns x 4 layers,
// unchanged) - just shifted down one chunk_y layer so it actually
// straddles sea level instead of sitting entirely above where
// terrain (and now water) can go.
ChunkLoadSettings chunk_load_settings_for(QualityProfile profile) {
    switch (profile) {
        case QualityProfile::MobileLow:
            // A single column, single layer straddling sea level
            // itself (world Y 0-15) - the minimal "show something real
            // near the player" slice this tier has always used.
            return ChunkLoadSettings{.radius_xz = 0, .min_chunk_y = 0, .max_chunk_y = 0};
        case QualityProfile::MobileMedium:
            return ChunkLoadSettings{.radius_xz = 1, .min_chunk_y = -1, .max_chunk_y = 0};
        case QualityProfile::MobileHigh:
            return ChunkLoadSettings{.radius_xz = 1, .min_chunk_y = -1, .max_chunk_y = 1};
        case QualityProfile::Desktop:
            return ChunkLoadSettings{.radius_xz = 1, .min_chunk_y = -1, .max_chunk_y = 2};
    }
    return ChunkLoadSettings{};
}

std::optional<QualityProfile> parse_quality_profile(std::string_view name) {
    if (name == "mobile_low") {
        return QualityProfile::MobileLow;
    }
    if (name == "mobile_medium") {
        return QualityProfile::MobileMedium;
    }
    if (name == "mobile_high") {
        return QualityProfile::MobileHigh;
    }
    if (name == "desktop") {
        return QualityProfile::Desktop;
    }
    return std::nullopt;
}

}  // namespace lcu::core
