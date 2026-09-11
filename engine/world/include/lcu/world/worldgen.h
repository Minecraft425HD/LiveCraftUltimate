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
// up to kSeaLevel by generate_terrain_chunk below). Climate/biome/
// caves/ores/structures/vegetation/decoration are later pipeline
// stages (brief section 21) and are not implemented - nothing consumes
// biome data yet (no biomes registered anywhere), so building that now
// would be speculative (see DECISIONS.md).
i32 terrain_height(u32 seed, i32 world_x, i32 world_z);

// Fills `chunk` (at chunk coordinate `coord`) from terrain_height():
// `surface_block` at the topmost solid layer, `subsurface_block` for
// the next kSubsurfaceDepth layers beneath it, `stone_block` for
// everything deeper. Above the terrain height: `water_block` for any
// cell at or below kSeaLevel (Phase 37 - a below-sea-level column's
// "hole" between its terrain and the sea surface), air everywhere
// else (unchanged from before this phase for any column whose terrain
// is already at or above sea level - a real behavior no-op for dry
// land). A real surface/subsurface distinction (brief section 21's
// "climate/terrain" stage growing a grass-over-dirt-over-stone
// column, not a single block type filling everything below the
// height) - still no climate/biome variation (every column uses the
// same three land block ids regardless of position - see
// DECISIONS.md), and still no caves/ores/structures/vegetation (later
// brief section 21 pipeline stages, not implemented).
void generate_terrain_chunk(voxel::Chunk& chunk, voxel::ChunkCoord coord, u32 seed, voxel::BlockId surface_block,
                             voxel::BlockId subsurface_block, voxel::BlockId stone_block,
                             voxel::BlockId water_block);

}  // namespace lcu::world::worldgen
