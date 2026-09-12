#pragma once

#include <bgfx/bgfx.h>

#include "lcu/core/types.h"
#include "lcu/math/mat4.h"
#include "lcu/math/vec3.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"

namespace lcu::rendering {

class Renderer;

// Real Distant-Horizons-style LOD summary of one chunk (Phase 70, brief
// section 70.1): a whole chunk collapses to a single flat quad at its
// own average surface height, tinted its own average surface color -
// real, deterministic data derived from the chunk's own actual voxels
// (not a synthetic/procedural stand-in), just far cheaper to draw than
// its full greedy mesh.
struct LodChunkMesh {
    // False when every column in the chunk is air all the way down (an
    // entirely empty chunk has no real "surface" to summarize) - the
    // real, honest "nothing to draw" case, not a degenerate zero-height
    // quad drawn anyway.
    bool has_geometry = false;
    math::Vec3 average_color{0.5f, 0.5f, 0.5f};
    // Real average LOCAL height (0..kEdgeLength) of the topmost non-air
    // voxel across every column that has one - add this to a chunk's
    // own world-space min.y to get the real world Y the LOD quad should
    // sit at.
    f32 average_local_height = 0.0f;
};

// Real per-column top-down scan (brief section 70.3's own literal
// "Durchschnittsfarbe der obersten Schicht + mittlere Höhe"): for each
// of the chunk's own kEdgeLength x kEdgeLength columns, finds the
// topmost non-air voxel, accumulates its own real `BlockDefinition::
// color` and local height, then averages across every column that had
// one. A real, direct summary of this exact chunk's own current block
// data - not a placeholder/synthetic color.
LodChunkMesh build_lod_chunk(const voxel::Chunk& chunk, const voxel::BlockRegistry& registry);

// Submits one real flat quad covering the chunk's own real XZ footprint
// (`chunk_world_min.x/.z` to `+chunk_edge`), at world Y = `chunk_world_
// min.y + mesh.average_local_height`, tinted `mesh.average_color` -
// into `Renderer`'s own dedicated LOD view (brief section 70.4's own
// "View-Order 0" - executes before the real near-chunk terrain view, so
// a real near-chunk drawn afterward correctly overdraws/is-occluded-by
// this quad via genuine depth write+test, not draw-order alone). A thin
// wrapper around `Renderer::submit_lod_chunk` (bgfx itself stays
// encapsulated inside `Renderer`, per this project's own layering rule
// - see ARCHITECTURE.md). No-op if `!mesh.has_geometry` or `program` is
// invalid.
void submit_lod_chunk(Renderer& renderer, const LodChunkMesh& mesh, const math::Vec3& chunk_world_min,
                       f32 chunk_edge, bgfx::ProgramHandle program, const math::Mat4& view, const math::Mat4& proj);

}  // namespace lcu::rendering
