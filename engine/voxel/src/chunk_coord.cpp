#include "lcu/voxel/chunk_coord.h"

namespace lcu::voxel {

namespace {

// C++'s `/` and `%` truncate toward zero, which puts negative world
// coordinates in the wrong chunk (e.g. -1 / 16 == 0, not -1). These
// implement floor division/modulo instead.
i64 floor_div(i64 value, i64 divisor) {
    const i64 quotient = value / divisor;
    const i64 remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        return quotient - 1;
    }
    return quotient;
}

i64 floor_mod(i64 value, i64 divisor) {
    const i64 remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        return remainder + divisor;
    }
    return remainder;
}

}  // namespace

ChunkAndLocal world_to_chunk_and_local(BlockWorldCoord world, u32 edge_length) {
    const i64 edge = static_cast<i64>(edge_length);

    ChunkAndLocal result;
    result.chunk.x = static_cast<i32>(floor_div(world.x, edge));
    result.chunk.y = static_cast<i32>(floor_div(world.y, edge));
    result.chunk.z = static_cast<i32>(floor_div(world.z, edge));

    result.local.x = static_cast<u32>(floor_mod(world.x, edge));
    result.local.y = static_cast<u32>(floor_mod(world.y, edge));
    result.local.z = static_cast<u32>(floor_mod(world.z, edge));

    return result;
}

}  // namespace lcu::voxel
