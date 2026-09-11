#include "lcu/ui/inventory_screen.h"

#include <gtest/gtest.h>

using lcu::ui::hit_test_inventory_screen;
using lcu::ui::InventoryScreenLayout;
using lcu::ui::InventoryScreenRegion;
using lcu::ui::inventory_screen_layout;
using lcu::ui::kCraftGridSlotCount;
using lcu::ui::kHotbarSlotCount;
using lcu::ui::kInventoryMainSlotCount;

namespace {
constexpr lcu::u32 kScreenWidth = 800;
constexpr lcu::u32 kScreenHeight = 600;
}  // namespace

TEST(InventoryScreenLayoutTest, ProducesRealNonOverlappingRowsAndColumns) {
    const InventoryScreenLayout layout = inventory_screen_layout(kScreenWidth, kScreenHeight);

    // Craft grid: 2x2, row-major - row 1 sits strictly below row 0, same x
    // columns repeat.
    EXPECT_FLOAT_EQ(layout.craft_input[0].x, layout.craft_input[2].x);
    EXPECT_LT(layout.craft_input[0].y, layout.craft_input[2].y);
    EXPECT_LT(layout.craft_input[0].x, layout.craft_input[1].x);

    // Result slot sits to the right of the craft grid with a real gap.
    EXPECT_GT(layout.craft_result.x, layout.craft_input[1].x + layout.craft_input[1].size);

    // Main grid: 27 slots, 9 per row - row 1 starts directly below row 0.
    EXPECT_FLOAT_EQ(layout.main_slots[0].x, layout.main_slots[9].x);
    EXPECT_LT(layout.main_slots[0].y, layout.main_slots[9].y);
    EXPECT_LT(layout.main_slots[0].x, layout.main_slots[1].x);

    // Hotbar sits below the main grid.
    EXPECT_GT(layout.hotbar_slots[0].y, layout.main_slots[kInventoryMainSlotCount - 1].y);

    // Hotbar shares the main grid's horizontal span (same slot count/size).
    EXPECT_FLOAT_EQ(layout.hotbar_slots[0].x, layout.main_slots[0].x);
}

TEST(InventoryScreenLayoutTest, ScalesWithScreenSize) {
    const InventoryScreenLayout small = inventory_screen_layout(800, 600);
    const InventoryScreenLayout large = inventory_screen_layout(1600, 1200);

    // A bigger screen centers the same fixed-size panel further from the
    // origin - real recentering, not a static/hardcoded layout.
    EXPECT_NE(small.main_slots[0].x, large.main_slots[0].x);
    EXPECT_NE(small.main_slots[0].y, large.main_slots[0].y);
}

TEST(HitTestInventoryScreen, FindsCraftInputSlot) {
    const InventoryScreenLayout layout = inventory_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.craft_input[1];

    const auto hit = hit_test_inventory_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, InventoryScreenRegion::kCraftInput);
    EXPECT_EQ(hit.index, 1u);
}

TEST(HitTestInventoryScreen, FindsCraftResultSlot) {
    const InventoryScreenLayout layout = inventory_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.craft_result;

    const auto hit = hit_test_inventory_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, InventoryScreenRegion::kCraftResult);
}

TEST(HitTestInventoryScreen, FindsMainInventorySlot) {
    const InventoryScreenLayout layout = inventory_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.main_slots[15];

    const auto hit = hit_test_inventory_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, InventoryScreenRegion::kMainInventory);
    EXPECT_EQ(hit.index, 15u);
}

TEST(HitTestInventoryScreen, FindsHotbarSlot) {
    const InventoryScreenLayout layout = inventory_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.hotbar_slots[4];

    const auto hit = hit_test_inventory_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, InventoryScreenRegion::kHotbar);
    EXPECT_EQ(hit.index, 4u);
}

TEST(HitTestInventoryScreen, ReturnsNoneOutsideAnySlot) {
    const InventoryScreenLayout layout = inventory_screen_layout(kScreenWidth, kScreenHeight);

    const auto hit = hit_test_inventory_screen(layout, 0.0f, 0.0f);

    EXPECT_EQ(hit.region, InventoryScreenRegion::kNone);
}

TEST(InventoryScreenConstants, RealSlotCountsMatchMinecraftLayout) {
    EXPECT_EQ(kCraftGridSlotCount, 4u);
    EXPECT_EQ(kInventoryMainSlotCount, 27u);
    EXPECT_EQ(kHotbarSlotCount, 9u);
}
