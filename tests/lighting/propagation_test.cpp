#include "lcu/lighting/propagation.h"

#include <gtest/gtest.h>

#include "lcu/lighting/light_storage.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"

using lcu::voxel::BlockDefinition;
using lcu::voxel::BlockRegistry;
using lcu::voxel::Chunk;

namespace {

BlockRegistry make_registry(lcu::voxel::BlockId& out_stone, lcu::voxel::BlockId& out_torch) {
    BlockRegistry registry;

    BlockDefinition stone;
    stone.namespaced_id = "game:stone";
    stone.is_transparent = false;
    out_stone = registry.register_block(stone);

    BlockDefinition torch;
    torch.namespaced_id = "game:torch";
    torch.is_transparent = true;
    torch.light_emission = 14;
    out_torch = registry.register_block(torch);

    return registry;
}

}  // namespace

TEST(ComputeBlockLight, EmptyChunkHasNoBlockLight) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;  // all air
    lcu::lighting::Light light;
    lcu::lighting::compute_block_light(chunk, registry, light);

    EXPECT_EQ(light.block_light(0, 0, 0), 0u);
    EXPECT_EQ(light.block_light(8, 8, 8), 0u);
}

TEST(ComputeBlockLight, EmitterLightsItselfAtFullEmission) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(8, 8, 8, torch);
    lcu::lighting::Light light;
    lcu::lighting::compute_block_light(chunk, registry, light);

    EXPECT_EQ(light.block_light(8, 8, 8), 14u);
}

TEST(ComputeBlockLight, LightDecaysByOnePerStepInOpenAir) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(8, 8, 8, torch);
    lcu::lighting::Light light;
    lcu::lighting::compute_block_light(chunk, registry, light);

    EXPECT_EQ(light.block_light(8, 8, 8), 14u);
    EXPECT_EQ(light.block_light(9, 8, 8), 13u);
    EXPECT_EQ(light.block_light(10, 8, 8), 12u);
    EXPECT_EQ(light.block_light(8, 8, 5), 11u);  // 3 steps away
}

TEST(ComputeBlockLight, OpaqueWallStopsLightPropagation) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(8, 8, 8, torch);
    // A full solid wall across the x=9 plane - unlike a single block,
    // light has no way to flood *around* it (light propagates through
    // all 6 neighbor directions, so a single-block "wall" only blocks
    // the direct path, not the ones that bend around it via y/z).
    for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
        for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
            chunk.set_block(9, y, z, stone);
        }
    }
    lcu::lighting::Light light;
    lcu::lighting::compute_block_light(chunk, registry, light);

    EXPECT_EQ(light.block_light(9, 8, 8), 0u);   // the wall itself stays dark
    EXPECT_EQ(light.block_light(10, 8, 8), 0u);  // and nothing beyond it is lit
    EXPECT_EQ(light.block_light(15, 8, 8), 0u);
}

TEST(ComputeBlockLight, TwoEmittersCombineToTheBrighterValueAtOverlap) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(4, 8, 8, torch);
    chunk.set_block(12, 8, 8, torch);
    lcu::lighting::Light light;
    lcu::lighting::compute_block_light(chunk, registry, light);

    // Cell at x=8 is 4 steps from each torch either way; both give the
    // same level, so the stored value is that shared level, not summed.
    EXPECT_EQ(light.block_light(8, 8, 8), 10u);
}

TEST(PropagateAddedBlockLight, MatchesAFullRecomputeForASingleNewEmitter) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(8, 8, 8, torch);

    lcu::lighting::Light incremental;
    incremental.set_block_light(8, 8, 8, registry.definition_of(torch).light_emission);
    lcu::lighting::propagate_added_block_light(chunk, registry, incremental, 8, 8, 8);

    lcu::lighting::Light full;
    lcu::lighting::compute_block_light(chunk, registry, full);

    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        EXPECT_EQ(incremental.block_light(x, 8, 8), full.block_light(x, 8, 8)) << "x=" << x;
    }
}

TEST(UnpropagateBlockLight, RemovingTheOnlySourceDarkensEverythingItLit) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(8, 8, 8, torch);
    lcu::lighting::Light light;
    lcu::lighting::compute_block_light(chunk, registry, light);
    ASSERT_EQ(light.block_light(9, 8, 8), 13u);

    chunk.set_block(8, 8, 8, lcu::voxel::kAirBlockId);  // torch removed
    lcu::lighting::unpropagate_block_light(chunk, registry, light, 8, 8, 8, 14);

    EXPECT_EQ(light.block_light(8, 8, 8), 0u);
    EXPECT_EQ(light.block_light(9, 8, 8), 0u);
    EXPECT_EQ(light.block_light(10, 8, 8), 0u);
}

TEST(UnpropagateBlockLight, RemovingOneOfTwoSourcesRefillsFromTheRemainingOne) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(4, 8, 8, torch);
    chunk.set_block(12, 8, 8, torch);
    lcu::lighting::Light light;
    lcu::lighting::compute_block_light(chunk, registry, light);

    chunk.set_block(4, 8, 8, lcu::voxel::kAirBlockId);
    lcu::lighting::unpropagate_block_light(chunk, registry, light, 4, 8, 8, 14);

    // The remaining torch at x=12 should now be the sole source: light
    // at x=4 should match what a full recompute with only that torch
    // produces.
    Chunk solo_chunk;
    solo_chunk.set_block(12, 8, 8, torch);
    lcu::lighting::Light solo_light;
    lcu::lighting::compute_block_light(solo_chunk, registry, solo_light);

    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        EXPECT_EQ(light.block_light(x, 8, 8), solo_light.block_light(x, 8, 8)) << "x=" << x;
    }
}

TEST(ComputeSkyLight, OpenColumnIsFullyLitTopToBottom) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;  // all air
    lcu::lighting::Light light;
    lcu::lighting::compute_sky_light(chunk, registry, light);

    for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
        EXPECT_EQ(light.sky_light(3, y, 3), lcu::lighting::Light::kMaxLightLevel) << "y=" << y;
    }
}

TEST(ComputeSkyLight, OpaqueBlockShadowsEverythingBelowItInItsColumn) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(3, 10, 3, stone);  // a roof partway up the column
    lcu::lighting::Light light;
    lcu::lighting::compute_sky_light(chunk, registry, light);

    EXPECT_EQ(light.sky_light(3, 15, 3), lcu::lighting::Light::kMaxLightLevel);  // above the roof
    EXPECT_EQ(light.sky_light(3, 10, 3), 0u);                                    // the roof block itself
    EXPECT_EQ(light.sky_light(3, 9, 3), 0u);                                     // below the roof
    EXPECT_EQ(light.sky_light(3, 0, 3), 0u);
}

TEST(ComputeSkyLight, AdjacentColumnsAreIndependent) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;
    chunk.set_block(3, 10, 3, stone);
    lcu::lighting::Light light;
    lcu::lighting::compute_sky_light(chunk, registry, light);

    // No lateral bleed - the documented simplification (see
    // DECISIONS.md): the neighboring open column is unaffected by the
    // roof one column over.
    EXPECT_EQ(light.sky_light(4, 9, 3), lcu::lighting::Light::kMaxLightLevel);
}

TEST(LightStorage, CombinedLightIsTheBrighterChannel) {
    lcu::lighting::Light light;
    light.set_sky_light(1, 1, 1, 4);
    light.set_block_light(1, 1, 1, 9);
    EXPECT_EQ(light.combined_light(1, 1, 1), 9u);

    light.set_block_light(1, 1, 1, 2);
    EXPECT_EQ(light.combined_light(1, 1, 1), 4u);
}
