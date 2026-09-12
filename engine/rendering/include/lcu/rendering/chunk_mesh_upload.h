#pragma once

#include <bgfx/bgfx.h>

#include "lcu/core/types.h"
#include "lcu/voxel/greedy_mesher.h"

namespace lcu::rendering {

// The real bgfx::VertexLayout describing voxel::MeshVertex's exact byte
// layout - the same one upload_chunk_mesh_layer builds internally (see
// its own .cpp), exposed here (Phase 76) so a diagnostic caller can query
// its real, currently-registered attribute offsets/stride (bgfx::
// VertexLayout::getOffset/getStride) instead of hand-deriving them or
// duplicating this construction into a second copy that could silently
// drift out of sync with the one actually used to build GPU buffers.
bgfx::VertexLayout chunk_mesh_vertex_layout();

// GPU-side vertex/index buffers for one ChunkMeshLayer's worth of
// geometry. A plain data holder rather than an RAII type: bgfx resource
// creation/destruction has frame-boundary semantics (destroying a buffer
// still in flight on the render thread is a bgfx-level concern) that
// don't map cleanly onto ordinary C++ object lifetime, so callers create
// and destroy explicitly via the functions below rather than relying on
// a destructor.
struct GpuChunkMesh {
    bgfx::VertexBufferHandle vertex_buffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle index_buffer = BGFX_INVALID_HANDLE;
    u32 index_count = 0;

    bool is_valid() const { return bgfx::isValid(vertex_buffer) && bgfx::isValid(index_buffer); }
};

// Copies `layer`'s vertex/index data into new bgfx GPU buffers. Returns
// a GpuChunkMesh with invalid handles (is_valid() == false) if `layer`
// has no geometry - that's a valid empty result, not an error (an
// all-air chunk meshes to nothing).
//
// This uploads geometry only - there is no shader/draw-call submission
// yet. bgfx requires a compiled shader program (vertex+fragment) to
// actually draw anything, and this repository has no shader compiler
// (shaderc) built yet (bgfx.cmake's BGFX_BUILD_TOOLS is off - see
// third_party/CMakeLists.txt) nor any .sc shader source written. Wiring
// up an actual draw call is a separate, larger task - see TASK_QUEUE.md.
GpuChunkMesh upload_chunk_mesh_layer(const voxel::ChunkMeshLayer& layer);

void destroy_gpu_chunk_mesh(GpuChunkMesh& mesh);

}  // namespace lcu::rendering
