#pragma once

#include "lcu/core/types.h"

namespace lcu::assets {

// Real texture-atlas geometry (Phase 53, brief section "Texture-Atlas-
// Pipeline"): a single flat 256x256 RGBA atlas, packed as a fixed
// 16x16 grid of 16x16-pixel tiles - not a texture array, not a 3D
// texture (the directive is explicit about this). Pure logic, no bgfx
// dependency at all - engine/rendering::Renderer::create_texture_from_
// pixels is the only place that actually touches a GPU texture; this
// header only computes where in that flat pixel buffer/UV space a
// given tile lives, so it's testable (and tested) in both the bgfx and
// non-bgfx build configs, same as every other pure-logic module in
// this project.
constexpr u32 kAtlasSize = 256;
constexpr u32 kTileSize = 16;
constexpr u32 kTilesPerRow = kAtlasSize / kTileSize;         // 16
constexpr u32 kMaxTiles = kTilesPerRow * kTilesPerRow;       // 256

// Real anti-bleed technique for a NEAREST-filtered atlas (Phase 53.1's
// own "Padding verhindert Neighbouring-tile-Bleeding" requirement),
// implemented as a half-texel UV inset rather than literal padding
// pixels between tiles: the directive's own atlas size (256x256 = 16x16
// tiles of exactly 16x16 pixels) leaves no spare pixels to spend on a
// literal 1px border without either shrinking every tile's real content
// to 14x14 or growing the atlas past its own stated fixed size - both
// real, avoidable costs. A half-texel inward inset from each tile's own
// edge achieves the identical goal (nearest-filter sampling never reads
// a neighboring tile's pixel) at zero pixel-budget cost, since NEAREST
// filtering (unlike bilinear) only ever risks bleeding from floating-
// point rounding landing exactly on a tile boundary, which an inset
// this small already fully prevents - see DECISIONS.md for the full
// reasoning and the literal-padding alternative this deliberately
// wasn't picked.
constexpr f32 kTileInsetTexels = 0.5f;

// One tile's real sample rectangle in normalized (0..1) atlas UV space,
// already inset by kTileInsetTexels on every edge.
struct TileUvRange {
    f32 u0 = 0.0f;
    f32 v0 = 0.0f;
    f32 u1 = 0.0f;
    f32 v1 = 0.0f;
};

// Returns tile `tile_index`'s real UV rect (asserts tile_index <
// kMaxTiles - always checked, even in Release, matching this project's
// LCU_ASSERT convention for "caller passed a genuinely out-of-range
// index" bugs). Tile 0 sits at the atlas's top-left grid cell (u,v
// increasing right/down, matching this project's existing screen/UV
// convention elsewhere), tile index increases left-to-right then
// top-to-bottom (`tx = tile_index % kTilesPerRow`,
// `ty = tile_index / kTilesPerRow`) - the same row-major order
// engine/assets::procedural_textures (Phase 54) fills the atlas pixel
// buffer in, so a tile's index always means the same physical cell on
// both the CPU pixel-packing side and this UV-lookup side.
TileUvRange tile_uv_range(u32 tile_index);

}  // namespace lcu::assets
