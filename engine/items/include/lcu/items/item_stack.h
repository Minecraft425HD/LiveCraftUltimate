#pragma once

#include "lcu/core/types.h"
#include "lcu/items/item_id.h"

namespace lcu::items {

// A quantity of one item type occupying one inventory slot (or one
// crafting-grid cell). A default-constructed (zero-initialized)
// ItemStack is always a valid empty slot: item == kNoItemId, count == 0.
struct ItemStack {
    ItemId item = kNoItemId;
    u32 count = 0;

    bool is_empty() const { return count == 0; }
};

}  // namespace lcu::items
