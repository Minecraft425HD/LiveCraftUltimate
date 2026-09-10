#include "lcu/lighting/world_light.h"

#include <gtest/gtest.h>

using lcu::voxel::ChunkCoord;

namespace {
constexpr lcu::u32 kEdge = 16;
using TestWorldLight = lcu::lighting::WorldLight<kEdge>;
}  // namespace

TEST(WorldLight, NewlyCreatedChunkLightStartsAllZero) {
    TestWorldLight world_light;
    const auto& light = world_light.chunk_light({0, 0, 0});
    EXPECT_EQ(light.sky_light(5, 5, 5), 0u);
    EXPECT_EQ(light.block_light(5, 5, 5), 0u);
}

TEST(WorldLight, FindChunkLightReturnsNullptrForAnUnknownChunk) {
    TestWorldLight world_light;
    EXPECT_EQ(world_light.find_chunk_light({3, 0, 0}), nullptr);
    EXPECT_FALSE(world_light.has_chunk_light({3, 0, 0}));
}

TEST(WorldLight, ChunkLightGetOrCreateMakesItFindable) {
    TestWorldLight world_light;
    world_light.chunk_light({1, 2, 3}).set_sky_light(0, 0, 0, 9);

    const auto* found = world_light.find_chunk_light({1, 2, 3});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->sky_light(0, 0, 0), 9u);
    EXPECT_TRUE(world_light.has_chunk_light({1, 2, 3}));
    EXPECT_EQ(world_light.loaded_chunk_count(), 1u);
}

TEST(WorldLight, RemoveChunkLightDropsIt) {
    TestWorldLight world_light;
    world_light.chunk_light({0, 0, 0});
    ASSERT_TRUE(world_light.has_chunk_light({0, 0, 0}));

    world_light.remove_chunk_light({0, 0, 0});

    EXPECT_FALSE(world_light.has_chunk_light({0, 0, 0}));
    EXPECT_EQ(world_light.loaded_chunk_count(), 0u);
}

TEST(WorldLight, SkyLightAtResolvesAnInBoundsLocalCoordinateWithinTheSameChunk) {
    TestWorldLight world_light;
    world_light.chunk_light({2, 0, 0}).set_sky_light(4, 5, 6, 11);

    const auto result = world_light.sky_light_at({2, 0, 0}, 4, 5, 6);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 11u);
}

TEST(WorldLight, SkyLightAtResolvesAPositiveOutOfRangeCoordinateIntoTheNextChunk) {
    // lx == EdgeLength is exactly one step past chunk (0,0,0)'s own
    // local range - the real case a +X cross-chunk BFS step produces
    // (Phase 30/31). It must land in chunk (1,0,0)'s local x=0, not
    // silently clamp or wrap within the origin chunk.
    TestWorldLight world_light;
    world_light.chunk_light({1, 0, 0}).set_sky_light(0, 5, 5, 13);

    const auto result = world_light.sky_light_at({0, 0, 0}, static_cast<lcu::i32>(kEdge), 5, 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 13u);
}

TEST(WorldLight, SkyLightAtResolvesANegativeOutOfRangeCoordinateIntoThePreviousChunk) {
    // lx == -1 from chunk (0,0,0) must resolve to chunk (-1,0,0)'s local
    // x=15 (floor division, not truncation - see
    // voxel::world_to_chunk_and_local's own doc comment), the same
    // negative-coordinate correctness engine/world already relies on.
    TestWorldLight world_light;
    world_light.chunk_light({-1, 0, 0}).set_sky_light(kEdge - 1, 5, 5, 7);

    const auto result = world_light.sky_light_at({0, 0, 0}, -1, 5, 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 7u);
}

TEST(WorldLight, SkyLightAtReturnsNulloptWhenTheResolvedNeighborChunkIsNotLoaded) {
    TestWorldLight world_light;  // chunk (1,0,0) never created

    const auto result = world_light.sky_light_at({0, 0, 0}, static_cast<lcu::i32>(kEdge), 5, 5);
    EXPECT_FALSE(result.has_value());
}

TEST(WorldLight, BlockLightAtUsesTheSameCrossChunkResolutionAsSkyLightAt) {
    TestWorldLight world_light;
    world_light.chunk_light({0, 1, 0}).set_block_light(5, 0, 5, 6);

    const auto result = world_light.block_light_at({0, 0, 0}, 5, static_cast<lcu::i32>(kEdge), 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 6u);
}
