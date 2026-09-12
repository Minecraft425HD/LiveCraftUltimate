#include "lcu/rendering/lod_mesher.h"

#include "lcu/rendering/renderer.h"

namespace lcu::rendering {

LodChunkMesh build_lod_chunk(const voxel::Chunk& chunk, const voxel::BlockRegistry& registry) {
    constexpr u32 kEdge = voxel::Chunk::kEdgeLength;

    math::Vec3 color_sum{0.0f, 0.0f, 0.0f};
    f32 height_sum = 0.0f;
    u32 columns_with_geometry = 0;

    for (u32 x = 0; x < kEdge; ++x) {
        for (u32 z = 0; z < kEdge; ++z) {
            for (u32 y = kEdge; y-- > 0;) {
                const voxel::BlockId id = chunk.block_at(x, y, z);
                if (id == voxel::kAirBlockId) {
                    continue;
                }
                color_sum += registry.definition_of(id).color;
                height_sum += static_cast<f32>(y + 1);
                ++columns_with_geometry;
                break;
            }
        }
    }

    LodChunkMesh mesh;
    if (columns_with_geometry > 0) {
        mesh.has_geometry = true;
        mesh.average_color = color_sum / static_cast<f32>(columns_with_geometry);
        mesh.average_local_height = height_sum / static_cast<f32>(columns_with_geometry);
    }
    return mesh;
}

void submit_lod_chunk(Renderer& renderer, const LodChunkMesh& mesh, const math::Vec3& chunk_world_min,
                       f32 chunk_edge, bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj) {
    if (!mesh.has_geometry) {
        return;
    }
    renderer.submit_lod_chunk(chunk_world_min, chunk_edge, mesh.average_local_height, mesh.average_color, program,
                               view, proj);
}

}  // namespace lcu::rendering
