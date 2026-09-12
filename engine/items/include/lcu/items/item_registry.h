#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/items/item_id.h"
#include "lcu/math/vec4.h"

namespace lcu::items {

// Datadriven item definition (mirrors lcu::voxel::BlockDefinition).
// Gameplay/inventory code queries properties through here, never
// hardcodes `if (id == stick)`. Fields beyond what Phase 5 actually
// consumes (durability, tool tiers, food, scripted behavior - Phase 9)
// are added when something needs them, not speculatively.
struct ItemDefinition {
    std::string namespaced_id;  // e.g. "game:stone", "example_mod:magic_wand"
    std::string display_name;
    u32 max_stack_size = 64;
    // Real hotbar/inventory icon color (Phase 47, RGBA) - the real
    // fallback tint an icon quad renders as when texture_index (below)
    // is unset, and still the real alpha-multiplying tint even when it
    // is set (see engine/ui's own *_renderer.cpp for the exact mix).
    // Default white so an item that never sets either field still
    // renders as a real, visible (if undistinguished) icon rather than
    // invisible/black.
    math::Vec4 icon_color{1.0f, 1.0f, 1.0f, 1.0f};
    // Real atlas texture index (Phase 56, lcu::assets::TileId) - unset
    // (the real default) means "no real icon texture for this item
    // yet", drawing the flat icon_color quad above instead, same as
    // every item did before this phase. A plain std::optional<u32>, not
    // lcu::assets::TileId directly - engine/items is EngineCore-level
    // (built for VoxelServer too, see ARCHITECTURE.md), so it can't
    // depend on engine/assets (LCU_BUILD_CLIENT-only) - each real
    // item's own registration site in client/main.cpp is the only place
    // that actually knows about lcu::assets::TileId, casting to u32
    // there (mirrors BlockDefinition::top_texture's own reasoning
    // exactly).
    std::optional<u32> texture_index;
};

// Central, namespaced item type registry (mirrors BlockRegistry). ItemId
// 0 is always "no item" (kNoItemId), registered automatically by the
// constructor.
class ItemRegistry : public NonCopyable {
   public:
    ItemRegistry();

    // Registers a new item definition and returns its ItemId. Asserts
    // (LCU_VERIFY) if namespaced_id is already registered, or if the
    // registry is full (ItemId is 16-bit: 65536 item types max).
    ItemId register_item(ItemDefinition definition);

    const ItemDefinition& definition_of(ItemId id) const;

    // Returns nullptr if no item with that namespaced id is registered.
    const ItemDefinition* find_by_namespaced_id(const std::string& namespaced_id) const;

    usize count() const { return definitions_.size(); }

   private:
    std::vector<ItemDefinition> definitions_;
    std::unordered_map<std::string, ItemId> id_lookup_;
};

}  // namespace lcu::items
