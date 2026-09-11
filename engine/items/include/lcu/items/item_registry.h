#pragma once

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
    // Real hotbar/inventory icon color (Phase 47, RGBA) - deferred back
    // in Phase 44 ("no inventory/hotbar widget exists yet to consume
    // it" - see DECISIONS.md) until a real consumer existed; the HUD
    // hotbar is that consumer. No texture atlas exists (see
    // BlockDefinition::color's own doc comment for why), so an item's
    // "icon" is a flat colored quad, the same approach block tinting
    // already uses. Default white so an item that never sets this still
    // renders as a real, visible (if undistinguished) icon rather than
    // invisible/black.
    math::Vec4 icon_color{1.0f, 1.0f, 1.0f, 1.0f};
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
