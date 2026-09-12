#include "lcu/rendering/occlusion_culler.h"

#include <gtest/gtest.h>

#include "lcu/math/mat4.h"
#include "lcu/rendering/frustum.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/world/world.h"

using lcu::math::Mat4;
using lcu::rendering::Frustum;
using lcu::rendering::OcclusionCuller;
using lcu::voxel::BlockDefinition;
using lcu::voxel::BlockId;
using lcu::voxel::BlockRegistry;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkCoord;
using lcu::world::World;

namespace {

BlockRegistry make_registry_with_stone(BlockId& out_stone_id) {
    BlockRegistry registry;
    BlockDefinition stone;
    stone.namespaced_id = "test:stone";
    stone.display_name = "Stone";
    stone.is_transparent = false;
    out_stone_id = registry.register_block(stone);
    return registry;
}

void fill_solid(Chunk& chunk, BlockId id) {
    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        for (lcu::u32 y = 0; y < Chunk::kEdgeLength; ++y) {
            for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
                chunk.set_block(x, y, z, id);
            }
        }
    }
}

// A frustum that accepts every AABB - built from a degenerate (all-zero)
// view*projection matrix, the same real "no real camera set up yet"
// fallback Frustum::from_view_projection's own doc comment describes
// (see Phase 68's own DegenerateViewProjectionAcceptsEverything test).
// These tests are about OcclusionCuller's own real BFS/portal logic, not
// about re-exercising the frustum boundary a second time.
Frustum permissive_frustum() {
    Mat4 zero{};
    for (lcu::f32& value : zero.m) {
        value = 0.0f;
    }
    return Frustum::from_view_projection(zero);
}

World make_all_air_world() {
    return World(1, [](Chunk&, ChunkCoord) {});
}

}  // namespace

TEST(OcclusionCuller, AllAirMakesEveryLoadedChunkVisible) {
    World world = make_all_air_world();
    for (lcu::i32 x = -1; x <= 1; ++x) {
        for (lcu::i32 y = -1; y <= 1; ++y) {
            for (lcu::i32 z = -1; z <= 1; ++z) {
                world.load_chunk({x, y, z});
            }
        }
    }
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    OcclusionCuller culler;

    const auto visible = culler.compute(world, registry, {0, 0, 0}, permissive_frustum());

    EXPECT_EQ(visible.size(), 27u);
}

TEST(OcclusionCuller, FullySolidWorldWithCameraInsideSeesOnlyItsOwnChunk) {
    World world = make_all_air_world();
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    for (lcu::i32 x = -1; x <= 1; ++x) {
        for (lcu::i32 y = -1; y <= 1; ++y) {
            for (lcu::i32 z = -1; z <= 1; ++z) {
                world.load_chunk({x, y, z});
                fill_solid(*world.chunk_at_mutable({x, y, z}), stone_id);
            }
        }
    }
    OcclusionCuller culler;

    const auto visible = culler.compute(world, registry, {0, 0, 0}, permissive_frustum());

    EXPECT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible.count({0, 0, 0}), 1u);
}

TEST(OcclusionCuller, HoleInCeilingMakesTheChunkAboveVisible) {
    World world = make_all_air_world();
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);

    world.load_chunk({0, 0, 0});
    fill_solid(*world.chunk_at_mutable({0, 0, 0}), stone_id);
    // A single real gap in the camera chunk's own ceiling (+Y face).
    world.chunk_at_mutable({0, 0, 0})->set_block(8, Chunk::kEdgeLength - 1, 8, 0 /* air */);

    world.load_chunk({0, 1, 0});  // stays default all-air, so its own -Y face is never fully opaque

    OcclusionCuller culler;
    const auto visible = culler.compute(world, registry, {0, 0, 0}, permissive_frustum());

    EXPECT_EQ(visible.count({0, 1, 0}), 1u) << "the chunk above the ceiling hole should be reachable";
}

TEST(OcclusionCuller, SolidCeilingWithNoGapHidesTheChunkAbove) {
    World world = make_all_air_world();
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);

    world.load_chunk({0, 0, 0});
    fill_solid(*world.chunk_at_mutable({0, 0, 0}), stone_id);
    world.load_chunk({0, 1, 0});
    fill_solid(*world.chunk_at_mutable({0, 1, 0}), stone_id);

    OcclusionCuller culler;
    const auto visible = culler.compute(world, registry, {0, 0, 0}, permissive_frustum());

    EXPECT_EQ(visible.size(), 1u) << "a real, unbroken solid ceiling must not let BFS through";
}

TEST(OcclusionCuller, EmptyResultWhenCameraChunkItselfIsNotLoaded) {
    World world = make_all_air_world();
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    OcclusionCuller culler;

    const auto visible = culler.compute(world, registry, {5, 5, 5}, permissive_frustum());

    EXPECT_TRUE(visible.empty());
}

TEST(OcclusionCuller, OutsideTheFrustumIsNeverEnteredEvenThroughARealPortal) {
    World world = make_all_air_world();
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    world.load_chunk({0, 0, 0});
    world.load_chunk({1, 0, 0});

    OcclusionCuller culler;
    // A degenerate, zero-normal-length frustum plane set never rejects
    // anything by construction (see Frustum's own fallback) - build a
    // real, narrow one instead so {1,0,0} genuinely fails contains_aabb.
    const Mat4 view = Mat4::look_at({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 proj = Mat4::perspective(0.2f, 1.0f, 0.1f, 5.0f);
    const Frustum narrow_frustum = Frustum::from_view_projection(proj * view);

    const auto visible = culler.compute(world, registry, {0, 0, 0}, narrow_frustum);

    EXPECT_EQ(visible.count({1, 0, 0}), 0u);
}

TEST(OcclusionCuller, InvalidateForcesARecomputeReflectingANewBlockEdit) {
    World world = make_all_air_world();
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);

    world.load_chunk({0, 0, 0});
    fill_solid(*world.chunk_at_mutable({0, 0, 0}), stone_id);
    world.load_chunk({0, 1, 0});
    fill_solid(*world.chunk_at_mutable({0, 1, 0}), stone_id);

    OcclusionCuller culler;
    ASSERT_EQ(culler.compute(world, registry, {0, 0, 0}, permissive_frustum()).size(), 1u);

    // A real block edit breaks a hole through BOTH sides of the shared
    // boundary (the portal test needs a real gap on each side - see
    // OcclusionCuller's own doc comment on the approximation) - without
    // invalidating the cached masks, compute() would keep reusing the
    // stale "fully solid" bits forever.
    world.chunk_at_mutable({0, 0, 0})->set_block(8, Chunk::kEdgeLength - 1, 8, 0);
    world.chunk_at_mutable({0, 1, 0})->set_block(8, 0, 8, 0);
    culler.invalidate_neighbors({0, 0, 0});
    culler.invalidate_neighbors({0, 1, 0});

    const auto visible = culler.compute(world, registry, {0, 0, 0}, permissive_frustum());
    EXPECT_EQ(visible.count({0, 1, 0}), 1u);
}
