#pragma once

#include <optional>
#include <string_view>

#include "lcu/core/types.h"

namespace lcu::core {

// Device performance tier (brief section 60's MOBILE_LOW/MEDIUM/HIGH,
// plus Desktop for this project's primary dev/server target). Nothing in
// this enum is mobile-specific by construction - it's just "how much
// world to stream" - but the concrete numbers below are sized for the
// gap between a constrained mobile GPU/battery budget and a desktop.
enum class QualityProfile : u8 {
    MobileLow,
    MobileMedium,
    MobileHigh,
    Desktop,
};

// The one real consumer this phase has: how big an area VoxelClient/
// VoxelServer load around spawn at startup (see kLoadRadiusXZ/kMinChunkY/
// kMaxChunkY in their main.cpp). More profile-driven settings (render
// distance once World::update_streaming has a real per-frame caller,
// texture/shadow quality once those exist) are added once something
// downstream actually reads them - not speculatively now (brief section
// 98).
struct ChunkLoadSettings {
    i32 radius_xz = 1;
    i32 min_chunk_y = 0;
    i32 max_chunk_y = 3;
};

// Desktop matches this project's existing hardcoded defaults exactly (no
// behavior change for anyone not opting into a mobile profile). Each
// Mobile tier trims both the horizontal radius and the vertical chunk
// span - a real reduction in loaded/meshed/lit chunk count, not just a
// label: MobileHigh drops one vertical layer (27 vs. Desktop's 36
// chunks), MobileMedium also narrows the vertical band (18 chunks),
// MobileLow drops to a single column (1 chunk) for the most constrained
// devices.
ChunkLoadSettings chunk_load_settings_for(QualityProfile profile);

// Parses the LCU_QUALITY_PROFILE env var's accepted values
// ("mobile_low"/"mobile_medium"/"mobile_high"/"desktop", case-sensitive).
// Returns std::nullopt for anything else - the caller decides the
// fallback (see client/main.cpp/server/main.cpp), so this stays free of
// getenv itself and is fully unit-testable on a plain string.
std::optional<QualityProfile> parse_quality_profile(std::string_view name);

}  // namespace lcu::core
