#pragma once

#include <array>
#include <vector>

#include "lcu/assets/texture_atlas.h"
#include "lcu/core/types.h"

namespace lcu::assets {

// One tile's worth of RGBA8 pixels, row-major top-to-bottom - the same
// per-tile shape every generator function below returns and
// build_block_atlas_pixels() packs into the real 256x256 atlas buffer.
using TilePixels = std::array<u8, static_cast<usize>(kTileSize) * kTileSize * 4>;

// Real, fixed atlas tile slots (Phase 54.3's own "Block-ID -> Atlas-
// Slot" requirement, brief section on the texture-atlas pipeline) -
// every procedural texture below owns exactly one of these, referenced
// by BlockDefinition's real top/side/bottom texture fields (Phase 55)
// and ItemDefinition's own texture_index (Phase 56). A plain, stable
// enum rather than deriving slots from BlockId directly: several real
// blocks share one texture (grass's own underside IS the dirt texture,
// a crafting table's sides reuse the plank/wood texture), and items
// with no block (nothing here yet, but Phase 56's own icon slots don't
// need to line up with BlockId either) need slots too - a texture is
// its own real, independent piece of content, not 1:1 with a block.
enum class TileId : u32 {
    GrassTop = 0,
    GrassSide = 1,
    Dirt = 2,  // Also grass's own underside - real blocks reuse this
               // same tile index for both, they don't get two copies.
    Stone = 3,
    Sand = 4,
    Snow = 5,
    Water = 6,
    WoodSide = 7,
    WoodTop = 8,
    Leaves = 9,
    CoalOre = 10,
    IronOre = 11,
    Torch = 12,
    CraftingTableTop = 13,
    Cactus = 14,
    Compost = 15,
    Planks = 16,
    // Real break-progress "crack" overlay tiles (Phase 60.1) - 10 real
    // stages (0 = barely cracked, 9 = almost fully cracked), each a
    // transparent base with an increasing real coverage of dark crack
    // pixels (see generate_crack's own doc comment). Their own atlas
    // area within the SAME block/item atlas (brief 60.1's own "Eigener
    // Atlas-Bereich" reading) rather than a whole second texture/
    // sampler - unlike the font atlas (Phase 57) or skin texture (Phase
    // 58), these are still real block-face-adjacent overlay content
    // conceptually close to the rest of this atlas, and there's ample
    // spare atlas room (17 of 256 slots used before this phase).
    Crack0 = 17,
    Crack1 = 18,
    Crack2 = 19,
    Crack3 = 20,
    Crack4 = 21,
    Crack5 = 22,
    Crack6 = 23,
    Crack7 = 24,
    Crack8 = 25,
    Crack9 = 26,
    Count = 27,
};

// One real, deterministic (fixed-seed-per-texture, see each .cpp
// function's own doc comment) 16x16 generator per real texture this
// project ships (Phase 54.2's own MC-style palette/pattern spec).
// Every call with the same TileId returns byte-identical pixels - no
// hidden global RNG state, no time-based seeding - the same "real,
// reproducible content, not randomly different every run" property
// this project's worldgen (`kWorldSeed`) already established.
TilePixels generate_grass_top();
TilePixels generate_grass_side();
TilePixels generate_dirt();
TilePixels generate_stone();
TilePixels generate_sand();
TilePixels generate_snow();
TilePixels generate_water();
TilePixels generate_wood_side();
TilePixels generate_wood_top();
TilePixels generate_leaves();
TilePixels generate_coal_ore();
TilePixels generate_iron_ore();
TilePixels generate_torch();
TilePixels generate_crafting_table_top();
TilePixels generate_cactus();
TilePixels generate_compost();
TilePixels generate_planks();

// Real break-progress crack overlay (Phase 60.1) - `stage` 0-9, an
// increasingly dense real coverage of dark crack pixels over a
// transparent base (see the .cpp for the exact deterministic
// generation). `generate_tile(TileId::Crack0 + stage)` reaches the
// same real pixels through the general dispatcher below.
TilePixels generate_crack(u32 stage);

// Returns the same TilePixels generate_* above would for `tile`
// (dispatches on TileId) - the one real call site build_block_atlas_
// pixels below uses, and the one real call site a unit test needs to
// check "every tile is reachable" without a 17-way if/else of its own.
TilePixels generate_tile(TileId tile);

// Generates every real texture above and packs them into one real
// kAtlasSize*kAtlasSize*4-byte RGBA8 buffer (row-major top-to-bottom,
// the same layout Renderer::create_texture_from_pixels expects), each
// at its own fixed TileId slot (tile_uv_range(static_cast<u32>(tile))
// is where the chunk/UI shaders will actually sample it from). Every
// slot from 0 to kMaxTiles-1 beyond TileId::Count stays black/
// transparent (zero-initialized) - real, unused atlas space, not
// garbage.
std::vector<u8> build_block_atlas_pixels();

}  // namespace lcu::assets
