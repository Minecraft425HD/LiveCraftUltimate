#pragma once

#include "lcu/core/types.h"
#include "lcu/voxel/block_id.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/chunk_coord.h"

namespace lcu::world::worldgen {

// Deterministic terrain height (world Y) at a given world X/Z column,
// for a given seed: pipeline stage "continental -> terrain" (brief
// section 21). Same (seed, world_x, world_z) always produces the same
// height - no hidden global state, no time-of-day/random-device input.
// Climate/biome/caves/ores/structures/vegetation/decoration are later
// pipeline stages (brief section 21) and are not implemented - nothing
// consumes biome data yet (no biomes registered anywhere), so building
// that now would be speculative (see DECISIONS.md).
i32 terrain_height(u32 seed, i32 world_x, i32 world_z);

// Fills `chunk` (at chunk coordinate `coord`) from terrain_height():
// `surface_block` at the topmost solid layer, `subsurface_block` for
// the next kSubsurfaceDepth layers beneath it, `stone_block` for
// everything deeper, air above the surface. A real surface/subsurface
// distinction (brief section 21's "climate/terrain" stage growing a
// grass-over-dirt-over-stone column, not a single block type filling
// everything below the height) - still no climate/biome variation
// (every column uses the same three block ids regardless of position -
// see DECISIONS.md), and still no caves/ores/structures/vegetation
// (later brief section 21 pipeline stages, not implemented).
void generate_terrain_chunk(voxel::Chunk& chunk, voxel::ChunkCoord coord, u32 seed, voxel::BlockId surface_block,
                             voxel::BlockId subsurface_block, voxel::BlockId stone_block);

}  // namespace lcu::world::worldgen
