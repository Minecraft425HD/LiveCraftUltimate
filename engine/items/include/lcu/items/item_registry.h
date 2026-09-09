#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/items/item_id.h"

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
