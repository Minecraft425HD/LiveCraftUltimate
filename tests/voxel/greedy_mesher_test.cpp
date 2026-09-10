#include "lcu/voxel/greedy_mesher.h"

#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "lcu/lighting/light_storage.h"

using lcu::voxel::BlockDefinition;
using lcu::voxel::BlockRegistry;
using lcu::voxel::Chunk;
using lcu::voxel::ChunkMesh;
using lcu::voxel::mesh_chunk_greedy;

namespace {

lcu::voxel::BlockId register_opaque(BlockRegistry& registry, const std::string& id) {
    BlockDefinition def;
    def.namespaced_id = id;
    def.is_transparent = false;
    return registry.register_block(def);
}

lcu::voxel::BlockId register_transparent(BlockRegistry& registry, const std::string& id) {
    BlockDefinition def;
    def.namespaced_id = id;
    def.is_transparent = true;
    def.has_collision = false;
    return registry.register_block(def);
}

// Every triangle's geometric winding (right-hand rule on its edges) must
// match its stored vertex normal - this is the property a renderer's
// backface culling actually depends on, and the only way to check
// winding correctness without a display in this sandbox.
void expect_all_triangles_wound_correctly(const lcu::voxel::ChunkMeshLayer& layer) {
    ASSERT_EQ(layer.indices.size() % 3, 0u);
    for (std::size_t i = 0; i < layer.indices.size(); i += 3) {
        const auto& v0 = layer.vertices[layer.indices[i]];
        const auto& v1 = layer.vertices[layer.indices[i + 1]];
        const auto& v2 = layer.vertices[layer.indices[i + 2]];

        const lcu::math::Vec3 edge1 = v1.position - v0.position;
        const lcu::math::Vec3 edge2 = v2.position - v0.position;
        const lcu::math::Vec3 geometric_normal = lcu::math::normalize(lcu::math::cross(edge1, edge2));

        EXPECT_GT(lcu::math::dot(geometric_normal, v0.normal), 0.99f)
            << "triangle at index " << i << " geometric normal doesn't match stored normal";
    }
}

}  // namespace

TEST(GreedyMesher, EmptyChunkProducesEmptyMesh) {
    BlockRegistry registry;
    Chunk chunk;

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    EXPECT_TRUE(mesh.opaque.empty());
    EXPECT_TRUE(mesh.transparent.empty());
    EXPECT_TRUE(mesh.water.empty());
}

TEST(GreedyMesher, SingleIsolatedBlockProducesSixUnmergedFaces) {
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    chunk.set_block(5, 5, 5, stone);

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    EXPECT_EQ(mesh.opaque.vertices.size(), 6u * 4u);
    EXPECT_EQ(mesh.opaque.indices.size(), 6u * 6u);
    EXPECT_TRUE(mesh.transparent.empty());
    expect_all_triangles_wound_correctly(mesh.opaque);
}

TEST(GreedyMesher, PerFaceColorUsesTopSideBottomFallbackChain) {
    // Phase 26: a block with distinct top/side/bottom colors (the real
    // grass-block convention - green top, brown sides, brown-by-
    // fallback bottom) must mesh each of its six faces with the correct
    // color, purely from BlockDefinition data - no shader-side special
    // casing needed.
    BlockRegistry registry;
    BlockDefinition grass_like;
    grass_like.namespaced_id = "test:grass";
    grass_like.color = {0.3f, 0.7f, 0.2f};        // top
    grass_like.side_color = {0.4f, 0.25f, 0.1f};  // sides; bottom_color left unset
    const auto grass = registry.register_block(grass_like);

    Chunk chunk;
    chunk.set_block(5, 5, 5, grass);

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);
    ASSERT_EQ(mesh.opaque.vertices.size(), 6u * 4u);

    const auto color_of_face_with_normal = [&](const lcu::math::Vec3& normal) {
        for (const auto& vertex : mesh.opaque.vertices) {
            if (lcu::math::dot(vertex.normal, normal) > 0.99f) {
                return vertex.color;
            }
        }
        ADD_FAILURE() << "no vertex found with the expected face normal";
        return lcu::math::Vec3{};
    };

    const lcu::math::Vec3 top_color = color_of_face_with_normal({0.0f, 1.0f, 0.0f});
    const lcu::math::Vec3 bottom_color = color_of_face_with_normal({0.0f, -1.0f, 0.0f});
    const lcu::math::Vec3 side_color = color_of_face_with_normal({1.0f, 0.0f, 0.0f});

    EXPECT_FLOAT_EQ(top_color.x, 0.3f);
    EXPECT_FLOAT_EQ(top_color.y, 0.7f);
    EXPECT_FLOAT_EQ(top_color.z, 0.2f);

    // bottom_color unset -> falls back to side_color, not top color().
    EXPECT_FLOAT_EQ(bottom_color.x, 0.4f);
    EXPECT_FLOAT_EQ(bottom_color.y, 0.25f);
    EXPECT_FLOAT_EQ(bottom_color.z, 0.1f);

    EXPECT_FLOAT_EQ(side_color.x, 0.4f);
    EXPECT_FLOAT_EQ(side_color.y, 0.25f);
    EXPECT_FLOAT_EQ(side_color.z, 0.1f);
}

TEST(GreedyMesher, UnsetSideAndBottomColorFallBackToTopColorOnEveryFace) {
    BlockRegistry registry;
    BlockDefinition stone;
    stone.namespaced_id = "test:stone";
    stone.color = {0.5f, 0.5f, 0.5f};
    const auto stone_id = registry.register_block(stone);

    Chunk chunk;
    chunk.set_block(5, 5, 5, stone_id);

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);
    ASSERT_EQ(mesh.opaque.vertices.size(), 6u * 4u);
    for (const auto& vertex : mesh.opaque.vertices) {
        EXPECT_FLOAT_EQ(vertex.color.x, 0.5f);
        EXPECT_FLOAT_EQ(vertex.color.y, 0.5f);
        EXPECT_FLOAT_EQ(vertex.color.z, 0.5f);
    }
}

TEST(GreedyMesher, AdjacentSameTypeBlocksMergeCoplanarFaces) {
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    chunk.set_block(5, 5, 5, stone);
    chunk.set_block(6, 5, 5, stone);  // adjacent along +X

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    // 2 unmerged X-axis cap faces (different planes, can't merge) + 2
    // merged Y-axis faces (2x1) + 2 merged Z-axis faces (2x1) = 6 quads,
    // versus 10 quads if nothing merged (two 5-visible-face blocks after
    // culling their shared internal face). Proves merging actually
    // reduces triangle count, not just that culling works.
    EXPECT_EQ(mesh.opaque.vertices.size(), 6u * 4u);
    EXPECT_EQ(mesh.opaque.indices.size(), 6u * 6u);
    expect_all_triangles_wound_correctly(mesh.opaque);
}

TEST(GreedyMesher, AdjacentDifferentTypeBlocksDoNotMerge) {
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    const auto dirt = register_opaque(registry, "test:dirt");
    Chunk chunk;
    chunk.set_block(5, 5, 5, stone);
    chunk.set_block(6, 5, 5, dirt);

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    // Shared internal face is still culled (both opaque), but nothing
    // merges across the block-type boundary: 5 unmerged faces per block.
    EXPECT_EQ(mesh.opaque.vertices.size(), 10u * 4u);
    EXPECT_EQ(mesh.opaque.indices.size(), 10u * 6u);
    expect_all_triangles_wound_correctly(mesh.opaque);
}

TEST(GreedyMesher, TransparentNeighborDoesNotCullOpaqueBlockFace) {
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    const auto glass = register_transparent(registry, "test:glass");
    Chunk chunk;
    chunk.set_block(5, 5, 5, stone);
    chunk.set_block(6, 5, 5, glass);  // stands in for "air" on this side

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    // The stone block should still get all 6 faces: a transparent
    // neighbor culls exactly like air (registry-driven, not a hardcoded
    // air-id check). Glass itself never gets meshed into the opaque
    // layer, and there's no transparent-layer meshing yet (see
    // DECISIONS.md), so glass contributes nothing here.
    EXPECT_EQ(mesh.opaque.vertices.size(), 6u * 4u);
    EXPECT_EQ(mesh.opaque.indices.size(), 6u * 6u);
}

TEST(GreedyMesher, TwoAdjacentTransparentBlocksProduceNoOpaqueFaces) {
    BlockRegistry registry;
    const auto glass = register_transparent(registry, "test:glass");
    Chunk chunk;
    chunk.set_block(5, 5, 5, glass);
    chunk.set_block(6, 5, 5, glass);

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    // Known simplification, not a bug: transparent-vs-transparent never
    // draws a face here (even between two *different* transparent
    // materials, e.g. glass touching water), since no transparent-layer
    // meshing exists yet - see DECISIONS.md. Revisit once a real
    // transparent block exists to motivate it.
    EXPECT_TRUE(mesh.opaque.empty());
    EXPECT_TRUE(mesh.transparent.empty());
}

TEST(GreedyMesher, BlockAtChunkBoundaryStillProducesBoundaryFace) {
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    chunk.set_block(0, 0, 0, stone);  // corner of the chunk

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    // Still 6 faces: out-of-chunk neighbors are treated as air (see
    // block_or_air), not as "no face" or an out-of-bounds crash.
    EXPECT_EQ(mesh.opaque.vertices.size(), 6u * 4u);
    expect_all_triangles_wound_correctly(mesh.opaque);
}

TEST(GreedyMesher, LargeFlatSlabMergesIntoTwoFacesPerAxisPair) {
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    for (lcu::u32 x = 0; x < Chunk::kEdgeLength; ++x) {
        for (lcu::u32 z = 0; z < Chunk::kEdgeLength; ++z) {
            chunk.set_block(x, 0, z, stone);
        }
    }

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);

    // A full 16x1x16 slab: top and bottom should each merge into a
    // single 16x16 quad; the four side walls each merge into a single
    // 16x1 quad. Total: 6 quads regardless of the slab covering 256
    // blocks - the whole point of greedy meshing.
    EXPECT_EQ(mesh.opaque.vertices.size(), 6u * 4u);
    EXPECT_EQ(mesh.opaque.indices.size(), 6u * 6u);
    expect_all_triangles_wound_correctly(mesh.opaque);
}

TEST(GreedyMesher, NoLightArgumentOverloadProducesFullBrightVertices) {
    // The two-argument overload (Phase 16-27's existing signature) must
    // keep working unchanged for every caller that doesn't care about
    // lighting (tools/benchmark, the geometry/color tests above) -
    // backed by a duck-typed "everything is fully lit" LightStorageT
    // stand-in, not a breaking API change.
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    chunk.set_block(5, 5, 5, stone);

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry);
    ASSERT_FALSE(mesh.opaque.vertices.empty());
    for (const auto& vertex : mesh.opaque.vertices) {
        EXPECT_EQ(vertex.light, 0xFFu);
    }
}

TEST(GreedyMesher, FacePicksUpLightFromTheExposedAirCellNotTheSolidBlock) {
    // Phase 28: a face's light comes from the air cell it's actually
    // exposed to (where lcu::lighting propagation actually stores real
    // values - solid cells are never targeted by flood_block_light/
    // compute_sky_light_column), packed the same way LightStorage packs
    // it internally (low nibble sky, high nibble block).
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    chunk.set_block(5, 5, 5, stone);

    lcu::lighting::Light light;
    light.set_sky_light(6, 5, 5, 9);  // the +X air neighbor's cell
    light.set_block_light(6, 5, 5, 3);

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry, light);

    bool found_positive_x_face = false;
    for (const auto& vertex : mesh.opaque.vertices) {
        if (lcu::math::dot(vertex.normal, lcu::math::Vec3{1.0f, 0.0f, 0.0f}) > 0.99f) {
            found_positive_x_face = true;
            EXPECT_EQ(vertex.light, static_cast<lcu::u8>((3 << 4) | 9));
        }
    }
    EXPECT_TRUE(found_positive_x_face);
}

TEST(GreedyMesher, DifferentlyLitCoplanarFacesDoNotMerge) {
    // Same block type, same plane, same facing - would merge under
    // Phase 26's rules - but different light at each face's exposed air
    // cell. Merging them would flatten a real per-voxel brightness
    // difference into one arbitrary value, so merges_with must treat
    // differing light as a hard merge boundary, same as a differing
    // block id.
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    chunk.set_block(5, 5, 5, stone);
    chunk.set_block(6, 5, 5, stone);  // adjacent along +X

    lcu::lighting::Light light;
    light.set_sky_light(5, 6, 5, 15);  // top face of (5,5,5): bright
    light.set_sky_light(6, 6, 5, 2);   // top face of (6,5,5): dim

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry, light);

    int top_face_quads = 0;
    for (std::size_t i = 0; i + 3 < mesh.opaque.vertices.size(); i += 4) {
        if (lcu::math::dot(mesh.opaque.vertices[i].normal, lcu::math::Vec3{0.0f, 1.0f, 0.0f}) > 0.99f) {
            ++top_face_quads;
        }
    }
    // Would be 1 merged 2x1 quad if light were ignored (see
    // AdjacentSameTypeBlocksMergeCoplanarFaces) - differing light keeps
    // them as 2 separate 1x1 quads instead.
    EXPECT_EQ(top_face_quads, 2);
}

TEST(GreedyMesher, BoundaryFaceWithNoNeighborChunkLightDefaultsToFullBright) {
    // Phase 28 scope: cross-chunk light doesn't exist yet (Phase 29-31),
    // so a face at plane==0/N whose "air" side falls outside this
    // chunk's own LightStorage bounds must not read out of bounds or
    // guess dark - it keeps the same full-bright default used before
    // real per-voxel light existed, honestly deferring correctness
    // there to the phases that actually add cross-chunk data. A block
    // at the x=0 corner has both boundary faces (its own -X/-Y/-Z,
    // exposed to the *neighbor* chunk this LightStorage knows nothing
    // about) and ordinary in-chunk faces (+X/+Y/+Z, exposed to real
    // in-chunk air cells this LightStorage legitimately reports as dark
    // since it's default-constructed) - only the -X face below is
    // actually a boundary face.
    BlockRegistry registry;
    const auto stone = register_opaque(registry, "test:stone");
    Chunk chunk;
    chunk.set_block(0, 0, 0, stone);

    lcu::lighting::Light light;  // default-constructed: every in-bounds cell is 0/0

    const ChunkMesh mesh = mesh_chunk_greedy(chunk, registry, light);

    bool found_negative_x_face = false;
    for (const auto& vertex : mesh.opaque.vertices) {
        if (lcu::math::dot(vertex.normal, lcu::math::Vec3{-1.0f, 0.0f, 0.0f}) > 0.99f) {
            found_negative_x_face = true;
            EXPECT_EQ(vertex.light, 0xFFu);
        }
    }
    EXPECT_TRUE(found_negative_x_face);
}
