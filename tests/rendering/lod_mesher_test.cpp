#include "lcu/rendering/lod_mesher.h"

#include <gtest/gtest.h>

#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"

using lcu::rendering::build_lod_chunk;
using lcu::rendering::LodChunkMesh;
using lcu::voxel::BlockDefinition;
using lcu::voxel::BlockId;
using lcu::voxel::BlockRegistry;
using lcu::voxel::Chunk;

namespace {

BlockRegistry make_registry_with_stone(BlockId& out_stone_id) {
    BlockRegistry registry;
    BlockDefinition stone;
    stone.namespaced_id = "test:stone";
    stone.display_name = "Stone";
    stone.color = {0.5f, 0.5f, 0.5f};
    out_stone_id = registry.register_block(stone);
    return registry;
}

}  // namespace

TEST(LodMesher, AllAirChunkHasNoGeometry) {
    Chunk chunk;
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);

    const LodChunkMesh mesh = build_lod_chunk(chunk, registry);

    EXPECT_FALSE(mesh.has_geometry);
}

TEST(LodMesher, FlatSurfaceGivesExactHeightAndColor) {
    Chunk chunk;
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    // A uniform, flat slab at y=0..3 (4 blocks tall) across every column
    // - the real topmost voxel in every column is y=3, so the real
    // average local height should be exactly 4 (y+1) everywhere.
    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
            for (lcu::u32 y = 0; y < 4; ++y) {
                chunk.set_block(x, y, z, stone_id);
            }
        }
    }

    const LodChunkMesh mesh = build_lod_chunk(chunk, registry);

    ASSERT_TRUE(mesh.has_geometry);
    EXPECT_FLOAT_EQ(mesh.average_local_height, 4.0f);
    EXPECT_FLOAT_EQ(mesh.average_color.x, 0.5f);
    EXPECT_FLOAT_EQ(mesh.average_color.y, 0.5f);
    EXPECT_FLOAT_EQ(mesh.average_color.z, 0.5f);
}

TEST(LodMesher, OnlySomeColumnsHavingGeometryStillAveragesCorrectly) {
    Chunk chunk;
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    // Exactly one column (0,0) has a single stone block at y=9 (real
    // local height 10); every other column stays real air. The average
    // must be computed only over the ONE column that actually has
    // geometry, not divided by the full 256-column count.
    chunk.set_block(0, 9, 0, stone_id);

    const LodChunkMesh mesh = build_lod_chunk(chunk, registry);

    ASSERT_TRUE(mesh.has_geometry);
    EXPECT_FLOAT_EQ(mesh.average_local_height, 10.0f);
}

TEST(LodMesher, TopmostVoxelWinsOverDeeperOnesInTheSameColumn) {
    Chunk chunk;
    BlockId stone_id = 0;
    BlockRegistry registry = make_registry_with_stone(stone_id);
    // Two real blocks stacked in the same column - the real topmost one
    // (y=5) is what should determine this column's own height, not the
    // lower one (y=2) or an average of the two.
    chunk.set_block(0, 2, 0, stone_id);
    chunk.set_block(0, 5, 0, stone_id);

    const LodChunkMesh mesh = build_lod_chunk(chunk, registry);

    ASSERT_TRUE(mesh.has_geometry);
    EXPECT_FLOAT_EQ(mesh.average_local_height, 6.0f);
}

TEST(LodMesher, DifferentColoredColumnsAverageTheirColors) {
    Chunk chunk;
    BlockRegistry registry;
    BlockDefinition red;
    red.namespaced_id = "test:red";
    red.display_name = "Red";
    red.color = {1.0f, 0.0f, 0.0f};
    const BlockId red_id = registry.register_block(red);
    BlockDefinition blue;
    blue.namespaced_id = "test:blue";
    blue.display_name = "Blue";
    blue.color = {0.0f, 0.0f, 1.0f};
    const BlockId blue_id = registry.register_block(blue);

    chunk.set_block(0, 0, 0, red_id);
    chunk.set_block(1, 0, 0, blue_id);

    const LodChunkMesh mesh = build_lod_chunk(chunk, registry);

    ASSERT_TRUE(mesh.has_geometry);
    EXPECT_FLOAT_EQ(mesh.average_color.x, 0.5f);
    EXPECT_FLOAT_EQ(mesh.average_color.y, 0.0f);
    EXPECT_FLOAT_EQ(mesh.average_color.z, 0.5f);
}
