#include "lcu/items/inventory_ops.h"

#include <gtest/gtest.h>

using lcu::items::Inventory;
using lcu::items::inventory_left_click;
using lcu::items::inventory_right_click;
using lcu::items::inventory_shift_click;
using lcu::items::ItemDefinition;
using lcu::items::ItemRegistry;
using lcu::items::ItemStack;
using lcu::items::kNoItemId;

namespace {

ItemRegistry make_registry_with_stone_and_wand(lcu::items::ItemId& out_stone, lcu::items::ItemId& out_wand) {
    ItemRegistry registry;

    ItemDefinition stone;
    stone.namespaced_id = "game:stone";
    stone.max_stack_size = 64;
    out_stone = registry.register_item(stone);

    ItemDefinition wand;
    wand.namespaced_id = "game:wand";
    wand.max_stack_size = 1;
    out_wand = registry.register_item(wand);

    return registry;
}

}  // namespace

TEST(InventoryLeftClick, PicksUpWholeStackFromSlotIntoEmptyCursor) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 20});
    ItemStack cursor;

    inventory_left_click(inventory, registry, 0, cursor);

    EXPECT_EQ(cursor.item, stone_id);
    EXPECT_EQ(cursor.count, 20u);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
}

TEST(InventoryLeftClick, PlacesWholeCursorStackIntoEmptySlot) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    ItemStack cursor{stone_id, 15};

    inventory_left_click(inventory, registry, 2, cursor);

    EXPECT_TRUE(cursor.is_empty());
    EXPECT_EQ(inventory.slot_at(2).item, stone_id);
    EXPECT_EQ(inventory.slot_at(2).count, 15u);
}

TEST(InventoryLeftClick, MergesSameItemUpToMaxStackLeavingRemainderInCursor) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 50});
    ItemStack cursor{stone_id, 30};

    inventory_left_click(inventory, registry, 0, cursor);

    EXPECT_EQ(inventory.slot_at(0).count, 64u);  // topped up to max
    EXPECT_EQ(cursor.item, stone_id);
    EXPECT_EQ(cursor.count, 16u);  // 30 - (64 - 50) leftover
}

TEST(InventoryLeftClick, SwapsDifferentItemsOutright) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 5});
    ItemStack cursor{wand_id, 1};

    inventory_left_click(inventory, registry, 0, cursor);

    EXPECT_EQ(inventory.slot_at(0).item, wand_id);
    EXPECT_EQ(inventory.slot_at(0).count, 1u);
    EXPECT_EQ(cursor.item, stone_id);
    EXPECT_EQ(cursor.count, 5u);
}

TEST(InventoryLeftClick, BothEmptyIsNoOp) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    ItemStack cursor;

    inventory_left_click(inventory, registry, 0, cursor);

    EXPECT_TRUE(cursor.is_empty());
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
}

TEST(InventoryRightClick, TakesLargerHalfOfOddSlotIntoEmptyCursor) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 7});
    ItemStack cursor;

    inventory_right_click(inventory, registry, 0, cursor);

    EXPECT_EQ(cursor.item, stone_id);
    EXPECT_EQ(cursor.count, 4u);  // ceil(7/2)
    EXPECT_EQ(inventory.slot_at(0).item, stone_id);
    EXPECT_EQ(inventory.slot_at(0).count, 3u);
}

TEST(InventoryRightClick, TakingHalfOfSingleItemEmptiesTheSlot) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 1});
    ItemStack cursor;

    inventory_right_click(inventory, registry, 0, cursor);

    EXPECT_EQ(cursor.count, 1u);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
}

TEST(InventoryRightClick, PlacesExactlyOneFromCursorIntoEmptySlot) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    ItemStack cursor{stone_id, 10};

    inventory_right_click(inventory, registry, 1, cursor);

    EXPECT_EQ(inventory.slot_at(1).item, stone_id);
    EXPECT_EQ(inventory.slot_at(1).count, 1u);
    EXPECT_EQ(cursor.count, 9u);
}

TEST(InventoryRightClick, PlacingLastCursorUnitEmptiesCursor) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    ItemStack cursor{stone_id, 1};

    inventory_right_click(inventory, registry, 1, cursor);

    EXPECT_TRUE(cursor.is_empty());
}

TEST(InventoryRightClick, AddsOneToMatchingSlotWithRoom) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 5});
    ItemStack cursor{stone_id, 10};

    inventory_right_click(inventory, registry, 0, cursor);

    EXPECT_EQ(inventory.slot_at(0).count, 6u);
    EXPECT_EQ(cursor.count, 9u);
}

TEST(InventoryRightClick, NoOpWhenSlotIsAtMaxStack) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 64});
    ItemStack cursor{stone_id, 10};

    inventory_right_click(inventory, registry, 0, cursor);

    EXPECT_EQ(inventory.slot_at(0).count, 64u);
    EXPECT_EQ(cursor.count, 10u);
}

TEST(InventoryRightClick, NoOpOnMismatchedNonEmptySlot) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{wand_id, 1});
    ItemStack cursor{stone_id, 10};

    inventory_right_click(inventory, registry, 0, cursor);

    EXPECT_EQ(inventory.slot_at(0).item, wand_id);
    EXPECT_EQ(cursor.count, 10u);
}

TEST(InventoryShiftClick, MovesWholeStackIntoTargetRangeEmptySlot) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 20});

    const bool moved = inventory_shift_click(inventory, 0, registry, inventory, 2, 4);

    EXPECT_TRUE(moved);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
    EXPECT_EQ(inventory.slot_at(2).item, stone_id);
    EXPECT_EQ(inventory.slot_at(2).count, 20u);
}

TEST(InventoryShiftClick, MergesIntoExistingStackInTargetRangeFirst) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 20});
    inventory.set_slot(2, ItemStack{stone_id, 5});

    const bool moved = inventory_shift_click(inventory, 0, registry, inventory, 2, 4);

    EXPECT_TRUE(moved);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
    EXPECT_EQ(inventory.slot_at(2).count, 25u);
}

TEST(InventoryShiftClick, LeavesUnfittableRemainderInSourceSlot) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(3);
    inventory.set_slot(0, ItemStack{stone_id, 100});
    inventory.set_slot(2, ItemStack{stone_id, 60});  // target range is just slot 2, room for 4 more

    const bool moved = inventory_shift_click(inventory, 0, registry, inventory, 2, 3);

    EXPECT_TRUE(moved);
    EXPECT_EQ(inventory.slot_at(2).count, 64u);
    EXPECT_EQ(inventory.slot_at(0).item, stone_id);
    EXPECT_EQ(inventory.slot_at(0).count, 96u);  // 100 - 4 that fit
}

TEST(InventoryShiftClick, EmptySourceSlotIsNoOpAndReturnsFalse) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(4);

    const bool moved = inventory_shift_click(inventory, 0, registry, inventory, 2, 4);

    EXPECT_FALSE(moved);
}

TEST(InventoryShiftClick, FullTargetRangeReturnsFalseAndLeavesSourceUntouched) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory inventory(3);
    inventory.set_slot(0, ItemStack{stone_id, 10});
    inventory.set_slot(2, ItemStack{stone_id, 64});  // target range (slot 2 only) already full

    const bool moved = inventory_shift_click(inventory, 0, registry, inventory, 2, 3);

    EXPECT_FALSE(moved);
    EXPECT_EQ(inventory.slot_at(0).count, 10u);
    EXPECT_EQ(inventory.slot_at(2).count, 64u);
}

TEST(InventoryShiftClick, WorksAcrossTwoDistinctInventoryObjects) {
    lcu::items::ItemId stone_id = 0, wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);
    Inventory craft_grid(5);
    Inventory main_inventory(36);
    craft_grid.set_slot(0, ItemStack{stone_id, 3});

    const bool moved = inventory_shift_click(craft_grid, 0, registry, main_inventory, 0, 36);

    EXPECT_TRUE(moved);
    EXPECT_TRUE(craft_grid.slot_at(0).is_empty());
    EXPECT_EQ(main_inventory.slot_at(0).item, stone_id);
    EXPECT_EQ(main_inventory.slot_at(0).count, 3u);
}
