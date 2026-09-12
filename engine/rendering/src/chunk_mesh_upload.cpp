#include "lcu/rendering/chunk_mesh_upload.h"

#include "lcu/core/assert.h"

namespace lcu::rendering {

bgfx::VertexLayout chunk_mesh_vertex_layout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        // Phase 26.
        .add(bgfx::Attrib::Color0, 3, bgfx::AttribType::Float)
        // Phase 53 - real atlas tile index (see voxel::MeshVertex::
        // texture_index's own doc comment for why it's added HERE, right
        // after Color0, rather than after Color1 below): Uint16, not
        // normalized, so the shader receives the raw 0-65535 tile index
        // to decompose into atlas grid coordinates itself (see
        // fs_chunk.sc), the same "raw byte value, unpacked shader-side"
        // convention Color1/light below already establishes.
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Uint16)
        // Phase 28 - packed sky/block light byte (see
        // voxel::MeshVertex::light's doc comment). Uint8, not normalized
        // (normalized defaults to false), so the fragment shader receives
        // the raw 0-255 byte value to unpack itself - must stay last,
        // matching MeshVertex::light being the last struct field (this
        // layout describes the exact byte layout of that struct; see
        // upload_chunk_mesh_layer below).
        .add(bgfx::Attrib::Color1, 1, bgfx::AttribType::Uint8);

    // voxel::MeshVertex ends with that single trailing u8 right after
    // several 4-byte-aligned fields, so the compiler pads the struct's
    // total *size* (not this last field itself) up to a 4-byte multiple
    // for array-of-struct alignment - bytes bgfx's tightly-packed sum of
    // .add() calls above doesn't know about. skip() tells it to advance
    // its stride by the same trailing amount so the two agree exactly;
    // without this, every vertex after the first would read from the
    // wrong offset (this struct is memcpy'd straight into the GPU
    // buffer per vertex - see upload_chunk_mesh_layer below).
    layout.skip(static_cast<u8>(sizeof(voxel::MeshVertex) - layout.getStride()));
    layout.end();
    LCU_ASSERT(layout.getStride() == sizeof(voxel::MeshVertex));
    return layout;
}

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
