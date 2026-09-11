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

// Phase 38: two genuinely separate noise stages, matching brief
// section 21's own pipeline naming ("kontinental -> terrain") for
// real instead of just as a comment on one combined noise sample.
//
//   - Continental (kContinentalNoiseScale, ~85% of a chunk-edge's
//     worth of blocks per lattice cell - a much lower frequency than
//     the terrain-detail layer below): a broad, slowly-varying
//     "how much landmass is here" value in [0, 1) that alone decides
//     two things per column - the base elevation before any local
//     detail (kDeepOceanBase for continental=0 up to kHighlandBase
//     for continental=1), and how much room local detail gets to work
//     with (kMinMountainAmplitude..kMaxMountainAmplitude) - a coastal
//     /oceanic area is capped to gentle relief regardless of what the
//     detail layer samples there, while a highland area can swing
//     into real mountain-sized peaks and valleys.
//   - Terrain detail (kNoiseScale, the original Phase 3 frequency,
//     unchanged): the same 4-octave fractal_noise as before, now
//     scaled by the continental-driven amplitude above instead of a
//     single fixed kHeightVariation everywhere - this is what actually
//     produces mountain-shaped relief in highland regions and keeps
//     ocean/coastal regions comparatively flat, rather than the same
//     uniform bumpiness Phase 37 (and every earlier phase) generated
//     regardless of where a column was.
//
// A different seed offset per stage (kContinentalSeedOffset) keeps the
// two noise fields statistically independent - without it, "how
// mountainous" and "the mountain shape itself" would correlate through
// the exact same lattice, producing visible, unnatural alignment
// between coastlines and ridge lines.
constexpr f32 kContinentalNoiseScale = 0.0015f;
constexpr u32 kContinentalSeedOffset = 747796405u;
constexpr i32 kDeepOceanBase = -14;
constexpr i32 kHighlandBase = 10;
constexpr f32 kMinMountainAmplitude = 4.0f;
constexpr f32 kMaxMountainAmplitude = 20.0f;
constexpr f32 kNoiseScale = 0.01f;

// How many layers of `subsurface_block` sit directly beneath the
// surface layer before `stone_block` takes over - matches the common
// voxel-game convention this genre's players already expect (a grass
// top, a few blocks of dirt, then stone), not tuned against anything
// more specific since there's no other block variation yet to balance
// it against.
constexpr i32 kSubsurfaceDepth = 3;

// Climate/biome stage (Phase 39): a genuinely separate, independent
// noise field from both continental and terrain-detail above (its own
// seed offset, same reasoning as kContinentalSeedOffset - without it,
// "which biome" and "how mountainous"/"the terrain shape" would
// visibly correlate through the same lattice). Frequency similar to
// the continental layer (biomes are large regions, not block-by-block
// noise) but not identical, so biome boundaries and coastlines don't
// trace the same curve.
constexpr f32 kClimateNoiseScale = 0.0012f;
constexpr u32 kClimateSeedOffset = 2246822519u;
// Thresholds splitting the climate value's [0,1) range into three
// bands - Snowy below kSnowyThreshold, Desert above kDesertThreshold,
// Plains (the pre-Phase-39 default) in between. Plains deliberately
// the widest band (50% of the range vs. 25% each for Snowy/Desert) -
// it was every column's behavior before this phase and should stay
// the common case, not become a minority one.
constexpr f32 kSnowyThreshold = 0.25f;
constexpr f32 kDesertThreshold = 0.75f;

}  // namespace

i32 terrain_height(u32 seed, i32 world_x, i32 world_z) {
    const f32 x = static_cast<f32>(world_x);
    const f32 z = static_cast<f32>(world_z);

    // Continental stage: broad, low-frequency "how much landmass"
    // value - decides the base elevation and how tall local relief is
    // allowed to get (see the constants' own doc comment above).
    const f32 continental = fractal_noise(seed + kContinentalSeedOffset, x * kContinentalNoiseScale,
                                           z * kContinentalNoiseScale);
    const f32 base_elevation = lerp(static_cast<f32>(kDeepOceanBase), static_cast<f32>(kHighlandBase), continental);
    const f32 mountain_amplitude = lerp(kMinMountainAmplitude, kMaxMountainAmplitude, continental);

    // Terrain stage: the original Phase 3 detail noise, unchanged in
    // frequency - only its amplitude now varies with continentalness
    // instead of being the same fixed kHeightVariation everywhere.
    const f32 detail = fractal_noise(seed, x * kNoiseScale, z * kNoiseScale);

    return static_cast<i32>(base_elevation + (detail - 0.5f) * 2.0f * mountain_amplitude);
}

Biome biome_at(u32 seed, i32 world_x, i32 world_z) {
    const f32 climate = fractal_noise(seed + kClimateSeedOffset, static_cast<f32>(world_x) * kClimateNoiseScale,
                                       static_cast<f32>(world_z) * kClimateNoiseScale);
    if (climate < kSnowyThreshold) {
        return Biome::Snowy;
    }
    if (climate > kDesertThreshold) {
        return Biome::Desert;
    }
    return Biome::Plains;
}

namespace {

// Which land block ids a column's own biome maps to (Phase 39) -
// resolved once per column, not once per cell, since it's the same
// answer for every Y in that column.
struct SurfaceBlocks {
    voxel::BlockId surface;
    voxel::BlockId subsurface;
};

SurfaceBlocks surface_blocks_for(Biome biome, const BiomeBlocks& biome_blocks) {
    switch (biome) {
        case Biome::Snowy:
            return {biome_blocks.snowy_surface, biome_blocks.snowy_subsurface};
        case Biome::Desert:
            return {biome_blocks.desert_surface, biome_blocks.desert_subsurface};
        case Biome::Plains:
            return {biome_blocks.plains_surface, biome_blocks.plains_subsurface};
    }
    return {biome_blocks.plains_surface, biome_blocks.plains_subsurface};
}

}  // namespace

void generate_terrain_chunk(voxel::Chunk& chunk, voxel::ChunkCoord coord, u32 seed, const BiomeBlocks& biome_blocks,
                             voxel::BlockId stone_block, voxel::BlockId water_block) {
    constexpr u32 kEdge = voxel::Chunk::kEdgeLength;

    for (u32 lz = 0; lz < kEdge; ++lz) {
        const i32 world_z = coord.z * static_cast<i32>(kEdge) + static_cast<i32>(lz);
        for (u32 lx = 0; lx < kEdge; ++lx) {
            const i32 world_x = coord.x * static_cast<i32>(kEdge) + static_cast<i32>(lx);
            const i32 height = terrain_height(seed, world_x, world_z);
            const SurfaceBlocks surface_blocks = surface_blocks_for(biome_at(seed, world_x, world_z), biome_blocks);

            for (u32 ly = 0; ly < kEdge; ++ly) {
                const i32 world_y = coord.y * static_cast<i32>(kEdge) + static_cast<i32>(ly);
                if (world_y > height) {
                    // Above the terrain: water fills the gap up to sea
                    // level for a below-sea-level column (Phase 37);
                    // above sea level (or a dry column, where height
                    // is already >= kSeaLevel and this branch is never
                    // reached at or below it) stays air - the chunk's
                    // default fill, nothing to set, unchanged from
                    // before this phase. Not biome-dependent - a
                    // below-sea-level column is water regardless of
                    // climate (no ice-cap-vs-open-water distinction
                    // yet, an honest scoped gap, not a hidden one).
                    if (world_y <= kSeaLevel) {
                        chunk.set_block(lx, ly, lz, water_block);
                    }
                    continue;
                }
                voxel::BlockId block_id = stone_block;
                if (world_y == height) {
                    block_id = surface_blocks.surface;
                } else if (world_y > height - kSubsurfaceDepth) {
                    block_id = surface_blocks.subsurface;
                }
                chunk.set_block(lx, ly, lz, block_id);
            }
        }
    }
}

}  // namespace lcu::world::worldgen
