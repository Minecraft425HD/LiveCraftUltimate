#include "lcu/items/inventory.h"

#include <gtest/gtest.h>

using lcu::items::Inventory;
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

TEST(Inventory, DefaultSlotsAreEmpty) {
    Inventory inventory(9);
    EXPECT_EQ(inventory.slot_count(), 9u);
    for (lcu::usize i = 0; i < inventory.slot_count(); ++i) {
        EXPECT_TRUE(inventory.slot_at(i).is_empty());
        EXPECT_EQ(inventory.slot_at(i).item, kNoItemId);
    }
}

TEST(Inventory, SetSlotAndSlotAtRoundTrip) {
    Inventory inventory(4);
    inventory.set_slot(2, ItemStack{5, 10});

    EXPECT_EQ(inventory.slot_at(2).item, 5);
    EXPECT_EQ(inventory.slot_at(2).count, 10u);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
}

TEST(Inventory, AddItemFillsEmptySlotsUpToMaxStack) {
    lcu::items::ItemId stone_id = 0;
    lcu::items::ItemId wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);

    Inventory inventory(4);
    const lcu::u32 leftover = inventory.add_item(registry, ItemStack{stone_id, 100});

    // 64 fit in the first slot, 36 spill into the second.
    EXPECT_EQ(leftover, 0u);
    EXPECT_EQ(inventory.slot_at(0).item, stone_id);
    EXPECT_EQ(inventory.slot_at(0).count, 64u);
    EXPECT_EQ(inventory.slot_at(1).item, stone_id);
    EXPECT_EQ(inventory.slot_at(1).count, 36u);
    EXPECT_TRUE(inventory.slot_at(2).is_empty());
}

TEST(Inventory, AddItemTopsUpExistingPartialStackBeforeUsingNewSlot) {
    lcu::items::ItemId stone_id = 0;
    lcu::items::ItemId wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);

    Inventory inventory(4);
    inventory.set_slot(0, ItemStack{stone_id, 60});

    const lcu::u32 leftover = inventory.add_item(registry, ItemStack{stone_id, 10});

    EXPECT_EQ(leftover, 0u);
    EXPECT_EQ(inventory.slot_at(0).count, 64u);  // topped up first
    EXPECT_EQ(inventory.slot_at(1).item, stone_id);
    EXPECT_EQ(inventory.slot_at(1).count, 6u);  // the remaining 6 spill into a new slot
}

TEST(Inventory, AddItemReturnsLeftoverWhenInventoryFull) {
    lcu::items::ItemId stone_id = 0;
    lcu::items::ItemId wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);

    Inventory inventory(2);
    const lcu::u32 leftover = inventory.add_item(registry, ItemStack{stone_id, 200});

    EXPECT_EQ(leftover, 200u - 128u);
    EXPECT_EQ(inventory.slot_at(0).count, 64u);
    EXPECT_EQ(inventory.slot_at(1).count, 64u);
}

TEST(Inventory, AddItemRespectsPerItemMaxStackSize) {
    lcu::items::ItemId stone_id = 0;
    lcu::items::ItemId wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);

    Inventory inventory(3);
    const lcu::u32 leftover = inventory.add_item(registry, ItemStack{wand_id, 2});

    // wand's max_stack_size is 1 - each unit needs its own slot.
    EXPECT_EQ(leftover, 0u);
    EXPECT_EQ(inventory.slot_at(0).count, 1u);
    EXPECT_EQ(inventory.slot_at(1).count, 1u);
}

TEST(Inventory, AddItemNoOpForNoItemOrZeroCount) {
    lcu::items::ItemId stone_id = 0;
    lcu::items::ItemId wand_id = 0;
    const ItemRegistry registry = make_registry_with_stone_and_wand(stone_id, wand_id);

    Inventory inventory(2);
    EXPECT_EQ(inventory.add_item(registry, ItemStack{kNoItemId, 5}), 5u);
    EXPECT_EQ(inventory.add_item(registry, ItemStack{stone_id, 0}), 0u);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
}

TEST(Inventory, RemoveItemTakesFromEarliestSlotsFirstAndEmptiesSlot) {
    Inventory inventory(3);
    inventory.set_slot(0, ItemStack{7, 5});
    inventory.set_slot(1, ItemStack{7, 5});

    const lcu::u32 removed = inventory.remove_item(7, 8);

    EXPECT_EQ(removed, 8u);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());  // fully drained, reset to kNoItemId
    EXPECT_EQ(inventory.slot_at(0).item, kNoItemId);
    EXPECT_EQ(inventory.slot_at(1).count, 2u);
    EXPECT_EQ(inventory.slot_at(1).item, 7);
}

TEST(Inventory, RemoveItemReturnsActualAmountRemovedWhenInsufficient) {
    Inventory inventory(2);
    inventory.set_slot(0, ItemStack{7, 3});

    const lcu::u32 removed = inventory.remove_item(7, 10);

    EXPECT_EQ(removed, 3u);
    EXPECT_TRUE(inventory.slot_at(0).is_empty());
}

TEST(Inventory, CountItemSumsAcrossSlots) {
    Inventory inventory(3);
    inventory.set_slot(0, ItemStack{7, 3});
    inventory.set_slot(1, ItemStack{9, 4});
    inventory.set_slot(2, ItemStack{7, 6});

    EXPECT_EQ(inventory.count_item(7), 9u);
    EXPECT_EQ(inventory.count_item(9), 4u);
    EXPECT_EQ(inventory.count_item(42), 0u);
}
