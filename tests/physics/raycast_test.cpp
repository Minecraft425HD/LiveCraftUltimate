#include "lcu/physics/raycast.h"

#include <cmath>

#include <gtest/gtest.h>

using lcu::voxel::BlockId;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::World;

namespace {

constexpr BlockId kSolid = 5;
constexpr BlockId kOtherSolid = 6;

bool is_solid_predicate(BlockId id) { return id == kSolid || id == kOtherSolid; }

// A World whose single chunk (0,0,0) is empty except for whatever the
// test sets via a mutable-chunk lookup after loading it.
World make_test_world() {
    World world(1, [](Chunk&, ChunkCoord) {});
    world.load_chunk({0, 0, 0});
    return world;
}

}  // namespace

TEST(Raycast, HitsSolidBlockAlongPositiveXAxis) {
    World world = make_test_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(5, 5, 5, kSolid);

    const auto hit = lcu::physics::raycast(world, {0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f}, 20.0f, is_solid_predicate);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->world.x, 5);
    EXPECT_EQ(hit->world.y, 5);
    EXPECT_EQ(hit->world.z, 5);
    EXPECT_EQ(hit->block, kSolid);
    EXPECT_NEAR(hit->distance, 4.5f, 0.001f);
    EXPECT_FLOAT_EQ(hit->normal.x, -1.0f);
    EXPECT_FLOAT_EQ(hit->normal.y, 0.0f);
    EXPECT_FLOAT_EQ(hit->normal.z, 0.0f);
}

TEST(Raycast, MissesWhenNoSolidBlockInPath) {
    World world = make_test_world();  // entirely air

    const auto hit = lcu::physics::raycast(world, {0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f}, 20.0f, is_solid_predicate);

    EXPECT_FALSE(hit.has_value());
}

TEST(Raycast, ReturnsNulloptWhenBlockIsBeyondMaxDistance) {
    World world = make_test_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(15, 5, 5, kSolid);

    const auto hit = lcu::physics::raycast(world, {0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f}, 2.0f, is_solid_predicate);

    EXPECT_FALSE(hit.has_value());
}

TEST(Raycast, HitsImmediatelyWhenOriginStartsInsideSolidBlock) {
    World world = make_test_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(5, 5, 5, kSolid);

    const auto hit = lcu::physics::raycast(world, {5.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f}, 20.0f, is_solid_predicate);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->world.x, 5);
    EXPECT_FLOAT_EQ(hit->distance, 0.0f);
    EXPECT_FLOAT_EQ(hit->normal.x, 0.0f);
    EXPECT_FLOAT_EQ(hit->normal.y, 0.0f);
    EXPECT_FLOAT_EQ(hit->normal.z, 0.0f);
}

TEST(Raycast, RaycastThroughUnloadedChunkNeverHitsAndTerminates) {
    // No chunks loaded at all - block_at_world always returns nullopt.
    // The important property is that this still terminates (bounded by
    // max_distance) rather than looping forever.
    World world(1, [](Chunk&, ChunkCoord) {});

    const auto hit = lcu::physics::raycast(world, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 50.0f, is_solid_predicate);

    EXPECT_FALSE(hit.has_value());
}

TEST(Raycast, ZeroDirectionReturnsNullopt) {
    World world = make_test_world();
    const auto hit = lcu::physics::raycast(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f}, 20.0f, is_solid_predicate);
    EXPECT_FALSE(hit.has_value());
}

TEST(Raycast, PredicateControlsWhatCountsAsSolid) {
    World world = make_test_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(5, 5, 5, kOtherSolid);

    // Predicate that only treats kSolid (not kOtherSolid) as solid.
    const auto only_solid = [](BlockId id) { return id == kSolid; };
    const auto miss = lcu::physics::raycast(world, {0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f}, 20.0f, only_solid);
    EXPECT_FALSE(miss.has_value());

    const auto hit = lcu::physics::raycast(world, {0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f}, 20.0f, is_solid_predicate);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block, kOtherSolid);
}

TEST(Raycast, DiagonalRayProducesAxisAlignedUnitNormal) {
    World world = make_test_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(5, 5, 5, kSolid);

    const auto hit =
        lcu::physics::raycast(world, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, 20.0f, is_solid_predicate);

    ASSERT_TRUE(hit.has_value());
    // Exactly one axis should be +-1, the other two exactly 0 - the
    // defining property of a DDA face-entry normal.
    const int nonzero_axes = (hit->normal.x != 0.0f ? 1 : 0) + (hit->normal.y != 0.0f ? 1 : 0) +
                              (hit->normal.z != 0.0f ? 1 : 0);
    EXPECT_EQ(nonzero_axes, 1);

    const float magnitude = std::abs(hit->normal.x) + std::abs(hit->normal.y) + std::abs(hit->normal.z);
    EXPECT_FLOAT_EQ(magnitude, 1.0f);
}

TEST(Raycast, HitReportsCorrectChunkAndLocalCoordinates) {
    World world = make_test_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(5, 5, 5, kSolid);

    const auto hit = lcu::physics::raycast(world, {0.5f, 5.5f, 5.5f}, {1.0f, 0.0f, 0.0f}, 20.0f, is_solid_predicate);

    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->chunk, (ChunkCoord{0, 0, 0}));
    EXPECT_EQ(hit->local.x, 5u);
    EXPECT_EQ(hit->local.y, 5u);
    EXPECT_EQ(hit->local.z, 5u);
}
