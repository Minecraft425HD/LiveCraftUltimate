#include "lcu/lighting/propagation.h"

#include <unordered_map>
#include <unordered_set>

#include <gtest/gtest.h>

#include "lcu/lighting/light_storage.h"
#include "lcu/lighting/world_light.h"
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

// Minimal stand-in for lcu::world::World (Phase 31): a plain
// ChunkCoord -> ChunkStorage map with a chunk_at() matching World's own
// signature exactly (const ChunkStorage<EdgeLength>* chunk_at(coord)
// const) - the duck-typed ChunkProviderT the cross-chunk propagation
// functions are templated on. Real World isn't used here so these
// tests stay focused on the propagation algorithm itself, not on
// engine/world's own chunk lifecycle machinery.
template <lcu::u32 EdgeLength>
struct TestChunkProvider {
    std::unordered_map<lcu::voxel::ChunkCoord, lcu::voxel::ChunkStorage<EdgeLength>> chunks;

    void add_chunk(lcu::voxel::ChunkCoord coord, lcu::voxel::ChunkStorage<EdgeLength> chunk) {
        chunks[coord] = std::move(chunk);
    }

    const lcu::voxel::ChunkStorage<EdgeLength>* chunk_at(lcu::voxel::ChunkCoord coord) const {
        const auto it = chunks.find(coord);
        return it != chunks.end() ? &it->second : nullptr;
    }
};

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

TEST(PropagateAddedBlockLightCrossChunk, LightCrossesIntoAnAdjacentLoadedChunk) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);
    const lcu::u8 emission = registry.definition_of(torch).light_emission;

    TestChunkProvider<Chunk::kEdgeLength> provider;
    Chunk origin_chunk;
    origin_chunk.set_block(Chunk::kEdgeLength - 1, 8, 8, torch);  // right at the +X chunk edge
    provider.add_chunk({0, 0, 0}, origin_chunk);
    provider.add_chunk({1, 0, 0}, Chunk{});  // neighbor loaded, all air

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, emission);
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                            Chunk::kEdgeLength - 1, 8, 8);

    // Real decay continuing across the boundary, not stopping at it.
    EXPECT_EQ(world_light.chunk_light({1, 0, 0}).block_light(0, 8, 8), emission - 1);
    EXPECT_EQ(world_light.chunk_light({1, 0, 0}).block_light(1, 8, 8), emission - 2);
    EXPECT_EQ(world_light.chunk_light({1, 0, 0}).block_light(2, 8, 8), emission - 3);
}

TEST(PropagateAddedBlockLightCrossChunk, DoesNotCrossIntoAnUnloadedNeighborChunk) {
    // Honesty guarantee (see WorldLight's own doc comment): an unloaded
    // neighbor is never written to, not guessed at - has_chunk_light
    // must stay false for it, same as before this source ever existed.
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);
    const lcu::u8 emission = registry.definition_of(torch).light_emission;

    TestChunkProvider<Chunk::kEdgeLength> provider;
    Chunk origin_chunk;
    origin_chunk.set_block(Chunk::kEdgeLength - 1, 8, 8, torch);
    provider.add_chunk({0, 0, 0}, origin_chunk);
    // {1, 0, 0} deliberately never added - not loaded.

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, emission);
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                            Chunk::kEdgeLength - 1, 8, 8);

    EXPECT_FALSE(world_light.has_chunk_light({1, 0, 0}));
}

TEST(UnpropagateBlockLightCrossChunk, RemovingACrossChunkSourceDarkensBothChunks) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);
    const lcu::u8 emission = registry.definition_of(torch).light_emission;

    TestChunkProvider<Chunk::kEdgeLength> provider;
    Chunk origin_chunk;
    origin_chunk.set_block(Chunk::kEdgeLength - 1, 8, 8, torch);
    provider.add_chunk({0, 0, 0}, origin_chunk);
    provider.add_chunk({1, 0, 0}, Chunk{});

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, emission);
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                            Chunk::kEdgeLength - 1, 8, 8);
    ASSERT_GT(world_light.chunk_light({1, 0, 0}).block_light(0, 8, 8), 0u);

    // Torch removed from the origin chunk's own block data, then retract.
    provider.add_chunk({0, 0, 0}, Chunk{});
    lcu::lighting::unpropagate_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                        Chunk::kEdgeLength - 1, 8, 8, emission);

    EXPECT_EQ(world_light.chunk_light({0, 0, 0}).block_light(Chunk::kEdgeLength - 1, 8, 8), 0u);
    EXPECT_EQ(world_light.chunk_light({1, 0, 0}).block_light(0, 8, 8), 0u);
    EXPECT_EQ(world_light.chunk_light({1, 0, 0}).block_light(1, 8, 8), 0u);
}

TEST(UnpropagateBlockLightCrossChunk, RemovingOneOfTwoCrossChunkSourcesRefillsFromTheRemainingOne) {
    // Cross-chunk analog of UnpropagateBlockLight.RemovingOneOfTwo
    // SourcesRefillsFromTheRemainingOne: one torch on each side of the
    // boundary: removing the origin-chunk torch must correctly refill
    // the overlap from the neighbor-chunk torch's own light, not leave
    // a dark gap.
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);
    const lcu::u8 emission = registry.definition_of(torch).light_emission;

    TestChunkProvider<Chunk::kEdgeLength> provider;
    Chunk origin_chunk;
    origin_chunk.set_block(Chunk::kEdgeLength - 1, 8, 8, torch);
    provider.add_chunk({0, 0, 0}, origin_chunk);
    Chunk neighbor_chunk;
    neighbor_chunk.set_block(0, 8, 8, torch);
    provider.add_chunk({1, 0, 0}, neighbor_chunk);

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, emission);
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                            Chunk::kEdgeLength - 1, 8, 8);
    world_light.chunk_light({1, 0, 0}).set_block_light(0, 8, 8, emission);
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {1, 0, 0}, 0, 8, 8);

    // Remove the origin-chunk torch only.
    provider.add_chunk({0, 0, 0}, Chunk{});
    lcu::lighting::unpropagate_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                        Chunk::kEdgeLength - 1, 8, 8, emission);

    // The neighbor chunk's own torch is untouched and still lights its
    // own cell at full emission - the refill phase must not have
    // wrongly darkened a source that was never part of the retracted
    // BFS tree.
    EXPECT_EQ(world_light.chunk_light({1, 0, 0}).block_light(0, 8, 8), emission);
    // The origin chunk's edge cell, one step from the neighbor's torch,
    // should be refilled to emission-1 by the neighbor's own light
    // flowing back across the boundary - not left dark.
    EXPECT_EQ(world_light.chunk_light({0, 0, 0}).block_light(Chunk::kEdgeLength - 1, 8, 8), emission - 1);
}

TEST(PropagateAddedBlockLightCrossChunk, TouchedChunksReportsTheRealNeighborLightActuallyReached) {
    // Phase 33: the real answer to "which chunks besides the edited one
    // need remeshing" - not a guessed fixed radius.
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);
    const lcu::u8 emission = registry.definition_of(torch).light_emission;

    TestChunkProvider<Chunk::kEdgeLength> provider;
    Chunk origin_chunk;
    origin_chunk.set_block(Chunk::kEdgeLength - 1, 8, 8, torch);
    provider.add_chunk({0, 0, 0}, origin_chunk);
    provider.add_chunk({1, 0, 0}, Chunk{});

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, emission);
    std::unordered_set<lcu::voxel::ChunkCoord> touched;
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                            Chunk::kEdgeLength - 1, 8, 8, &touched);

    EXPECT_EQ(touched.size(), 1u);
    EXPECT_TRUE(touched.count({1, 0, 0}));
    // The origin chunk's own extent (where most of the flood actually
    // happens) must not appear - the caller already knows to remesh
    // the chunk it just edited, this is purely the "what ELSE" answer.
    EXPECT_FALSE(touched.count({0, 0, 0}));
}

TEST(PropagateAddedBlockLightCrossChunk, TouchedChunksStaysEmptyWhenLightNeverReachesAnyNeighbor) {
    // A dim source (emission 2, unlike "torch"'s 14) placed dead center
    // of a 16-wide chunk: 8 steps from the nearest face in any
    // direction, far more than its 2-step reach - genuinely can't cross
    // a boundary.
    BlockRegistry registry;
    BlockDefinition dim_light;
    dim_light.namespaced_id = "test:glowstone_dim";
    dim_light.is_transparent = true;
    dim_light.light_emission = 2;
    const auto dim = registry.register_block(dim_light);
    const lcu::u8 emission = registry.definition_of(dim).light_emission;

    TestChunkProvider<Chunk::kEdgeLength> provider;
    Chunk origin_chunk;
    origin_chunk.set_block(8, 8, 8, dim);
    provider.add_chunk({0, 0, 0}, origin_chunk);
    provider.add_chunk({1, 0, 0}, Chunk{});

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    world_light.chunk_light({0, 0, 0}).set_block_light(8, 8, 8, emission);
    std::unordered_set<lcu::voxel::ChunkCoord> touched;
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0}, 8, 8, 8,
                                                            &touched);

    EXPECT_TRUE(touched.empty());
}

TEST(UnpropagateBlockLightCrossChunk, TouchedChunksReportsNeighborsDarkenedOrRefilled) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);
    const lcu::u8 emission = registry.definition_of(torch).light_emission;

    TestChunkProvider<Chunk::kEdgeLength> provider;
    Chunk origin_chunk;
    origin_chunk.set_block(Chunk::kEdgeLength - 1, 8, 8, torch);
    provider.add_chunk({0, 0, 0}, origin_chunk);
    provider.add_chunk({1, 0, 0}, Chunk{});

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    world_light.chunk_light({0, 0, 0}).set_block_light(Chunk::kEdgeLength - 1, 8, 8, emission);
    lcu::lighting::propagate_added_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                            Chunk::kEdgeLength - 1, 8, 8);

    provider.add_chunk({0, 0, 0}, Chunk{});
    std::unordered_set<lcu::voxel::ChunkCoord> touched;
    lcu::lighting::unpropagate_block_light_cross_chunk(provider, registry, world_light, {0, 0, 0},
                                                        Chunk::kEdgeLength - 1, 8, 8, emission, &touched);

    EXPECT_EQ(touched.size(), 1u);
    EXPECT_TRUE(touched.count({1, 0, 0}));
    EXPECT_FALSE(touched.count({0, 0, 0}));
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

TEST(ComputeSkyLightCrossChunk, NoChunkAboveAssumesOpenSkySameAsSingleChunkDefault) {
    // Phase 30: with nothing loaded above (the common "topmost loaded
    // chunk" case), a cross-chunk column must behave identically to the
    // original single-chunk compute_sky_light_column default.
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    Chunk chunk;  // all air
    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    const lcu::voxel::ChunkCoord coord{0, 0, 0};

    lcu::lighting::compute_sky_light_cross_chunk(chunk, registry, world_light, coord);

    for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
        EXPECT_EQ(world_light.chunk_light(coord).sky_light(3, y, 3), lcu::lighting::Light::kMaxLightLevel) << "y=" << y;
    }
}

TEST(ComputeSkyLightCrossChunk, SolidRoofInTheChunkAboveDarkensThisChunksTopLayer) {
    // The real bug Phase 30 fixes: a chunk with a solid roof directly
    // above it (in the neighboring chunk) must show 0 sky light at its
    // own top layer, not full brightness.
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    const lcu::voxel::ChunkCoord above{0, 1, 0};
    const lcu::voxel::ChunkCoord below{0, 0, 0};

    Chunk above_chunk;
    above_chunk.set_block(3, 0, 3, stone);  // solid floor at the bottom of the chunk above
    lcu::lighting::compute_sky_light_cross_chunk(above_chunk, registry, world_light, above);

    Chunk below_chunk;  // all air
    lcu::lighting::compute_sky_light_cross_chunk(below_chunk, registry, world_light, below);

    EXPECT_EQ(world_light.chunk_light(below).sky_light(3, Chunk::kEdgeLength - 1, 3), 0u);
    EXPECT_EQ(world_light.chunk_light(below).sky_light(3, 0, 3), 0u);
    // An unrelated column (no roof above it anywhere) stays fully lit -
    // proves the darkening is column-specific, not a whole-chunk effect.
    EXPECT_EQ(world_light.chunk_light(below).sky_light(4, Chunk::kEdgeLength - 1, 4),
              lcu::lighting::Light::kMaxLightLevel);
}

TEST(ComputeSkyLightCrossChunk, OpenSkyInTheChunkAboveLeavesThisChunkFullyLit) {
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    const lcu::voxel::ChunkCoord above{0, 1, 0};
    const lcu::voxel::ChunkCoord below{0, 0, 0};

    Chunk above_chunk;  // all air
    lcu::lighting::compute_sky_light_cross_chunk(above_chunk, registry, world_light, above);

    Chunk below_chunk;  // all air
    lcu::lighting::compute_sky_light_cross_chunk(below_chunk, registry, world_light, below);

    EXPECT_EQ(world_light.chunk_light(below).sky_light(3, Chunk::kEdgeLength - 1, 3),
              lcu::lighting::Light::kMaxLightLevel);
}

TEST(ComputeSkyLightCrossChunk, AChunksOwnRoofStillShadowsRegardlessOfWhatsAboveIt) {
    // The chunk's own solid block still blocks light independent of
    // sky_open_above - a chunk with open sky above it but its own roof
    // partway down must still go dark below that roof.
    lcu::voxel::BlockId stone = 0;
    lcu::voxel::BlockId torch = 0;
    const BlockRegistry registry = make_registry(stone, torch);

    lcu::lighting::WorldLight<Chunk::kEdgeLength> world_light;
    const lcu::voxel::ChunkCoord above{0, 1, 0};
    const lcu::voxel::ChunkCoord below{0, 0, 0};

    Chunk above_chunk;  // all air - open sky
    lcu::lighting::compute_sky_light_cross_chunk(above_chunk, registry, world_light, above);

    Chunk below_chunk;
    below_chunk.set_block(3, 10, 3, stone);
    lcu::lighting::compute_sky_light_cross_chunk(below_chunk, registry, world_light, below);

    EXPECT_EQ(world_light.chunk_light(below).sky_light(3, 15, 3), lcu::lighting::Light::kMaxLightLevel);
    EXPECT_EQ(world_light.chunk_light(below).sky_light(3, 10, 3), 0u);
    EXPECT_EQ(world_light.chunk_light(below).sky_light(3, 0, 3), 0u);
}

TEST(LightStorage, CombinedLightIsTheBrighterChannel) {
    lcu::lighting::Light light;
    light.set_sky_light(1, 1, 1, 4);
    light.set_block_light(1, 1, 1, 9);
    EXPECT_EQ(light.combined_light(1, 1, 1), 9u);

    light.set_block_light(1, 1, 1, 2);
    EXPECT_EQ(light.combined_light(1, 1, 1), 4u);
}
