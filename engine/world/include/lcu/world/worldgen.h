#pragma once

#include "lcu/core/types.h"
#include "lcu/voxel/block_id.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/chunk_coord.h"

namespace lcu::world::worldgen {

// World Y of sea level (Phase 37, brief section 21's "sea level at
// y=0"). Real, not just a comment: terrain_height's own range is
// centered on this value (some columns land above it - dry land,
// hills - some below it - lake/ocean floor), and generate_terrain_
// chunk fills anything between a below-sea-level column's terrain and
// this Y with `water_block`. Exposed here (not just a worldgen.cpp
// implementation detail) since other code - spawn placement, a future
// "am I underwater" check - has a real reason to know it too.
constexpr i32 kSeaLevel = 0;

// Deterministic terrain height (world Y) at a given world X/Z column,
// for a given seed: pipeline stage "continental -> terrain" (brief
// section 21). Same (seed, world_x, world_z) always produces the same
// height - no hidden global state, no time-of-day/random-device input.
// Centered on kSeaLevel (Phase 37) - roughly half of all columns land
// above it (dry land), half below (lake/ocean floor, filled with water
// up to kSeaLevel by generate_terrain_chunk below). Caves/ores/
// structures/vegetation/decoration are later pipeline stages (brief
// section 21) and are not implemented - nothing consumes that data yet
// (see DECISIONS.md).
i32 terrain_height(u32 seed, i32 world_x, i32 world_z);

// Real climate/biome pipeline stage (Phase 39, brief section 21): a
// deliberately small, honest set - a temperature-only climate model
// (no separate humidity axis, no Whittaker-diagram-style biome table),
// not the full biome variety a shipped game would eventually want, but
// three real, distinct, surface-content-affecting categories rather
// than none. See DECISIONS.md for why this scope and not more.
enum class Biome {
    Snowy,   // cold - snow-capped surface
    Plains,  // temperate - grass, the pre-Phase-39 default everywhere
    Desert,  // hot - sand
};

// Deterministic biome (climate) at a given world X/Z column, for a
// given seed - same contract as terrain_height (pure, deterministic,
// no hidden state). Independent of terrain_height/elevation on
// purpose (brief section 21 lists "kontinental/terrain" - shape - and
// "climate/biome" - surface content - as separate pipeline stages);
// a real game might correlate them (colder at higher elevation), but
// that coupling is a deliberate future refinement, not assumed here.
Biome biome_at(u32 seed, i32 world_x, i32 world_z);

// The real land block ids each Biome maps to (Phase 39) - passed in by
// the caller (VoxelClient/VoxelServer each register their own blocks
// and build one of these), mirroring how surface/subsurface/stone/
// water block ids were already caller-supplied before this phase.
struct BiomeBlocks {
    voxel::BlockId plains_surface;
    voxel::BlockId plains_subsurface;
    voxel::BlockId desert_surface;
    voxel::BlockId desert_subsurface;
    voxel::BlockId snowy_surface;
    voxel::BlockId snowy_subsurface;
};

// Real cave-carving pipeline stage (Phase 40, brief section 21): a
// genuine 3D noise "tunnel" predicate - true where a solid,
// underground cell should be carved into open air instead of stone.
// Built from two independent 3D noise fields whose difference
// crossing near zero forms winding, tunnel-like voids (the classic
// "noise crevice" technique) rather than a single-threshold blob
// ("cheese cave") - a real, deliberately simple choice for a more
// tunnel-like result (see DECISIONS.md for the alternatives this
// passed over). `surface_height` is that column's own
// terrain_height() - the caller already has it (generate_terrain_
// chunk computes it once per column), no reason to recompute it here
// - used to keep caves a real minimum depth below the surface so they
// never punch daylight holes right at ground level.
bool is_cave(u32 seed, i32 world_x, i32 world_y, i32 world_z, i32 surface_height);

// Real ore pipeline stage (Phase 40): which ore, if any, replaces
// plain stone at a given world position. Deliberately small - two
// real, distinct ores, not the full variety a shipped game would
// eventually want (see DECISIONS.md, the same "honestly scoped, not
// the final variety" reasoning Phase 39's three biomes already
// established). `None` (the overwhelmingly common outcome, matching
// real ore rarity) means "no ore here, this cell stays plain stone".
enum class OreType {
    None,
    Coal,
    Iron,
};
OreType ore_at(u32 seed, i32 world_x, i32 world_y, i32 world_z);

// The real block ids OreType::Coal/Iron map to (Phase 40) - caller-
// supplied, the same pattern BiomeBlocks already established.
struct OreBlocks {
    voxel::BlockId coal_ore;
    voxel::BlockId iron_ore;
};

// Fills `chunk` (at chunk coordinate `coord`) from terrain_height()
// and biome_at(): the biome's own surface block at the topmost solid
// layer, that biome's subsurface block for the next kSubsurfaceDepth
// layers beneath it. Below that: `is_cave` carves real tunnels into
// open air (Phase 40); a cell that isn't carved and isn't `is_cave`
// checks `ore_at` next, substituting the matching block from
// `ore_blocks` for plain stone where an ore vein is present; anything
// left over is `stone_block`. Above the terrain height: `water_block`
// for any cell at or below kSeaLevel (Phase 37 - a below-sea-level
// column's "hole" between its terrain and the sea surface, unaffected
// by biome/caves/ores), air everywhere else. Still no structures/
// vegetation (later brief section 21 pipeline stages, not
// implemented).
void generate_terrain_chunk(voxel::Chunk& chunk, voxel::ChunkCoord coord, u32 seed, const BiomeBlocks& biome_blocks,
                             voxel::BlockId stone_block, voxel::BlockId water_block, const OreBlocks& ore_blocks);

}  // namespace lcu::world::worldgen
