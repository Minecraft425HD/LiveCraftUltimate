#include "lcu/world/world.h"

#include <gtest/gtest.h>

using lcu::voxel::BlockId;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::ChunkLifecycleState;
using lcu::world::World;

namespace {

constexpr BlockId kMarkerBlock = 7;

lcu::world::ChunkGenerator marker_generator() {
    return [](Chunk& chunk, ChunkCoord) { chunk.set_block(0, 0, 0, kMarkerBlock); };
}

}  // namespace

TEST(World, UnrequestedChunkStartsUnloaded) {
    World world(1, marker_generator());
    EXPECT_EQ(world.state_of({5, 0, 5}), ChunkLifecycleState::Unloaded);
    EXPECT_EQ(world.chunk_at({5, 0, 5}), nullptr);
}

TEST(World, RequestChunkTransitionsToRequested) {
    World world(1, marker_generator());
    world.request_chunk({0, 0, 0});
    EXPECT_EQ(world.state_of({0, 0, 0}), ChunkLifecycleState::Requested);
    EXPECT_EQ(world.chunk_at({0, 0, 0}), nullptr);  // not generated yet
}

TEST(World, LoadChunkRunsGeneratorAndReachesGenerated) {
    World world(1, marker_generator());
    world.load_chunk({2, 0, -3});

    EXPECT_EQ(world.state_of({2, 0, -3}), ChunkLifecycleState::Generated);
    const Chunk* chunk = world.chunk_at({2, 0, -3});
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->block_at(0, 0, 0), kMarkerBlock);
}

TEST(World, GeneratorReceivesTheRequestedCoordinate) {
    ChunkCoord seen{999, 999, 999};
    World world(1, [&seen](Chunk&, ChunkCoord coord) { seen = coord; });

    world.load_chunk({4, -1, 9});

    EXPECT_EQ(seen, (ChunkCoord{4, -1, 9}));
}

TEST(World, LoadChunkIsIdempotent) {
    int call_count = 0;
    World world(1, [&call_count](Chunk&, ChunkCoord) { ++call_count; });

    world.load_chunk({0, 0, 0});
    world.load_chunk({0, 0, 0});
    world.load_chunk({0, 0, 0});

    EXPECT_EQ(call_count, 1);
}

TEST(World, UnloadChunkRemovesIt) {
    World world(1, marker_generator());
    world.load_chunk({0, 0, 0});
    ASSERT_NE(world.chunk_at({0, 0, 0}), nullptr);

    world.unload_chunk({0, 0, 0});

    EXPECT_EQ(world.state_of({0, 0, 0}), ChunkLifecycleState::Unloaded);
    EXPECT_EQ(world.chunk_at({0, 0, 0}), nullptr);
}

TEST(World, AdoptGeneratedChunkSkipsTheGeneratorAndUsesTheProvidedContent) {
    int call_count = 0;
    World world(1, [&call_count](Chunk&, ChunkCoord) { ++call_count; });

    Chunk precomputed;
    precomputed.set_block(1, 2, 3, kMarkerBlock);
    world.adopt_generated_chunk({4, 0, -1}, precomputed);

    EXPECT_EQ(call_count, 0) << "adopt_generated_chunk must never invoke generator_";
    EXPECT_EQ(world.state_of({4, 0, -1}), ChunkLifecycleState::Generated);
    const Chunk* chunk = world.chunk_at({4, 0, -1});
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->block_at(1, 2, 3), kMarkerBlock);
}

TEST(World, AdoptGeneratedChunkIsANoOpIfAlreadyLoaded) {
    World world(1, marker_generator());
    world.load_chunk({0, 0, 0});  // real content: kMarkerBlock at (0,0,0)

    Chunk different;
    different.set_block(5, 5, 5, 99);
    world.adopt_generated_chunk({0, 0, 0}, different);

    const Chunk* chunk = world.chunk_at({0, 0, 0});
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->block_at(0, 0, 0), kMarkerBlock) << "existing chunk must not be clobbered";
    EXPECT_EQ(chunk->block_at(5, 5, 5), 0);
}

TEST(World, GenerateChunkAssertsIfNotRequestedFirst) {
    World world(1, marker_generator());
    EXPECT_DEATH(world.generate_chunk({0, 0, 0}), "");
}

TEST(World, SeedIsPreserved) {
    World world(12345, marker_generator());
    EXPECT_EQ(world.seed(), 12345u);
}

TEST(World, UpdateStreamingLoadsFullCubeWithinRadius) {
    World world(1, marker_generator());
    world.update_streaming({0, 0, 0}, /*load_radius=*/1, /*unload_radius=*/1);

    // 3x3x3 = 27 chunks.
    EXPECT_EQ(world.loaded_chunk_count(), 27u);
    EXPECT_NE(world.chunk_at({1, 1, 1}), nullptr);
    EXPECT_NE(world.chunk_at({-1, -1, -1}), nullptr);
    EXPECT_EQ(world.chunk_at({2, 0, 0}), nullptr);
}

TEST(World, UpdateStreamingUnloadsChunksBeyondUnloadRadius) {
    World world(1, marker_generator());
    world.load_chunk({10, 0, 0});
    ASSERT_NE(world.chunk_at({10, 0, 0}), nullptr);

    world.update_streaming({0, 0, 0}, /*load_radius=*/1, /*unload_radius=*/2);

    EXPECT_EQ(world.chunk_at({10, 0, 0}), nullptr);
}

TEST(World, UpdateStreamingHysteresisKeepsChunkJustOutsideLoadRadius) {
    World world(1, marker_generator());
    // Distance 2 from origin: outside load_radius (1) but inside
    // unload_radius (2) - should stay loaded across the call, not
    // bounce between loaded/unloaded every frame.
    world.load_chunk({2, 0, 0});

    world.update_streaming({0, 0, 0}, /*load_radius=*/1, /*unload_radius=*/2);

    EXPECT_NE(world.chunk_at({2, 0, 0}), nullptr);
}

TEST(World, UpdateStreamingCenterMovesAndOldChunksUnload) {
    World world(1, marker_generator());
    world.update_streaming({0, 0, 0}, 1, 1);
    ASSERT_NE(world.chunk_at({-1, 0, 0}), nullptr);

    // Move far away - everything from the old center should eventually
    // fall outside the new unload radius.
    world.update_streaming({20, 0, 0}, 1, 1);

    EXPECT_EQ(world.chunk_at({-1, 0, 0}), nullptr);
    EXPECT_NE(world.chunk_at({20, 0, 0}), nullptr);
}
