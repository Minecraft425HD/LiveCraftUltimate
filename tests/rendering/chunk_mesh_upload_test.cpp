#include "lcu/rendering/chunk_mesh_upload.h"

#include <gtest/gtest.h>

#include "lcu/rendering/renderer.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"
#include "lcu/voxel/greedy_mesher.h"

using lcu::rendering::destroy_gpu_chunk_mesh;
using lcu::rendering::Renderer;
using lcu::rendering::RendererDesc;
using lcu::rendering::upload_chunk_mesh_layer;
using lcu::voxel::BlockDefinition;
using lcu::voxel::BlockRegistry;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkMeshLayer;
using lcu::voxel::mesh_chunk_greedy;

namespace {

// Each test owns its own bgfx lifecycle via a headless Renderer (Noop
// backend - no GPU/display in this sandbox), constructed in the test
// body itself (Renderer has no move constructor - it wraps bgfx's
// process-wide singleton state, and NRVO returning it from a helper
// function is not guaranteed by the standard). bgfx is a singleton: only
// one Renderer may be initialized at a time, and it must be fully shut
// down (the Renderer destructor calls bgfx::shutdown()) before the next
// test's Renderer initializes a new one - GoogleTest runs tests
// sequentially within one process by default, so that's satisfied here.
RendererDesc headless_desc() {
    RendererDesc desc;
    desc.force_headless = true;
    return desc;
}

}  // namespace

TEST(ChunkMeshUpload, EmptyLayerProducesInvalidHandles) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    ChunkMeshLayer empty_layer;
    auto gpu_mesh = upload_chunk_mesh_layer(empty_layer);

    EXPECT_FALSE(gpu_mesh.is_valid());
    EXPECT_EQ(gpu_mesh.index_count, 0u);
}

TEST(ChunkMeshUpload, NonEmptyLayerProducesValidHandles) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    ChunkMeshLayer layer;
    layer.add_quad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 1}, 1.0f, 1.0f);

    auto gpu_mesh = upload_chunk_mesh_layer(layer);

    EXPECT_TRUE(gpu_mesh.is_valid());
    EXPECT_EQ(gpu_mesh.index_count, 6u);

    destroy_gpu_chunk_mesh(gpu_mesh);
    EXPECT_FALSE(gpu_mesh.is_valid());
}

TEST(ChunkMeshUpload, FullPipelineFromChunkToGpuBuffers) {
    Renderer renderer;
    ASSERT_TRUE(renderer.init(headless_desc()));

    BlockRegistry registry;
    BlockDefinition stone;
    stone.namespaced_id = "test:stone";
    stone.is_transparent = false;
    const auto stone_id = registry.register_block(stone);

    Chunk chunk;
    chunk.set_block(5, 5, 5, stone_id);

    const auto mesh = mesh_chunk_greedy(chunk, registry);
    ASSERT_FALSE(mesh.opaque.empty());

    auto gpu_mesh = upload_chunk_mesh_layer(mesh.opaque);

    EXPECT_TRUE(gpu_mesh.is_valid());
    EXPECT_EQ(gpu_mesh.index_count, mesh.opaque.indices.size());

    destroy_gpu_chunk_mesh(gpu_mesh);
}
