#pragma once

#include "lcu/items/inventory.h"
#include "lcu/items/item_registry.h"
#include "lcu/items/item_stack.h"

namespace lcu::items {

// Pure click-driven inventory manipulation (Phase 49.2), the real logic
// behind the inventory screen's drag/drop: every function here takes the
// "held by the mouse cursor" stack as an explicit in/out ItemStack&
// rather than owning any UI/mouse state itself, so it's testable without
// a renderer or a window (same split hud.h/menu_stack.h already use
// between pure layout/logic and their *_renderer.h drawing halves - see
// client/main.cpp for the real caller that owns the actual cursor
// stack).

// Left-click on `slot` (Minecraft's "pick up/place whole stack"):
//  - cursor empty, slot has an item -> cursor takes the whole slot stack,
//    slot becomes empty.
//  - cursor has an item, slot empty -> slot takes the whole cursor stack,
//    cursor becomes empty.
//  - both hold the *same* item -> as much of cursor as fits (up to
//    max_stack_size) moves into slot; any remainder stays in cursor.
//  - both hold *different* items -> the two stacks swap outright.
//  - both empty -> no-op.
void inventory_left_click(Inventory& inventory, const ItemRegistry& registry, usize slot_index, ItemStack& cursor);

// Right-click on `slot` (Minecraft's "half stack / place one"):
//  - cursor empty, slot has an item -> cursor takes ceil(slot.count / 2)
//    (the larger half on an odd count, matching Minecraft), the rest
//    stays in slot.
//  - cursor has an item, slot empty -> exactly one unit moves from
//    cursor into slot.
//  - cursor has an item, slot holds the *same* item and has room
//    (< max_stack_size) -> exactly one unit moves from cursor into slot.
//  - cursor has an item, slot holds a *different* item, or the same item
//    already at max_stack_size -> no-op (nothing to merge, and this
//    isn't a swap like left-click).
//  - both empty -> no-op.
void inventory_right_click(Inventory& inventory, const ItemRegistry& registry, usize slot_index, ItemStack& cursor);

// Shift-click on `source_slot` (Minecraft's "quick transfer to the other
// inventory section"): the whole stack at source_slot moves into
// `target`'s [target_range_begin, target_range_end) slot range - matching
// stacks first, then empty slots, same fill order add_item/
// add_item_to_range already use. Whatever doesn't fit (the target range
// is full) is left behind in source_slot rather than lost. `target` may
// be the very same Inventory as `inventory` (e.g. hotbar <-> main
// storage inside one 36-slot Inventory) as long as the caller passes a
// target range that doesn't overlap source_slot - true for every real
// caller (hotbar and main storage are disjoint ranges of the same
// Inventory; a craft grid and the main inventory are two distinct
// Inventory objects). Returns true if anything actually moved (false for
// an empty source_slot or a target range with no room at all) - callers
// use this to decide whether a transfer is worth logging.
bool inventory_shift_click(Inventory& inventory, usize source_slot, const ItemRegistry& registry, Inventory& target,
                            usize target_range_begin, usize target_range_end);

}  // namespace lcu::items
