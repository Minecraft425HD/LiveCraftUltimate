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
