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

// One preset's 4 flat material colors - hair, exposed skin (face/
// hands), shirt (torso + both arms, long-sleeved), pants (both legs).
// The one flat material color per region stays a real, deliberately
// simple choice (brief section 58.4 asks for "Hautfarbe, blaue Hose,
// grünes Shirt, braune Haare", not per-pixel detail) - Phase 62 only
// adds more palettes of the same 4 colors, not new per-pixel pattern
// logic.
struct SkinPalette {
    std::array<u8, 4> hair;
    std::array<u8, 4> skin_tone;
    std::array<u8, 4> shirt;
    std::array<u8, 4> pants;
};

// Steve (Phase 58's own default), Alex (real MC's own second default
// skin - strawberry-blonde hair, paler skin, lime shirt, tan pants),
// two further color variants (Red/Cyan - same Steve-like skin tone and
// hair, different shirt/pants colors, the brief's own "2 Farbvarianten"
// literally just needing two more colors), and a Ninja skin (all-black
// hair/shirt/pants, dark skin_tone so the whole head reads as a
// covered mask rather than an exposed face - the same 4-region split,
// just every material darkened, no new region carve-out needed).
constexpr std::array<SkinPalette, static_cast<u32>(SkinPreset::Count)> kPalettes{{
    /* Steve */ {{90, 60, 35, 255}, {225, 170, 130, 255}, {60, 140, 60, 255}, {50, 70, 150, 255}},
    /* Alex  */ {{190, 110, 60, 255}, {235, 190, 160, 255}, {60, 170, 130, 255}, {120, 90, 60, 255}},
    /* Red   */ {{40, 30, 25, 255}, {210, 160, 120, 255}, {170, 40, 40, 255}, {60, 60, 65, 255}},
    /* Cyan  */ {{40, 30, 25, 255}, {210, 160, 120, 255}, {40, 150, 170, 255}, {35, 55, 90, 255}},
    /* Ninja */ {{15, 15, 18, 255}, {45, 45, 50, 255}, {20, 20, 24, 255}, {15, 15, 18, 255}},
}};

constexpr const std::array<u8, 4>& material_for(const SkinPalette& palette, SkinRegion region) {
    switch (region) {
        case SkinRegion::HeadTop:
        case SkinRegion::HeadBack:
            return palette.hair;
        case SkinRegion::HeadBottom:
        case SkinRegion::HeadRight:
        case SkinRegion::HeadFront:
        case SkinRegion::HeadLeft:
            return palette.skin_tone;
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
            return palette.pants;
        default:
            // Torso + both arms (long-sleeved shirt covers the whole arm).
            return palette.shirt;
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

SkinPixelRect skin_pixel_rect(SkinRegion region) {
    LCU_ASSERT(region != SkinRegion::Count);
    const SkinRect& rect = kRegionRects[static_cast<u32>(region)];
    return {rect.x, rect.y, rect.w, rect.h};
}

std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> generate_skin_pixels(SkinPreset preset) {
    LCU_ASSERT(preset != SkinPreset::Count);
    const SkinPalette& palette = kPalettes[static_cast<u32>(preset)];

    std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> pixels{};
    pixels.fill(0);

    for (u32 i = 0; i < static_cast<u32>(SkinRegion::Count); ++i) {
        const auto region = static_cast<SkinRegion>(i);
        const SkinRect& rect = kRegionRects[i];
        const std::array<u8, 4>& color = material_for(palette, region);

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

std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> generate_default_skin_pixels() {
    return generate_skin_pixels(SkinPreset::Steve);
}

const char* skin_preset_name(SkinPreset preset) {
    switch (preset) {
        case SkinPreset::Steve:
            return "Steve";
        case SkinPreset::Alex:
            return "Alex";
        case SkinPreset::Red:
            return "Red";
        case SkinPreset::Cyan:
            return "Cyan";
        case SkinPreset::Ninja:
            return "Ninja";
        default:
            LCU_ASSERT(false && "unreachable SkinPreset");
            return "";
    }
}

bool parse_skin_preset_name(std::string_view name, SkinPreset& out) {
    for (u32 i = 0; i < static_cast<u32>(SkinPreset::Count); ++i) {
        const auto preset = static_cast<SkinPreset>(i);
        if (name == skin_preset_name(preset)) {
            out = preset;
            return true;
        }
    }
    return false;
}

}  // namespace lcu::assets
