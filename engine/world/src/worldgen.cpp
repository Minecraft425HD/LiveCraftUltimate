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

// --- 3D noise (Phase 40: caves need real volumetric noise, not just
// the 2D column-height noise every stage before this one used) ---

u32 hash3d(u32 seed, i32 x, i32 y, i32 z) {
    u32 h = seed;
    h ^= static_cast<u32>(x) * 0x27d4eb2du;
    h ^= static_cast<u32>(y) * 0x9e3779b1u;
    h ^= static_cast<u32>(z) * 0x165667b1u;
    h ^= h >> 15;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

f32 lattice_value3d(u32 seed, i32 x, i32 y, i32 z) {
    return static_cast<f32>(hash3d(seed, x, y, z) & 0x00FFFFFFu) / static_cast<f32>(0x01000000u);
}

// Trilinearly-interpolated, smoothed value noise in [0, 1) at
// continuous (x, y, z) - the 3D counterpart to smooth_noise above,
// same lattice-hash-then-interpolate structure, one more axis.
f32 smooth_noise3d(u32 seed, f32 x, f32 y, f32 z) {
    const i32 x0 = static_cast<i32>(std::floor(x));
    const i32 y0 = static_cast<i32>(std::floor(y));
    const i32 z0 = static_cast<i32>(std::floor(z));
    const f32 fx = x - static_cast<f32>(x0);
    const f32 fy = y - static_cast<f32>(y0);
    const f32 fz = z - static_cast<f32>(z0);

    const f32 v000 = lattice_value3d(seed, x0, y0, z0);
    const f32 v100 = lattice_value3d(seed, x0 + 1, y0, z0);
    const f32 v010 = lattice_value3d(seed, x0, y0 + 1, z0);
    const f32 v110 = lattice_value3d(seed, x0 + 1, y0 + 1, z0);
    const f32 v001 = lattice_value3d(seed, x0, y0, z0 + 1);
    const f32 v101 = lattice_value3d(seed, x0 + 1, y0, z0 + 1);
    const f32 v011 = lattice_value3d(seed, x0, y0 + 1, z0 + 1);
    const f32 v111 = lattice_value3d(seed, x0 + 1, y0 + 1, z0 + 1);

    const f32 sx = smoothstep(fx);
    const f32 sy = smoothstep(fy);
    const f32 sz = smoothstep(fz);

    const f32 v00 = lerp(v000, v100, sx);
    const f32 v10 = lerp(v010, v110, sx);
    const f32 v01 = lerp(v001, v101, sx);
    const f32 v11 = lerp(v011, v111, sx);
    const f32 v0 = lerp(v00, v10, sy);
    const f32 v1 = lerp(v01, v11, sy);
    return lerp(v0, v1, sz);
}

// 4-octave 3D fractal sum, normalized back to [0, 1) - same structure
// as the 2D fractal_noise above.
f32 fractal_noise3d(u32 seed, f32 x, f32 y, f32 z) {
    f32 total = 0.0f;
    f32 amplitude = 1.0f;
    f32 frequency = 1.0f;
    f32 amplitude_sum = 0.0f;

    for (int octave = 0; octave < 4; ++octave) {
        total += smooth_noise3d(seed + static_cast<u32>(octave) * 101u, x * frequency, y * frequency,
                                 z * frequency) *
                 amplitude;
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

// Cave stage (Phase 40): two independent 3D noise fields (own seed
// offsets, same "different offset = statistically independent"
// reasoning used everywhere else in this file) sampled at the same
// point - where their values land close together (kCaveThreshold),
// the cell is carved into open air. This "noise crevice" difference
// technique produces winding, connected, tunnel-like voids, unlike a
// single-field threshold ("cheese caves") which produces isolated
// round blobs (see DECISIONS.md for the comparison). kCaveNoiseScale
// is deliberately coarser than the ore scale below - caves should be
// large connected structures, not block-by-block static.
constexpr f32 kCaveNoiseScale = 0.05f;
constexpr u32 kCaveSeedOffsetA = 1013904223u;
constexpr u32 kCaveSeedOffsetB = 2654435761u;
constexpr f32 kCaveThreshold = 0.03f;
// How many blocks below that column's own terrain_height a cave is
// allowed to start - keeps tunnels from ever punching a hole right at
// ground level (which would look like a hole in the world, not a
// cave entrance).
constexpr i32 kCaveMinDepthBelowSurface = 4;

// Ore stage (Phase 40): each ore its own independent 3D noise field
// (own seed offset) plus an absolute world-Y range and a rarity
// threshold on [0,1) - a cell only becomes ore if it is both inside
// that ore's depth band and its own noise field crosses the
// threshold. Iron checked before Coal and given a narrower, deeper
// band and a higher threshold (rarer) - real ore rarity, Coal common
// and shallow, Iron uncommon and deep - rather than both being
// equally likely everywhere underground.
// Threshold values picked from fractal_noise3d's own real, empirically-
// measured output range at this amplitude/octave configuration (a
// 4-octave weighted average clusters well inside [0,1) - roughly
// [0.05, 0.95] in practice, not the full range - see DECISIONS.md for
// the real sampled data this was tuned against, the same "measure, don't
// guess" approach Phase 38's spawn-radius fix and Phase 39's biome
// thresholds already used). kCoalThreshold keeps Coal a real, regularly
//-findable resource (~3.7% of eligible cells); kIronThreshold is
// deliberately higher, on top of Iron's own narrower/deeper Y range
// below, so Iron stays genuinely rarer than Coal (~0.1% of eligible
// cells) - both still small next to OreType::None's overwhelming share.
constexpr f32 kOreNoiseScale = 0.08f;
constexpr u32 kCoalSeedOffset = 374761393u;
constexpr f32 kCoalThreshold = 0.70f;
constexpr i32 kCoalMinY = -48;
constexpr i32 kCoalMaxY = 48;
constexpr u32 kIronSeedOffset = 3266489917u;
constexpr f32 kIronThreshold = 0.80f;
constexpr i32 kIronMinY = -48;
constexpr i32 kIronMaxY = -4;

// Vegetation stage (Phase 41): a genuinely separate, independent noise
// field per vegetation type (own seed offset each, same "different
// offset = statistically independent" reasoning used everywhere else
// in this file), reusing the existing 2D fractal_noise (trees/cacti
// are column decisions, not volumetric like caves/ores).
// kVegetationNoiseScale is coarser than a per-block hash so placement
// clusters into real patches (small forests/cactus stands) instead of
// scattering uniformly - still fine-grained enough to vary within a
// single loaded area, unlike the much lower-frequency climate/
// continental fields. Thresholds picked from fractal_noise's own real,
// empirically-measured output range (the same "measure, don't guess"
// approach Phase 40's ore thresholds were just fixed with) -
// kTreeThreshold keeps trees a real, regularly-occurring feature
// (~4.5% of Plains columns); kCactusThreshold is slightly higher, so
// cacti stay somewhat sparser than trees (~2.5% of Desert columns) -
// both still small next to VegetationType::None's overwhelming share.
constexpr f32 kVegetationNoiseScale = 0.15f;
constexpr u32 kTreeSeedOffset = 668265263u;
constexpr f32 kTreeThreshold = 0.72f;
constexpr u32 kCactusSeedOffset = 2166136261u;
constexpr f32 kCactusThreshold = 0.75f;

// How tall a tree's trunk/canopy cap is, and a cactus' height -
// confined entirely to the one column that spawned it (see
// worldgen.h's own doc comment on VegetationType/vegetation_at for why
// - a real, deliberate scope choice, not an accidental cross-chunk
// gap). Small, simple shapes - not the varied tree/cactus silhouettes
// a shipped game would eventually want (see DECISIONS.md).
constexpr i32 kTreeTrunkHeight = 4;
constexpr i32 kTreeCanopyHeight = 3;
constexpr i32 kCactusHeight = 3;

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

bool is_cave(u32 seed, i32 world_x, i32 world_y, i32 world_z, i32 surface_height) {
    if (world_y > surface_height - kCaveMinDepthBelowSurface) {
        return false;
    }
    const f32 x = static_cast<f32>(world_x);
    const f32 y = static_cast<f32>(world_y);
    const f32 z = static_cast<f32>(world_z);
    const f32 field_a =
        fractal_noise3d(seed + kCaveSeedOffsetA, x * kCaveNoiseScale, y * kCaveNoiseScale, z * kCaveNoiseScale);
    const f32 field_b =
        fractal_noise3d(seed + kCaveSeedOffsetB, x * kCaveNoiseScale, y * kCaveNoiseScale, z * kCaveNoiseScale);
    return std::abs(field_a - field_b) < kCaveThreshold;
}

OreType ore_at(u32 seed, i32 world_x, i32 world_y, i32 world_z) {
    const f32 x = static_cast<f32>(world_x);
    const f32 y = static_cast<f32>(world_y);
    const f32 z = static_cast<f32>(world_z);

    if (world_y >= kIronMinY && world_y <= kIronMaxY) {
        const f32 iron_value =
            fractal_noise3d(seed + kIronSeedOffset, x * kOreNoiseScale, y * kOreNoiseScale, z * kOreNoiseScale);
        if (iron_value > kIronThreshold) {
            return OreType::Iron;
        }
    }
    if (world_y >= kCoalMinY && world_y <= kCoalMaxY) {
        const f32 coal_value =
            fractal_noise3d(seed + kCoalSeedOffset, x * kOreNoiseScale, y * kOreNoiseScale, z * kOreNoiseScale);
        if (coal_value > kCoalThreshold) {
            return OreType::Coal;
        }
    }
    return OreType::None;
}

VegetationType vegetation_at(u32 seed, i32 world_x, i32 world_z, Biome biome) {
    const f32 x = static_cast<f32>(world_x) * kVegetationNoiseScale;
    const f32 z = static_cast<f32>(world_z) * kVegetationNoiseScale;

    if (biome == Biome::Plains) {
        if (fractal_noise(seed + kTreeSeedOffset, x, z) > kTreeThreshold) {
            return VegetationType::Tree;
        }
    } else if (biome == Biome::Desert) {
        if (fractal_noise(seed + kCactusSeedOffset, x, z) > kCactusThreshold) {
            return VegetationType::Cactus;
        }
    }
    return VegetationType::None;
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
                             voxel::BlockId stone_block, voxel::BlockId water_block, const OreBlocks& ore_blocks,
                             const VegetationBlocks& vegetation_blocks) {
    constexpr u32 kEdge = voxel::Chunk::kEdgeLength;

    for (u32 lz = 0; lz < kEdge; ++lz) {
        const i32 world_z = coord.z * static_cast<i32>(kEdge) + static_cast<i32>(lz);
        for (u32 lx = 0; lx < kEdge; ++lx) {
            const i32 world_x = coord.x * static_cast<i32>(kEdge) + static_cast<i32>(lx);
            const i32 height = terrain_height(seed, world_x, world_z);
            const Biome biome = biome_at(seed, world_x, world_z);
            const SurfaceBlocks surface_blocks = surface_blocks_for(biome, biome_blocks);
            // Vegetation (Phase 41) never grows on a below-sea-level
            // (underwater) column - only decided once per column, not
            // once per cell, since it's the same answer for every Y.
            const VegetationType vegetation =
                height > kSeaLevel ? vegetation_at(seed, world_x, world_z, biome) : VegetationType::None;

            for (u32 ly = 0; ly < kEdge; ++ly) {
                const i32 world_y = coord.y * static_cast<i32>(kEdge) + static_cast<i32>(ly);
                if (world_y > height) {
                    // Above the terrain: water fills the gap up to sea
                    // level for a below-sea-level column (Phase 37).
                    // Above sea level, a column with real vegetation
                    // gets its trunk (Tree/Cactus alike start directly
                    // on top of the surface block) then, for a Tree
                    // only, a leaf cap directly above the trunk (Phase
                    // 41) - everything else stays air, the chunk's
                    // default fill. Not biome-dependent for water - a
                    // below-sea-level column is water regardless of
                    // climate (no ice-cap-vs-open-water distinction
                    // yet, an honest scoped gap, not a hidden one).
                    if (world_y <= kSeaLevel) {
                        chunk.set_block(lx, ly, lz, water_block);
                    } else if (vegetation == VegetationType::Tree) {
                        if (world_y <= height + kTreeTrunkHeight) {
                            chunk.set_block(lx, ly, lz, vegetation_blocks.wood);
                        } else if (world_y <= height + kTreeTrunkHeight + kTreeCanopyHeight) {
                            chunk.set_block(lx, ly, lz, vegetation_blocks.leaves);
                        }
                    } else if (vegetation == VegetationType::Cactus) {
                        if (world_y <= height + kCactusHeight) {
                            chunk.set_block(lx, ly, lz, vegetation_blocks.cactus);
                        }
                    }
                    continue;
                }
                if (is_cave(seed, world_x, world_y, world_z, height)) {
                    // Carved into open air (Phase 40) - the chunk's
                    // default fill, nothing to set. kCaveMinDepthBelowSurface
                    // already keeps this from ever firing within the
                    // surface/subsurface layers below, so no ordering
                    // conflict with them.
                    continue;
                }
                voxel::BlockId block_id = stone_block;
                if (world_y == height) {
                    block_id = surface_blocks.surface;
                } else if (world_y > height - kSubsurfaceDepth) {
                    block_id = surface_blocks.subsurface;
                } else {
                    switch (ore_at(seed, world_x, world_y, world_z)) {
                        case OreType::Coal:
                            block_id = ore_blocks.coal_ore;
                            break;
                        case OreType::Iron:
                            block_id = ore_blocks.iron_ore;
                            break;
                        case OreType::None:
                            break;
                    }
                }
                chunk.set_block(lx, ly, lz, block_id);
            }
        }
    }
}

}  // namespace lcu::world::worldgen
