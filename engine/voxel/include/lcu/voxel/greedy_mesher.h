#pragma once

#include <vector>

#include "lcu/math/vec3.h"
#include "lcu/voxel/block_registry.h"
#include "lcu/voxel/chunk.h"

namespace lcu::voxel {

struct MeshVertex {
    math::Vec3 position;
    math::Vec3 normal;
    f32 u = 0.0f;
    f32 v = 0.0f;
    // Per-block base tint (Phase 26, BlockDefinition::color) - appended
    // last so the bgfx vertex layout (chunk_mesh_upload.cpp) can add its
    // matching Color0 attribute last too, keeping struct field order and
    // layout attribute order in lockstep (this struct is memcpy'd
    // straight into a GPU buffer, see upload_chunk_mesh_layer).
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    // Packed per-voxel light (Phase 28): low nibble = sky light, high
    // nibble = block light, each 0-15 - the exact same packing
    // lcu::lighting::LightStorage itself uses (see its doc comment), one
    // byte total as required (brief: "Licht wird als EIN Byte im Vertex
    // gepackt"). Default 0xFF (full sky+block light) so any quad built
    // via the light-less mesh_chunk_greedy overload, or any test that
    // doesn't care about lighting, renders unshaded rather than black.
    u8 light = 0xFF;
};

// One renderable layer's worth of geometry: a plain vertex/index buffer,
// deliberately renderer-agnostic (no bgfx types here - engine/rendering
// uploads this into GPU buffers later, once meshing has something to
// feed it). Quads are two triangles, wound so that
// cross(v1-v0, v2-v0) points along the quad's stored normal - see
// greedy_mesher_test.cpp "NormalMatchesGeometricWinding" for the
// automated check backing this, since there is no display in this
// sandbox to check it visually.
struct ChunkMeshLayer {
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;

    // UV origin is the quad's (u=0,v=0) corner; `width`/`height` are in
    // block units along that quad's own two edges. There is no texture
    // atlas yet (Phase 12), so these coordinates aren't validated by
    // anything downstream today - they exist so meshing doesn't need a
    // breaking change once atlas mapping lands.
    void add_quad(const math::Vec3& v0, const math::Vec3& v1, const math::Vec3& v2, const math::Vec3& v3,
                  const math::Vec3& normal, f32 width, f32 height, const math::Vec3& color = {1.0f, 1.0f, 1.0f},
                  u8 light = 0xFF) {
        const u32 base = static_cast<u32>(vertices.size());
        vertices.push_back({v0, normal, 0.0f, 0.0f, color, light});
        vertices.push_back({v1, normal, width, 0.0f, color, light});
        vertices.push_back({v2, normal, width, height, color, light});
        vertices.push_back({v3, normal, 0.0f, height, color, light});

        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    bool empty() const { return vertices.empty(); }
};

struct ChunkMesh {
    ChunkMeshLayer opaque;
    // Populated once a transparent/water block actually exists to mesh
    // against (brief section 18's transparent/water layers). Real, empty
    // members now rather than bolted on later as a breaking change - see
    // DECISIONS.md.
    ChunkMeshLayer transparent;
    ChunkMeshLayer water;
};

namespace detail {

template <u32 EdgeLength>
BlockId block_or_air(const ChunkStorage<EdgeLength>& chunk, i32 x, i32 y, i32 z) {
    constexpr i32 kEdge = static_cast<i32>(EdgeLength);
    if (x < 0 || y < 0 || z < 0 || x >= kEdge || y >= kEdge || z >= kEdge) {
        return kAirBlockId;
    }
    return chunk.block_at(static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z));
}

inline bool is_opaque_block(BlockId id, const BlockRegistry& registry) {
    return !registry.definition_of(id).is_transparent;
}

struct MaskCell {
    BlockId block_id = kAirBlockId;
    bool positive_facing = false;
    bool has_face = false;
    // Packed light (Phase 28, same nibble layout as MeshVertex::light) of
    // the air cell this face is actually exposed to. Included in
    // merges_with so two adjacent faces only merge into one quad when
    // they'd actually be shaded the same - otherwise a run of e.g. lit
    // and shadowed grass would wrongly flatten into a single averaged (or
    // arbitrarily-first-cell) brightness. This does mean a lighting
    // gradient across an otherwise-uniform surface now also fragments
    // meshing there, same trade every engine using per-voxel light
    // (rather than per-quad/per-face light) makes.
    u8 light = 0xFF;

    bool merges_with(const MaskCell& other) const {
        return has_face && other.has_face && block_id == other.block_id &&
               positive_facing == other.positive_facing && light == other.light;
    }
};

}  // namespace detail

// Greedy meshing (brief section 18): sweeps each of the 3 axes' boundary
// planes, builds a 2D "is there a face here, and which block/facing"
// mask per plane, then merges adjacent mask cells of the same block id,
// facing direction, and light level into as few rectangles as possible -
// far fewer triangles than one quad per exposed block face for any chunk
// with runs of same-type, same-lit blocks. Works for any chunk edge
// length (a template over EdgeLength, like ChunkStorage itself, per
// brief section 15's "alternative chunk sizes").
//
// `LightStorageT` is duck-typed (needs `u8 sky_light(u32,u32,u32) const`
// and `u8 block_light(u32,u32,u32) const`, exactly
// lcu::lighting::LightStorage<EdgeLength>'s public interface) rather
// than a concrete lcu::lighting type: engine/lighting already depends on
// engine/voxel (it meshes/lights the same ChunkStorage), so engine/voxel
// including a lighting header back would be a circular target
// dependency - a template parameter needs no #include at all here, only
// at each real call site, which already has the concrete type visible.
template <u32 EdgeLength, typename LightStorageT>
ChunkMesh mesh_chunk_greedy(const ChunkStorage<EdgeLength>& chunk, const BlockRegistry& registry,
                            const LightStorageT& light) {
    ChunkMesh mesh;
    constexpr i32 N = static_cast<i32>(EdgeLength);

    for (i32 d = 0; d < 3; ++d) {
        const i32 axis_u = (d + 1) % 3;
        const i32 axis_v = (d + 2) % 3;

        std::vector<detail::MaskCell> mask(static_cast<usize>(N) * static_cast<usize>(N));

        // plane in [0, N]: the boundary between cell (plane-1) and cell
        // (plane) along axis d. plane-1 == -1 or plane == N means "off
        // the edge of the chunk", treated as air via block_or_air.
        for (i32 plane = 0; plane <= N; ++plane) {
            i32 neg_pos[3] = {0, 0, 0};
            neg_pos[d] = plane - 1;
            i32 pos_pos[3] = {0, 0, 0};
            pos_pos[d] = plane;

            usize n = 0;
            for (i32 jv = 0; jv < N; ++jv) {
                neg_pos[axis_v] = jv;
                pos_pos[axis_v] = jv;
                for (i32 iu = 0; iu < N; ++iu) {
                    neg_pos[axis_u] = iu;
                    pos_pos[axis_u] = iu;

                    const BlockId neg_id = detail::block_or_air(chunk, neg_pos[0], neg_pos[1], neg_pos[2]);
                    const BlockId pos_id = detail::block_or_air(chunk, pos_pos[0], pos_pos[1], pos_pos[2]);
                    const bool neg_opaque = detail::is_opaque_block(neg_id, registry);
                    const bool pos_opaque = detail::is_opaque_block(pos_id, registry);

                    detail::MaskCell cell;
                    if (neg_opaque != pos_opaque) {
                        cell.has_face = true;
                        if (neg_opaque) {
                            // Solid is on the negative side: the visible
                            // face points away from it, along +d.
                            cell.block_id = neg_id;
                            cell.positive_facing = true;
                        } else {
                            cell.block_id = pos_id;
                            cell.positive_facing = false;
                        }

                        // Shade the face by the light in the air cell
                        // it's actually exposed to (the non-opaque side),
                        // not the solid block's own cell (light is only
                        // ever propagated into non-opaque cells - see
                        // engine/lighting/propagation.h). At plane==0/N
                        // the air side can fall off this chunk's own
                        // LightStorage bounds (a genuine chunk-boundary
                        // face, already rendered "as if air" by
                        // block_or_air above) - cross-chunk light isn't
                        // computed yet (Phase 29-31), so this keeps the
                        // Phase 26/27-era full-bright default rather than
                        // reading out of bounds or guessing dark.
                        const i32* air_pos = neg_opaque ? pos_pos : neg_pos;
                        if (air_pos[0] >= 0 && air_pos[0] < N && air_pos[1] >= 0 && air_pos[1] < N &&
                            air_pos[2] >= 0 && air_pos[2] < N) {
                            const u32 ax = static_cast<u32>(air_pos[0]);
                            const u32 ay = static_cast<u32>(air_pos[1]);
                            const u32 az = static_cast<u32>(air_pos[2]);
                            const u8 sky = light.sky_light(ax, ay, az);
                            const u8 block = light.block_light(ax, ay, az);
                            cell.light = static_cast<u8>((block << 4) | sky);
                        }
                    }
                    mask[n++] = cell;
                }
            }

            // Greedy-merge this plane's mask into rectangles.
            n = 0;
            for (i32 jv = 0; jv < N; ++jv) {
                for (i32 iu = 0; iu < N;) {
                    if (!mask[n].has_face) {
                        ++iu;
                        ++n;
                        continue;
                    }

                    const detail::MaskCell current = mask[n];

                    i32 width = 1;
                    while (iu + width < N && mask[n + static_cast<usize>(width)].merges_with(current)) {
                        ++width;
                    }

                    i32 height = 1;
                    bool blocked = false;
                    while (jv + height < N) {
                        for (i32 k = 0; k < width; ++k) {
                            const usize idx = n + static_cast<usize>(k) +
                                               static_cast<usize>(height) * static_cast<usize>(N);
                            if (!mask[idx].merges_with(current)) {
                                blocked = true;
                                break;
                            }
                        }
                        if (blocked) {
                            break;
                        }
                        ++height;
                    }

                    f32 base3[3] = {0.0f, 0.0f, 0.0f};
                    base3[d] = static_cast<f32>(plane);
                    base3[axis_u] = static_cast<f32>(iu);
                    base3[axis_v] = static_cast<f32>(jv);

                    f32 du3[3] = {0.0f, 0.0f, 0.0f};
                    du3[axis_u] = static_cast<f32>(width);
                    f32 dv3[3] = {0.0f, 0.0f, 0.0f};
                    dv3[axis_v] = static_cast<f32>(height);

                    const math::Vec3 c0(base3[0], base3[1], base3[2]);
                    const math::Vec3 c1(base3[0] + du3[0], base3[1] + du3[1], base3[2] + du3[2]);
                    const math::Vec3 c2(base3[0] + du3[0] + dv3[0], base3[1] + du3[1] + dv3[1],
                                         base3[2] + du3[2] + dv3[2]);
                    const math::Vec3 c3(base3[0] + dv3[0], base3[1] + dv3[1], base3[2] + dv3[2]);

                    math::Vec3 normal(0.0f, 0.0f, 0.0f);
                    const f32 sign = current.positive_facing ? 1.0f : -1.0f;
                    if (d == 0) {
                        normal.x = sign;
                    } else if (d == 1) {
                        normal.y = sign;
                    } else {
                        normal.z = sign;
                    }

                    // (axis_u, axis_v) = ((d+1)%3, (d+2)%3) is always an
                    // even cyclic permutation of (X,Y,Z), so
                    // cross(du, dv) points along +axis_d: the winding
                    // c0->c1->c2->c3 is therefore correct for a
                    // positive-facing quad, and reversed
                    // (c0->c3->c2->c1) for a negative-facing one.
                    // Per-face color (Phase 26): d==1 is the Y axis, so
                    // positive_facing there means the top face (normal
                    // +Y) and non-positive_facing means the bottom face
                    // (normal -Y); d==0/d==2 are the four side faces.
                    // See BlockDefinition::color's doc comment for the
                    // fallback chain.
                    const BlockDefinition& def = registry.definition_of(current.block_id);
                    math::Vec3 quad_color = def.color;
                    if (d == 1 && !current.positive_facing) {
                        quad_color = def.bottom_color.value_or(def.side_color.value_or(def.color));
                    } else if (d != 1) {
                        quad_color = def.side_color.value_or(def.color);
                    }
                    if (current.positive_facing) {
                        mesh.opaque.add_quad(c0, c1, c2, c3, normal, static_cast<f32>(width),
                                              static_cast<f32>(height), quad_color, current.light);
                    } else {
                        mesh.opaque.add_quad(c0, c3, c2, c1, normal, static_cast<f32>(width),
                                              static_cast<f32>(height), quad_color, current.light);
                    }

                    for (i32 l = 0; l < height; ++l) {
                        for (i32 k = 0; k < width; ++k) {
                            mask[n + static_cast<usize>(k) + static_cast<usize>(l) * static_cast<usize>(N)]
                                .has_face = false;
                        }
                    }

                    iu += width;
                    n += static_cast<usize>(width);
                }
            }
        }
    }

    return mesh;
}

namespace detail {

// Duck-typed stand-in for lcu::lighting::LightStorage (see
// mesh_chunk_greedy's `LightStorageT` doc comment) that reports every
// cell as fully lit - backs the two-argument mesh_chunk_greedy overload
// below for callers with no real per-chunk light computed yet (existing
// geometry/color-focused unit tests, tools/benchmark), so Phase 28's new
// light parameter didn't force a mechanical, unrelated update of every
// pre-existing call site.
struct FullBrightLight {
    u8 sky_light(u32, u32, u32) const { return 15; }
    u8 block_light(u32, u32, u32) const { return 15; }
};

}  // namespace detail

template <u32 EdgeLength>
ChunkMesh mesh_chunk_greedy(const ChunkStorage<EdgeLength>& chunk, const BlockRegistry& registry) {
    return mesh_chunk_greedy(chunk, registry, detail::FullBrightLight{});
}

}  // namespace lcu::voxel
