#include "lcu/assets/skin_texture.h"

#include "lcu/core/assert.h"

namespace lcu::assets {

namespace {

struct SkinRect {
    u32 x, y, w, h;
};

// Real Minecraft "modern" (64x64, dual-arm/dual-leg) skin UV layout,
// transcribed from the format's own public documentation - not
// invented. Indexed in exactly SkinRegion's own declaration order.
constexpr SkinRect kRegionRects[static_cast<u32>(SkinRegion::Count)] = {
    {8, 0, 8, 8},    // HeadTop
    {16, 0, 8, 8},   // HeadBottom
    {0, 8, 8, 8},    // HeadRight
    {8, 8, 8, 8},    // HeadFront
    {16, 8, 8, 8},   // HeadLeft
    {24, 8, 8, 8},   // HeadBack
    {20, 16, 8, 4},  // TorsoTop
    {28, 16, 8, 4},  // TorsoBottom
    {16, 20, 4, 12}, // TorsoRight
    {20, 20, 8, 12}, // TorsoFront
    {28, 20, 4, 12}, // TorsoLeft
    {32, 20, 8, 12}, // TorsoBack
    {44, 16, 4, 4},  // RightArmTop
    {48, 16, 4, 4},  // RightArmBottom
    {40, 20, 4, 12}, // RightArmRight
    {44, 20, 4, 12}, // RightArmFront
    {48, 20, 4, 12}, // RightArmLeft
    {52, 20, 4, 12}, // RightArmBack
    {4, 16, 4, 4},   // RightLegTop
    {8, 16, 4, 4},   // RightLegBottom
    {0, 20, 4, 12},  // RightLegRight
    {4, 20, 4, 12},  // RightLegFront
    {8, 20, 4, 12},  // RightLegLeft
    {12, 20, 4, 12}, // RightLegBack
    {20, 48, 4, 4},  // LeftLegTop
    {24, 48, 4, 4},  // LeftLegBottom
    {16, 52, 4, 12}, // LeftLegRight
    {20, 52, 4, 12}, // LeftLegFront
    {24, 52, 4, 12}, // LeftLegLeft
    {28, 52, 4, 12}, // LeftLegBack
    {36, 48, 4, 4},  // LeftArmTop
    {40, 48, 4, 4},  // LeftArmBottom
    {32, 52, 4, 12}, // LeftArmRight
    {36, 52, 4, 12}, // LeftArmFront
    {40, 52, 4, 12}, // LeftArmLeft
    {44, 52, 4, 12}, // LeftArmBack
};

constexpr std::array<u8, 4> kSkinTone{225, 170, 130, 255};
constexpr std::array<u8, 4> kHairBrown{90, 60, 35, 255};
constexpr std::array<u8, 4> kShirtGreen{60, 140, 60, 255};
constexpr std::array<u8, 4> kPantsBlue{50, 70, 150, 255};

// The one flat material color painted across each region's own full
// rect - a real, deliberately simple first pass (brief section 58.4
// asks for "Hautfarbe, blaue Hose, grünes Shirt, braune Haare", not
// per-pixel detail); Phase 62's own additional skins can add real
// pattern/noise later if wanted, this default stays flat and legible.
constexpr std::array<u8, 4> material_for(SkinRegion region) {
    switch (region) {
        case SkinRegion::HeadTop:
        case SkinRegion::HeadBack:
            return kHairBrown;
        case SkinRegion::HeadBottom:
        case SkinRegion::HeadRight:
        case SkinRegion::HeadFront:
        case SkinRegion::HeadLeft:
            return kSkinTone;
        case SkinRegion::RightLegTop:
        case SkinRegion::RightLegBottom:
        case SkinRegion::RightLegRight:
        case SkinRegion::RightLegFront:
        case SkinRegion::RightLegLeft:
        case SkinRegion::RightLegBack:
        case SkinRegion::LeftLegTop:
        case SkinRegion::LeftLegBottom:
        case SkinRegion::LeftLegRight:
        case SkinRegion::LeftLegFront:
        case SkinRegion::LeftLegLeft:
        case SkinRegion::LeftLegBack:
            return kPantsBlue;
        default:
            // Torso + both arms (long-sleeved shirt covers the whole arm).
            return kShirtGreen;
    }
}

}  // namespace

SkinUvRange skin_uv_range(SkinRegion region) {
    LCU_ASSERT(region != SkinRegion::Count);
    const SkinRect& rect = kRegionRects[static_cast<u32>(region)];

    const f32 atlas_w = static_cast<f32>(kSkinWidth);
    const f32 atlas_h = static_cast<f32>(kSkinHeight);

    SkinUvRange range;
    range.u0 = (static_cast<f32>(rect.x) + kSkinInsetTexels) / atlas_w;
    range.v0 = (static_cast<f32>(rect.y) + kSkinInsetTexels) / atlas_h;
    range.u1 = (static_cast<f32>(rect.x + rect.w) - kSkinInsetTexels) / atlas_w;
    range.v1 = (static_cast<f32>(rect.y + rect.h) - kSkinInsetTexels) / atlas_h;
    return range;
}

std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> generate_default_skin_pixels() {
    std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> pixels{};
    pixels.fill(0);

    for (u32 i = 0; i < static_cast<u32>(SkinRegion::Count); ++i) {
        const auto region = static_cast<SkinRegion>(i);
        const SkinRect& rect = kRegionRects[i];
        const std::array<u8, 4> color = material_for(region);

        for (u32 y = rect.y; y < rect.y + rect.h; ++y) {
            for (u32 x = rect.x; x < rect.x + rect.w; ++x) {
                const usize offset = (static_cast<usize>(y) * kSkinWidth + x) * 4;
                pixels[offset + 0] = color[0];
                pixels[offset + 1] = color[1];
                pixels[offset + 2] = color[2];
                pixels[offset + 3] = color[3];
            }
        }
    }

    return pixels;
}

}  // namespace lcu::assets
