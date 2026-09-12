#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "lcu/core/types.h"

namespace lcu::assets {

// Real procedurally-generated player-skin texture (Phase 58, brief
// section "Spielermodell + korrekte Größe"): a 64x64 RGBA8 texture in
// the real Minecraft "modern" skin UV layout (dual-arm/dual-leg, added
// in MC 1.8) - not a simplified/invented layout, so a real MC skin file
// (once Phase 62 adds upload) lines up with these exact same regions.
// Own texture, own sampler slot, separate from both the block/item
// atlas (Phase 53) and the font atlas (Phase 57) - matches this
// project's established "unrelated content, unrelated schedule -> its
// own texture" convention (see DECISIONS.md).
constexpr u32 kSkinWidth = 64;
constexpr u32 kSkinHeight = 64;

// Same anti-bleed idea as texture_atlas.h/font_atlas.h's own inset
// constants, sized down for this atlas's smallest real region (a 4x4
// pixel limb top/bottom face) so the inset never eats a meaningful
// fraction of a small region's own area.
constexpr f32 kSkinInsetTexels = 0.2f;

// One named region per real face of each of the 6 Minecraft body parts
// (head, torso, right/left arm, right/left leg) - 36 regions total,6
// per part (top/bottom/front/back/left/right), matching the real MC
// skin format's own per-part face layout exactly (see skin_texture.cpp
// for the literal pixel-rect table, transcribed from the real,
// publicly-documented MC skin UV layout - the directive's own six
// example coordinates, e.g. "Torso: 8x12 Pixel ab (20,20)", match this
// table exactly).
enum class SkinRegion : u32 {
    HeadTop,
    HeadBottom,
    HeadRight,
    HeadFront,
    HeadLeft,
    HeadBack,
    TorsoTop,
    TorsoBottom,
    TorsoRight,
    TorsoFront,
    TorsoLeft,
    TorsoBack,
    RightArmTop,
    RightArmBottom,
    RightArmRight,
    RightArmFront,
    RightArmLeft,
    RightArmBack,
    RightLegTop,
    RightLegBottom,
    RightLegRight,
    RightLegFront,
    RightLegLeft,
    RightLegBack,
    LeftLegTop,
    LeftLegBottom,
    LeftLegRight,
    LeftLegFront,
    LeftLegLeft,
    LeftLegBack,
    LeftArmTop,
    LeftArmBottom,
    LeftArmRight,
    LeftArmFront,
    LeftArmLeft,
    LeftArmBack,
    Count,
};

// One region's real sample rectangle in normalized (0..1) skin-texture
// UV space, already inset by kSkinInsetTexels on every edge.
struct SkinUvRange {
    f32 u0 = 0.0f;
    f32 v0 = 0.0f;
    f32 u1 = 0.0f;
    f32 v1 = 0.0f;
};

// Returns `region`'s real UV rect. LCU_ASSERTs region != Count (a
// genuinely out-of-range value is a caller bug, matching this project's
// existing tile_uv_range()/glyph_uv_range() convention).
SkinUvRange skin_uv_range(SkinRegion region);

// `region`'s real, un-inset pixel rectangle in the 64x64 skin texture
// (top-left origin) - the same table skin_uv_range() itself reads from,
// exposed directly for SkinCatalog's own real pixel-level work (legacy
// 64x32 skin-file expansion, see skin_catalog.cpp) that needs literal
// pixel coordinates, not a sampling UV rect.
struct SkinPixelRect {
    u32 x = 0;
    u32 y = 0;
    u32 w = 0;
    u32 h = 0;
};
SkinPixelRect skin_pixel_rect(SkinRegion region);

// The single, real, procedurally-generated default skin (Phase 58) - a
// Steve-like character (skin-tone head/hands, brown hair, green shirt,
// blue pants), deterministic (same bytes every call, no RNG state,
// matching every other procedural-texture generator in this project).
// Phase 62 adds several more named skins plus real file upload; this
// stays the one always-available fallback those build on. Equivalent
// to generate_skin_pixels(SkinPreset::Steve) below - kept as its own
// function so every Phase 58 caller/test keeps working unchanged.
std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> generate_default_skin_pixels();

// Real, always-available procedural skins (Phase 62, brief section
// 62.1's "3-5 Skins: Steve, Alex, 2 Farbvarianten, ein Ninja-Skin") -
// each one a flat 4-material palette (hair/skin-tone/shirt/pants)
// painted across the same real MC region layout `kRegionRects` already
// uses, exactly like the Phase 58 default (see skin_texture.cpp's own
// `material_for`/`kPalettes`) - no new per-pixel pattern logic needed,
// only the 4 colors change per preset. `Steve` matches
// generate_default_skin_pixels() byte-for-byte.
enum class SkinPreset : u32 {
    Steve,
    Alex,
    Red,
    Cyan,
    Ninja,
    Count,
};

// Deterministic, same reasoning as generate_default_skin_pixels().
std::array<u8, static_cast<usize>(kSkinWidth) * kSkinHeight * 4> generate_skin_pixels(SkinPreset preset);

// Stable, human-readable names (also this preset's real SkinCatalog
// entry name and its real options.txt `skin=<name>` persisted value -
// see skin_catalog.h) - "Steve"/"Alex"/"Red"/"Cyan"/"Ninja".
const char* skin_preset_name(SkinPreset preset);

// Reverse of skin_preset_name() - a case-sensitive exact match against
// one of the 5 real names above. Returns false (leaving `out`
// unchanged) for anything else, including a custom uploaded skin's own
// name - SkinCatalog is the one real place that resolves a name that
// could be either a builtin preset or a custom file.
bool parse_skin_preset_name(std::string_view name, SkinPreset& out);

}  // namespace lcu::assets
