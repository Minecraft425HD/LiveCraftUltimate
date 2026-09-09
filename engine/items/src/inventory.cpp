#include "lcu/items/inventory.h"

#include <algorithm>

#include "lcu/core/assert.h"

namespace lcu::items {

Inventory::Inventory(usize slot_count) : slots_(slot_count) {}

const ItemStack& Inventory::slot_at(usize index) const {
    LCU_ASSERT(index < slots_.size());
    return slots_[index];
}

void Inventory::set_slot(usize index, ItemStack stack) {
    LCU_ASSERT(index < slots_.size());
    slots_[index] = stack;
}

u32 Inventory::add_item(const ItemRegistry& registry, ItemStack stack) {
    if (stack.item == kNoItemId || stack.count == 0) {
        return stack.count;
    }
    const u32 max_stack = registry.definition_of(stack.item).max_stack_size;

    for (ItemStack& slot : slots_) {
        if (stack.count == 0) {
            break;
        }
        if (slot.item == stack.item && slot.count > 0 && slot.count < max_stack) {
            const u32 space = max_stack - slot.count;
            const u32 added = std::min(space, stack.count);
            slot.count += added;
            stack.count -= added;
        }
    }

    for (ItemStack& slot : slots_) {
        if (stack.count == 0) {
            break;
        }
        if (slot.count == 0) {
            const u32 added = std::min(max_stack, stack.count);
            slot.item = stack.item;
            slot.count = added;
            stack.count -= added;
        }
    }

    return stack.count;
}

u32 Inventory::remove_item(ItemId item, u32 count) {
    u32 removed = 0;
    for (ItemStack& slot : slots_) {
        if (removed == count) {
            break;
        }
        if (slot.item == item && slot.count > 0) {
            const u32 taken = std::min(slot.count, count - removed);
            slot.count -= taken;
            removed += taken;
            if (slot.count == 0) {
                slot.item = kNoItemId;
            }
        }
    }
    return removed;
}

u32 Inventory::count_item(ItemId item) const {
    u32 total = 0;
    for (const ItemStack& slot : slots_) {
        if (slot.item == item) {
            total += slot.count;
        }
    }
    return total;
}

}  // namespace lcu::items
