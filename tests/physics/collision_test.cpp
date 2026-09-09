#include "lcu/physics/collision.h"

#include <gtest/gtest.h>

using lcu::voxel::BlockId;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::World;
using lcu::physics::AABB;
using lcu::physics::apply_gravity;
using lcu::physics::integrate_player;
using lcu::physics::move_and_collide;
using lcu::physics::PlayerPhysicsConfig;
using lcu::physics::PlayerPhysicsState;
using lcu::physics::try_jump;

namespace {

constexpr BlockId kSolid = 5;

bool is_solid_predicate(BlockId id) { return id == kSolid; }

World make_empty_world() {
    World world(1, [](Chunk&, ChunkCoord) {});
    world.load_chunk({0, 0, 0});
    return world;
}

// A world with a full floor plane at y=0 (occupying y in [0,1)) across
// the whole chunk.
World make_floor_world() {
    World world(1, [](Chunk& chunk, ChunkCoord) {
        for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
            for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                chunk.set_block(x, 0, z, kSolid);
            }
        }
    });
    world.load_chunk({0, 0, 0});
    return world;
}

}  // namespace

// --- AABB ---------------------------------------------------------------

TEST(AABB, CenterAndHalfExtents) {
    const AABB box{{0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, 6.0f}};
    EXPECT_FLOAT_EQ(box.center().x, 1.0f);
    EXPECT_FLOAT_EQ(box.center().y, 2.0f);
    EXPECT_FLOAT_EQ(box.center().z, 3.0f);
    EXPECT_FLOAT_EQ(box.half_extents().x, 1.0f);
    EXPECT_FLOAT_EQ(box.half_extents().y, 2.0f);
    EXPECT_FLOAT_EQ(box.half_extents().z, 3.0f);
}

TEST(AABB, TranslatedMovesBothCorners) {
    const AABB box{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    const AABB moved = box.translated({5.0f, -2.0f, 0.0f});
    EXPECT_FLOAT_EQ(moved.min.x, 5.0f);
    EXPECT_FLOAT_EQ(moved.min.y, -2.0f);
    EXPECT_FLOAT_EQ(moved.max.x, 6.0f);
    EXPECT_FLOAT_EQ(moved.max.y, -1.0f);
}

TEST(AABB, IntersectsDetectsOverlapAndNonOverlap) {
    const AABB a{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    const AABB overlapping{{0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 1.5f}};
    const AABB separate{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};
    const AABB touchingOnly{{1.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}};  // edges touch, no volume overlap

    EXPECT_TRUE(a.intersects(overlapping));
    EXPECT_FALSE(a.intersects(separate));
    EXPECT_FALSE(a.intersects(touchingOnly));
}

// --- move_and_collide -----------------------------------------------------

TEST(Collision, NoCollisionWhenPathIsClear) {
    World world = make_empty_world();
    const AABB aabb{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};

    const auto result = move_and_collide(world, aabb, {1.0f, 1.0f, 1.0f}, is_solid_predicate);

    EXPECT_FLOAT_EQ(result.resolved_delta.x, 1.0f);
    EXPECT_FLOAT_EQ(result.resolved_delta.y, 1.0f);
    EXPECT_FLOAT_EQ(result.resolved_delta.z, 1.0f);
    EXPECT_FALSE(result.hit_x);
    EXPECT_FALSE(result.hit_y);
    EXPECT_FALSE(result.hit_z);
}

TEST(Collision, FallingAabbStopsOnTopOfFloor) {
    World world = make_empty_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(5, 0, 5, kSolid);

    const AABB aabb{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};
    const auto result = move_and_collide(world, aabb, {0.0f, -10.0f, 0.0f}, is_solid_predicate);

    // Floor occupies y in [0,1); aabb.min.y falls from 5 to just above 1.
    EXPECT_NEAR(result.resolved_delta.y, -4.0f, 0.001f);
    EXPECT_TRUE(result.hit_y);
    EXPECT_TRUE(result.grounded);
    EXPECT_FALSE(result.hit_x);
    EXPECT_FALSE(result.hit_z);
}

TEST(Collision, MovingIntoWallStopsAtWallFace) {
    World world = make_empty_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(10, 5, 5, kSolid);

    const AABB aabb{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};
    const auto result = move_and_collide(world, aabb, {10.0f, 0.0f, 0.0f}, is_solid_predicate);

    // Wall face at x=10; aabb.max.x can advance from 6 to just under 10.
    EXPECT_NEAR(result.resolved_delta.x, 4.0f, 0.001f);
    EXPECT_TRUE(result.hit_x);
}

TEST(Collision, MovingAwayFromWallIsUnobstructed) {
    World world = make_empty_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(10, 5, 5, kSolid);

    const AABB aabb{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};
    const auto result = move_and_collide(world, aabb, {-3.0f, 0.0f, 0.0f}, is_solid_predicate);

    EXPECT_FLOAT_EQ(result.resolved_delta.x, -3.0f);
    EXPECT_FALSE(result.hit_x);
}

TEST(Collision, XAndZAxesResolveIndependently) {
    World world = make_empty_world();
    // Solid only along +X; nothing blocks +Z.
    world.chunk_at_mutable({0, 0, 0})->set_block(10, 5, 5, kSolid);

    const AABB aabb{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};
    const auto result = move_and_collide(world, aabb, {10.0f, 0.0f, 10.0f}, is_solid_predicate);

    EXPECT_NEAR(result.resolved_delta.x, 4.0f, 0.001f);
    EXPECT_TRUE(result.hit_x);
    EXPECT_FLOAT_EQ(result.resolved_delta.z, 10.0f);
    EXPECT_FALSE(result.hit_z);
}

// --- gravity / jump ---------------------------------------------------------

TEST(PlayerPhysics, GravityAccumulatesAndCapsAtMaxFallSpeed) {
    PlayerPhysicsState state;
    const PlayerPhysicsConfig config;

    apply_gravity(state, config, 0.1f);
    EXPECT_LT(state.vertical_velocity, 0.0f);
    EXPECT_GT(state.vertical_velocity, config.max_fall_speed);

    for (int i = 0; i < 50; ++i) {
        apply_gravity(state, config, 1.0f);
    }
    EXPECT_FLOAT_EQ(state.vertical_velocity, config.max_fall_speed);
}

TEST(PlayerPhysics, JumpDoesNothingWhenNotGrounded) {
    PlayerPhysicsState state;
    state.grounded = false;
    state.vertical_velocity = 0.0f;
    const PlayerPhysicsConfig config;

    try_jump(state, config);

    EXPECT_FLOAT_EQ(state.vertical_velocity, 0.0f);
}

TEST(PlayerPhysics, JumpSetsVelocityWhenGrounded) {
    PlayerPhysicsState state;
    state.grounded = true;
    const PlayerPhysicsConfig config;

    try_jump(state, config);

    EXPECT_FLOAT_EQ(state.vertical_velocity, config.jump_speed);
}

// --- integrate_player -----------------------------------------------------

TEST(PlayerPhysics, IntegratePlayerLandsOnFloorAndSetsGrounded) {
    World world = make_floor_world();
    PlayerPhysicsState state;
    state.aabb = AABB{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};
    state.vertical_velocity = -20.0f;
    state.grounded = false;

    integrate_player(world, state, {0.0f, 0.0f, 0.0f}, PlayerPhysicsConfig{}, 1.0f, is_solid_predicate);

    EXPECT_TRUE(state.grounded);
    EXPECT_FLOAT_EQ(state.vertical_velocity, 0.0f);
    EXPECT_NEAR(state.aabb.min.y, 1.0f, 0.01f);
}

TEST(PlayerPhysics, StationaryGroundedPlayerStaysGroundedWithoutFalling) {
    World world = make_floor_world();
    PlayerPhysicsState state;
    state.aabb = AABB{{5.0f, 1.0f, 5.0f}, {6.0f, 2.0f, 6.0f}};  // resting exactly on the floor
    state.vertical_velocity = 0.0f;
    state.grounded = true;

    integrate_player(world, state, {0.0f, 0.0f, 0.0f}, PlayerPhysicsConfig{}, 1.0f, is_solid_predicate);

    // No horizontal or vertical input this frame - grounded must stay
    // true (this is the exact bug a naive "grounded = this frame's
    // downward collision" implementation gets wrong: delta.y is 0, so
    // there is no downward collision to report, but the player hasn't
    // moved and is still standing on the floor).
    EXPECT_TRUE(state.grounded);
    EXPECT_NEAR(state.aabb.min.y, 1.0f, 0.01f);
}

TEST(PlayerPhysics, DefaultStepHeightDoesNotAutoClimbAFullBlock) {
    World world = make_floor_world();
    // A full-height (1 block) step directly ahead.
    world.chunk_at_mutable({0, 0, 0})->set_block(7, 1, 5, kSolid);

    PlayerPhysicsState state;
    state.aabb = AABB{{6.0f, 1.0f, 5.0f}, {7.0f, 2.0f, 6.0f}};
    state.vertical_velocity = 0.0f;
    state.grounded = true;

    integrate_player(world, state, {1.0f, 0.0f, 0.0f}, PlayerPhysicsConfig{}, 1.0f, is_solid_predicate);

    // Default step_height (0.51) intentionally can't clear a full
    // 1-block obstacle (matches real block-game "can't step over a full
    // wall, only small ledges" behavior) - the player should be blocked
    // roughly where they started, not teleported on top of the block.
    EXPECT_LT(state.aabb.max.x, 7.01f);
    EXPECT_NEAR(state.aabb.min.y, 1.0f, 0.01f);
    EXPECT_TRUE(state.grounded);
}

TEST(PlayerPhysics, IncreasedStepHeightAutoClimbsAFullBlock) {
    World world = make_floor_world();
    world.chunk_at_mutable({0, 0, 0})->set_block(7, 1, 5, kSolid);

    PlayerPhysicsState state;
    state.aabb = AABB{{6.0f, 1.0f, 5.0f}, {7.0f, 2.0f, 6.0f}};
    state.vertical_velocity = 0.0f;
    state.grounded = true;

    PlayerPhysicsConfig config;
    config.step_height = 1.05f;  // enough to fully clear a 1-block ledge

    integrate_player(world, state, {1.0f, 0.0f, 0.0f}, config, 1.0f, is_solid_predicate);

    EXPECT_NEAR(state.aabb.min.x, 7.0f, 0.01f);
    EXPECT_NEAR(state.aabb.min.y, 2.0f, 0.01f);  // standing on top of the step
    EXPECT_TRUE(state.grounded);
}
