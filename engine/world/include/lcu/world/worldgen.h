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
// `solid_block` below the height, air above. Single block type only -
// no surface/subsurface distinction (dirt/grass over stone) since no
// such blocks are registered anywhere real yet either.
void generate_terrain_chunk(voxel::Chunk& chunk, voxel::ChunkCoord coord, u32 seed, voxel::BlockId solid_block);

}  // namespace lcu::world::worldgen
