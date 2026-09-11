#pragma once

#include <array>
#include <vector>

#include "lcu/core/types.h"

namespace lcu::assets {

// Real bitmap-font atlas (Phase 57, brief section "Font-Atlas + Real
// Text-Renderer"): a separate, own-design procedurally-generated
// monospace bitmap font, ASCII 32-126 (95 printable characters), one
// glyph per 6x8-pixel cell (a 5x7-pixel glyph plus 1px right/bottom
// spacing, matching a real monospace-font convention). Deliberately a
// SEPARATE atlas from engine/assets::texture_atlas's own 256x256 block
// atlas (own texture, own sampler slot - see
// engine/rendering::Renderer::flush_ui_quads/submit_text_glyph_quad) -
// text and block/item textures are conceptually unrelated content, and
// packing them into one shared atlas would have coupled two things
// that change on totally different schedules (adding a block texture
// vs. adding a font glyph) for no real benefit. Pure logic, no bgfx
// dependency, same "testable/tested in both bgfx and non-bgfx configs"
// discipline as texture_atlas.h.
constexpr u32 kGlyphWidth = 6;   // 5 real pixels + 1px right spacing.
constexpr u32 kGlyphHeight = 8;  // 7 real pixels + 1px bottom spacing.
constexpr u32 kFirstChar = 32;   // ' ' (space).
constexpr u32 kLastChar = 126;   // '~'.
constexpr u32 kGlyphCount = kLastChar - kFirstChar + 1;  // 95.
constexpr u32 kGlyphsPerRow = 16;
constexpr u32 kFontAtlasRows = (kGlyphCount + kGlyphsPerRow - 1) / kGlyphsPerRow;  // 6 (96 cells, 1 spare/blank).
constexpr u32 kFontAtlasCellSlots = kGlyphsPerRow * kFontAtlasRows;                // 96.
constexpr u32 kFontAtlasWidth = kGlyphsPerRow * kGlyphWidth;                       // 96.
constexpr u32 kFontAtlasHeight = kFontAtlasRows * kGlyphHeight;                    // 48.

// Same anti-bleed technique as texture_atlas.h's own kTileInsetTexels -
// a half-texel inward UV inset instead of literal padding pixels
// between glyph cells, for the identical reason (a fixed-size atlas
// with no spare pixel budget for a literal border - see DECISIONS.md).
constexpr f32 kGlyphInsetTexels = 0.5f;

// One glyph's real sample rectangle in normalized (0..1) font-atlas UV
// space, already inset by kGlyphInsetTexels on every edge.
struct GlyphUvRange {
    f32 u0 = 0.0f;
    f32 v0 = 0.0f;
    f32 u1 = 0.0f;
    f32 v1 = 0.0f;
};

// Returns character `c`'s real UV rect within the font atlas. A
// character outside the real [kFirstChar, kLastChar] range this font
// covers (e.g. a stray control byte) falls back to '?' - an honest,
// always-renders-something choice rather than an assert/crash, since
// display text can plausibly contain content this codebase didn't
// generate itself (e.g. a player-typed value, once real text input
// exists). Glyph index increases left-to-right then top-to-bottom
// (`tx = index % kGlyphsPerRow`, `ty = index / kGlyphsPerRow`) - the
// same row-major order build_font_atlas_pixels() fills the atlas pixel
// buffer in, matching texture_atlas.h's own tile_uv_range convention.
GlyphUvRange glyph_uv_range(char c);

// One glyph's real 6x8 RGBA8 pixel buffer (kGlyphWidth * kGlyphHeight *
// 4 bytes), deterministic and stateless - same character in, same
// bytes out, always, matching engine/assets::procedural_textures'
// (Phase 54) own reproducibility ethos, even though this generator
// needs no noise/RNG at all (a crisp bitmap font, not a painterly
// texture). "On" pixels are opaque white (255,255,255,255) so the real
// glyph can be tinted to any text color at render time (see fs_ui2d.sc's
// real per-vertex tint-by-color mode, engine/rendering::Renderer::
// submit_text_glyph_quad); "off" pixels (including the whole 1px right/
// bottom spacing border) are fully transparent (0,0,0,0). Falls back to
// '?' for a character outside [kFirstChar, kLastChar], same as
// glyph_uv_range.
using GlyphPixels = std::array<u8, kGlyphWidth * kGlyphHeight * 4>;
GlyphPixels generate_glyph_pixels(char c);

// Packs every real glyph (kFirstChar..kLastChar) into one flat
// kFontAtlasWidth x kFontAtlasHeight RGBA8 buffer, in the same
// row-major slot order glyph_uv_range() reads back - mirrors
// engine/assets::build_block_atlas_pixels()'s own structure exactly.
// The trailing spare cell (index 95, kFontAtlasCellSlots - kGlyphCount
// = 1) stays fully transparent - never addressed by glyph_uv_range()
// since no character maps to it, kept only because 95 doesn't evenly
// divide kGlyphsPerRow.
std::vector<u8> build_font_atlas_pixels();

}  // namespace lcu::assets
