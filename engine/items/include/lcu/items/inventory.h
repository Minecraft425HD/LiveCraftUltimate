#pragma once

#include <vector>

#include "lcu/core/types.h"
#include "lcu/items/item_registry.h"
#include "lcu/items/item_stack.h"

namespace lcu::items {

// Fixed-size slot-based item storage (brief section 55's inventory
// component). Pure data + stacking logic - no UI/drag-drop yet, since
// there's no inventory screen to drive one (that's a later phase; see
// DECISIONS.md).
class Inventory {
   public:
    explicit Inventory(usize slot_count);

    usize slot_count() const { return slots_.size(); }
    const ItemStack& slot_at(usize index) const;
    void set_slot(usize index, ItemStack stack);

    // Adds as much of `stack` as fits: first into existing slots already
    // holding the same item (topped up to `registry`'s
    // ItemDefinition::max_stack_size for that item), then into empty
    // slots. Returns the leftover count that didn't fit anywhere (0 ==
    // fully added). A no-op (returns stack.count unchanged) for
    // kNoItemId or a zero count.
    u32 add_item(const ItemRegistry& registry, ItemStack stack);

    // Same fill order as add_item (matching stacks first, then empty
    // slots), but confined to the half-open slot index range
    // [range_begin, range_end) - the real primitive shift-click transfer
    // needs (Phase 49): "move this stack into the *other* inventory
    // section" (hotbar <-> main storage, or a craft grid <-> main
    // inventory) has to land only in that section, not anywhere in the
    // whole inventory the plain add_item would search. Asserts the range
    // is in bounds; a range with range_begin >= range_end is asserted
    // against too (callers always pass real, non-empty ranges).
    u32 add_item_to_range(const ItemRegistry& registry, ItemStack stack, usize range_begin, usize range_end);

    // Removes up to `count` of `item`, earliest slots first, emptying a
    // slot (resetting it to kNoItemId) once it reaches zero. Returns how
    // many were actually removed - may be less than `count` if the
    // inventory doesn't hold that many.
    u32 remove_item(ItemId item, u32 count);

    // Total count of `item` currently held across every slot.
    u32 count_item(ItemId item) const;

   private:
    std::vector<ItemStack> slots_;
};

}  // namespace lcu::items
