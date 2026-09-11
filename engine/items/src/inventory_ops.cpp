#include "lcu/items/inventory_ops.h"

#include <algorithm>

namespace lcu::items {

void inventory_left_click(Inventory& inventory, const ItemRegistry& registry, usize slot_index, ItemStack& cursor) {
    const ItemStack slot = inventory.slot_at(slot_index);

    if (cursor.is_empty() && slot.is_empty()) {
        return;
    }
    if (cursor.is_empty()) {
        cursor = slot;
        inventory.set_slot(slot_index, ItemStack{});
        return;
    }
    if (slot.is_empty()) {
        inventory.set_slot(slot_index, cursor);
        cursor = ItemStack{};
        return;
    }
    if (slot.item != cursor.item) {
        inventory.set_slot(slot_index, cursor);
        cursor = slot;
        return;
    }

    // Same item in both - merge as much of cursor into slot as fits.
    const u32 max_stack = registry.definition_of(slot.item).max_stack_size;
    const u32 space = max_stack > slot.count ? max_stack - slot.count : 0;
    const u32 moved = std::min(space, cursor.count);
    inventory.set_slot(slot_index, ItemStack{slot.item, slot.count + moved});
    cursor.count -= moved;
    if (cursor.count == 0) {
        cursor = ItemStack{};
    }
}

void inventory_right_click(Inventory& inventory, const ItemRegistry& registry, usize slot_index, ItemStack& cursor) {
    const ItemStack slot = inventory.slot_at(slot_index);

    if (cursor.is_empty() && slot.is_empty()) {
        return;
    }
    if (cursor.is_empty()) {
        // Take the larger half on an odd count (Minecraft's own rule).
        const u32 taken = (slot.count + 1) / 2;
        cursor = ItemStack{slot.item, taken};
        const u32 remaining = slot.count - taken;
        inventory.set_slot(slot_index, remaining > 0 ? ItemStack{slot.item, remaining} : ItemStack{});
        return;
    }
    if (slot.is_empty()) {
        inventory.set_slot(slot_index, ItemStack{cursor.item, 1});
        cursor.count -= 1;
        if (cursor.count == 0) {
            cursor = ItemStack{};
        }
        return;
    }
    if (slot.item != cursor.item) {
        return;
    }
    const u32 max_stack = registry.definition_of(slot.item).max_stack_size;
    if (slot.count >= max_stack) {
        return;
    }
    inventory.set_slot(slot_index, ItemStack{slot.item, slot.count + 1});
    cursor.count -= 1;
    if (cursor.count == 0) {
        cursor = ItemStack{};
    }
}

bool inventory_shift_click(Inventory& inventory, usize source_slot, const ItemRegistry& registry, Inventory& target,
                            usize target_range_begin, usize target_range_end) {
    const ItemStack source = inventory.slot_at(source_slot);
    if (source.is_empty()) {
        return false;
    }

    inventory.set_slot(source_slot, ItemStack{});
    const u32 leftover = target.add_item_to_range(registry, source, target_range_begin, target_range_end);
    if (leftover > 0) {
        inventory.set_slot(source_slot, ItemStack{source.item, leftover});
    }
    return leftover < source.count;
}

}  // namespace lcu::items
