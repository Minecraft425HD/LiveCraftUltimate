#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/math/vec3.h"
#include "lcu/voxel/block_id.h"

namespace lcu::voxel {

// Datadriven block definition (brief section 16). Gameplay/meshing code
// queries properties through here, never hardcodes `if (id == stone)`
// (brief section 84 "modding-first"). Fields beyond what Phase 2 meshing
// needs - block states (section 17), sounds, scripted behavior (Phase 9)
// - are added when something actually consumes them, not speculatively
// (brief section 98).
struct BlockDefinition {
    std::string namespaced_id;  // e.g. "game:stone", "example_mod:magic_stone"
    std::string display_name;
    f32 hardness = 1.0f;
    bool is_transparent = false;  // affects meshing (Phase 2): opaque neighbors cull shared faces
    bool has_collision = true;
    u8 light_emission = 0;  // 0-15, brief section 24
    // Base tint (Phase 26) - real per-block color a chunk's mesh vertices
    // carry directly (no texture atlas exists yet, see DECISIONS.md). The
    // fragment shader multiplies this by a procedural noise pattern, not
    // a flat fill - see client/shaders/fs_chunk.sc. Default white so an
    // unset color reads as "untinted", matching every other optional
    // field's zero-value-means-default convention in this file.
    //
    // `color` is the top-face (and default-everywhere) tint;
    // `side_color`/`bottom_color` optionally override it per face -
    // mesh_chunk_greedy already knows which face it's building (the axis
    // + facing direction that drove the greedy sweep), so this is a real,
    // data-driven per-face lookup at mesh time, not a shader-side special
    // case for "this is specifically grass". A block that doesn't set
    // them (most blocks) is uniformly `color` on every face, same as
    // before this field existed. `bottom_color` falls back to
    // `side_color` (then `color`) if unset - most blocks' undersides
    // look like their sides, not their tops (grass: dirt-brown both).
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    std::optional<math::Vec3> side_color;
    std::optional<math::Vec3> bottom_color;

    // Real per-face atlas texture index (Phase 55) - mirrors color/
    // side_color/bottom_color's own fallback chain exactly: `top_
    // texture` is the top-face (and default-everywhere) tile,
    // `side_texture`/`bottom_texture` optionally override it per face,
    // resolved by mesh_chunk_greedy the same way/at the same time it
    // already resolves quad_color. A plain u32, not lcu::assets::
    // TileId directly: engine/voxel is EngineCore-level (no SDL/bgfx,
    // built for VoxelServer too - see ARCHITECTURE.md), so it can't
    // depend on engine/assets (LCU_BUILD_CLIENT-only, see engine/assets/
    // CMakeLists.txt) - each real block's own registration site in
    // client/main.cpp is the only place that actually knows about
    // lcu::assets::TileId, casting to u32 there. Default 0 - a block
    // that never sets this renders atlas tile 0 everywhere, same as
    // every block did before Phase 55 (harmless: nothing samples the
    // atlas at all unless LCU_USE_TEXTURES is on, and even then this
    // is purely which tile gets sampled, not whether one does).
    u32 top_texture = 0;
    std::optional<u32> side_texture;
    std::optional<u32> bottom_texture;

    // Real block-state-to-texture mechanism (Phase 63, the farming
    // foundation's own "unterschiedliche Zustände nutzen unterschiedliche
    // Atlas-Slots" requirement): when true, mesh_chunk_greedy adds the
    // voxel's own real lcu::voxel::ChunkStorage state (0-255) to whichever
    // per-face texture index it already resolved (top/side/bottom, same
    // fallback chain as above) before writing it into the quad - so a
    // block registered with N sequential growth-stage tiles starting at
    // `top_texture` (Phase 64's real wheat: 8 states -> 8 consecutive
    // atlas tiles) needs no new BlockDefinition per stage, just this one
    // flag. False (the default) for every existing block - state never
    // changes their texture, matching pre-Phase-63 behavior exactly.
    bool texture_index_offset_by_state = false;
};

// Central, namespaced block type registry (brief section 16). Namespacing
// follows brief section 52 - "game:stone" vs. "example_mod:magic_stone",
// never a bare global id. BlockId 0 is always air (kAirBlockId),
// registered automatically by the constructor so the constant and the
// registry can never disagree.
class BlockRegistry : public NonCopyable {
   public:
    BlockRegistry();

    // Registers a new block definition and returns its BlockId. Asserts
    // (LCU_VERIFY - always checked, even in Release) if namespaced_id is
    // already registered, or if the registry is full (BlockId is 16-bit:
    // 65536 block types max, see block_id.h).
    BlockId register_block(BlockDefinition definition);

    const BlockDefinition& definition_of(BlockId id) const;

    // Returns nullptr if no block with that namespaced id is registered.
    const BlockDefinition* find_by_namespaced_id(const std::string& namespaced_id) const;

    usize count() const { return definitions_.size(); }

   private:
    std::vector<BlockDefinition> definitions_;
    std::unordered_map<std::string, BlockId> id_lookup_;
};

}  // namespace lcu::voxel
