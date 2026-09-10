#include "lcu/world/worldgen.h"

#include <cmath>

namespace lcu::world::worldgen {

namespace {

// Integer hash (a Squirrel3/wang-hash style avalanche), used as the
// noise lattice's source of "randomness". Deterministic and pure: same
// inputs always produce the same output, no global RNG state.
u32 hash2d(u32 seed, i32 x, i32 z) {
    u32 h = seed;
    h ^= static_cast<u32>(x) * 0x27d4eb2du;
    h ^= static_cast<u32>(z) * 0x165667b1u;
    h ^= h >> 15;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

// Lattice value in [0, 1) at integer coordinates.
f32 lattice_value(u32 seed, i32 x, i32 z) {
    return static_cast<f32>(hash2d(seed, x, z) & 0x00FFFFFFu) / static_cast<f32>(0x01000000u);
}

f32 smoothstep(f32 t) { return t * t * (3.0f - 2.0f * t); }

f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

// Bilinearly-interpolated, smoothed value noise in [0, 1) at continuous
// (x, z).
f32 smooth_noise(u32 seed, f32 x, f32 z) {
    const i32 x0 = static_cast<i32>(std::floor(x));
    const i32 z0 = static_cast<i32>(std::floor(z));
    const f32 fx = x - static_cast<f32>(x0);
    const f32 fz = z - static_cast<f32>(z0);

    const f32 v00 = lattice_value(seed, x0, z0);
    const f32 v10 = lattice_value(seed, x0 + 1, z0);
    const f32 v01 = lattice_value(seed, x0, z0 + 1);
    const f32 v11 = lattice_value(seed, x0 + 1, z0 + 1);

    const f32 sx = smoothstep(fx);
    const f32 sz = smoothstep(fz);
    return lerp(lerp(v00, v10, sx), lerp(v01, v11, sx), sz);
}

// 4-octave fractal sum, normalized back to [0, 1).
f32 fractal_noise(u32 seed, f32 x, f32 z) {
    f32 total = 0.0f;
    f32 amplitude = 1.0f;
    f32 frequency = 1.0f;
    f32 amplitude_sum = 0.0f;

    for (int octave = 0; octave < 4; ++octave) {
        // Perturb the seed per octave so octaves sample independent
        // lattices instead of the same one at different frequencies
        // (which would just look like one octave with visible axis
        // alignment artifacts).
        total += smooth_noise(seed + static_cast<u32>(octave) * 101u, x * frequency, z * frequency) * amplitude;
        amplitude_sum += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return total / amplitude_sum;
}

constexpr i32 kBaseHeight = 32;
constexpr i32 kHeightVariation = 24;
constexpr f32 kNoiseScale = 0.01f;

// How many layers of `subsurface_block` sit directly beneath the
// surface layer before `stone_block` takes over - matches the common
// voxel-game convention this genre's players already expect (a grass
// top, a few blocks of dirt, then stone), not tuned against anything
// more specific since there's no other block variation yet to balance
// it against.
constexpr i32 kSubsurfaceDepth = 3;

}  // namespace

i32 terrain_height(u32 seed, i32 world_x, i32 world_z) {
    const f32 n = fractal_noise(seed, static_cast<f32>(world_x) * kNoiseScale, static_cast<f32>(world_z) * kNoiseScale);
    return kBaseHeight + static_cast<i32>((n - 0.5f) * 2.0f * static_cast<f32>(kHeightVariation));
}

void generate_terrain_chunk(voxel::Chunk& chunk, voxel::ChunkCoord coord, u32 seed, voxel::BlockId surface_block,
                             voxel::BlockId subsurface_block, voxel::BlockId stone_block) {
    constexpr u32 kEdge = voxel::Chunk::kEdgeLength;

    for (u32 lz = 0; lz < kEdge; ++lz) {
        const i32 world_z = coord.z * static_cast<i32>(kEdge) + static_cast<i32>(lz);
        for (u32 lx = 0; lx < kEdge; ++lx) {
            const i32 world_x = coord.x * static_cast<i32>(kEdge) + static_cast<i32>(lx);
            const i32 height = terrain_height(seed, world_x, world_z);

            for (u32 ly = 0; ly < kEdge; ++ly) {
                const i32 world_y = coord.y * static_cast<i32>(kEdge) + static_cast<i32>(ly);
                if (world_y > height) {
                    continue;  // air - the chunk's default fill, nothing to set.
                }
                voxel::BlockId block_id = stone_block;
                if (world_y == height) {
                    block_id = surface_block;
                } else if (world_y > height - kSubsurfaceDepth) {
                    block_id = subsurface_block;
                }
                chunk.set_block(lx, ly, lz, block_id);
            }
        }
    }
}

}  // namespace lcu::world::worldgen
