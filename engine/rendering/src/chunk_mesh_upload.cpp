#include "lcu/rendering/chunk_mesh_upload.h"

namespace lcu::rendering {

namespace {

bgfx::VertexLayout chunk_mesh_vertex_layout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        // Phase 26 - must stay last, matching voxel::MeshVertex::color
        // being the last struct field (this layout describes the exact
        // byte layout of that struct; see upload_chunk_mesh_layer below).
        .add(bgfx::Attrib::Color0, 3, bgfx::AttribType::Float)
        .end();
    return layout;
}

}  // namespace

GpuChunkMesh upload_chunk_mesh_layer(const voxel::ChunkMeshLayer& layer) {
    GpuChunkMesh result;
    if (layer.vertices.empty() || layer.indices.empty()) {
        return result;
    }

    const bgfx::VertexLayout layout = chunk_mesh_vertex_layout();

    const bgfx::Memory* vertex_mem = bgfx::copy(
        layer.vertices.data(), static_cast<u32>(layer.vertices.size() * sizeof(voxel::MeshVertex)));
    result.vertex_buffer = bgfx::createVertexBuffer(vertex_mem, layout);

    const bgfx::Memory* index_mem =
        bgfx::copy(layer.indices.data(), static_cast<u32>(layer.indices.size() * sizeof(u32)));
    result.index_buffer = bgfx::createIndexBuffer(index_mem, BGFX_BUFFER_INDEX32);

    result.index_count = static_cast<u32>(layer.indices.size());
    return result;
}

void destroy_gpu_chunk_mesh(GpuChunkMesh& mesh) {
    if (bgfx::isValid(mesh.vertex_buffer)) {
        bgfx::destroy(mesh.vertex_buffer);
        mesh.vertex_buffer = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(mesh.index_buffer)) {
        bgfx::destroy(mesh.index_buffer);
        mesh.index_buffer = BGFX_INVALID_HANDLE;
    }
    mesh.index_count = 0;
}

}  // namespace lcu::rendering
