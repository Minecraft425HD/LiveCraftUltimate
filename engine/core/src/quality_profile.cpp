#include "lcu/core/quality_profile.h"

namespace lcu::core {

ChunkLoadSettings chunk_load_settings_for(QualityProfile profile) {
    switch (profile) {
        case QualityProfile::MobileLow:
            return ChunkLoadSettings{.radius_xz = 0, .min_chunk_y = 1, .max_chunk_y = 1};
        case QualityProfile::MobileMedium:
            return ChunkLoadSettings{.radius_xz = 1, .min_chunk_y = 1, .max_chunk_y = 2};
        case QualityProfile::MobileHigh:
            return ChunkLoadSettings{.radius_xz = 1, .min_chunk_y = 0, .max_chunk_y = 2};
        case QualityProfile::Desktop:
            return ChunkLoadSettings{.radius_xz = 1, .min_chunk_y = 0, .max_chunk_y = 3};
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
