#include "lcu/ui/crafting_table_screen.h"

#include <gtest/gtest.h>

using lcu::ui::CraftingTableScreenLayout;
using lcu::ui::CraftingTableScreenRegion;
using lcu::ui::crafting_table_screen_layout;
using lcu::ui::hit_test_crafting_table_screen;
using lcu::ui::kCraftingTableGridSlotCount;
using lcu::ui::kHotbarSlotCount;
using lcu::ui::kInventoryMainSlotCount;

namespace {
constexpr lcu::u32 kScreenWidth = 800;
constexpr lcu::u32 kScreenHeight = 600;
}  // namespace

TEST(CraftingTableScreenLayoutTest, ProducesReal3x3GridAboveMainGridAndHotbar) {
    const CraftingTableScreenLayout layout = crafting_table_screen_layout(kScreenWidth, kScreenHeight);

    // 3x3 grid: row 1 starts directly below row 0, 3 columns wide.
    EXPECT_FLOAT_EQ(layout.grid_input[0].x, layout.grid_input[3].x);
    EXPECT_LT(layout.grid_input[0].y, layout.grid_input[3].y);
    EXPECT_LT(layout.grid_input[0].x, layout.grid_input[1].x);
    EXPECT_LT(layout.grid_input[1].x, layout.grid_input[2].x);

    // Result slot sits to the right of the grid with a real gap.
    EXPECT_GT(layout.result.x, layout.grid_input[2].x + layout.grid_input[2].size);

    // Main grid sits below the crafting grid.
    EXPECT_GT(layout.main_slots[0].y, layout.grid_input[6].y);

    // Hotbar sits below the main grid, sharing its horizontal span.
    EXPECT_GT(layout.hotbar_slots[0].y, layout.main_slots[kInventoryMainSlotCount - 1].y);
    EXPECT_FLOAT_EQ(layout.hotbar_slots[0].x, layout.main_slots[0].x);
}

TEST(CraftingTableScreenLayoutTest, ScalesWithScreenSize) {
    const CraftingTableScreenLayout small = crafting_table_screen_layout(800, 600);
    const CraftingTableScreenLayout large = crafting_table_screen_layout(1600, 1200);

    EXPECT_NE(small.main_slots[0].x, large.main_slots[0].x);
    EXPECT_NE(small.main_slots[0].y, large.main_slots[0].y);
}

TEST(HitTestCraftingTableScreen, FindsGridInputSlot) {
    const CraftingTableScreenLayout layout = crafting_table_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.grid_input[4];

    const auto hit = hit_test_crafting_table_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, CraftingTableScreenRegion::kGridInput);
    EXPECT_EQ(hit.index, 4u);
}

TEST(HitTestCraftingTableScreen, FindsResultSlot) {
    const CraftingTableScreenLayout layout = crafting_table_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.result;

    const auto hit = hit_test_crafting_table_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, CraftingTableScreenRegion::kResult);
}

TEST(HitTestCraftingTableScreen, FindsMainInventorySlot) {
    const CraftingTableScreenLayout layout = crafting_table_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.main_slots[10];

    const auto hit = hit_test_crafting_table_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, CraftingTableScreenRegion::kMainInventory);
    EXPECT_EQ(hit.index, 10u);
}

TEST(HitTestCraftingTableScreen, FindsHotbarSlot) {
    const CraftingTableScreenLayout layout = crafting_table_screen_layout(kScreenWidth, kScreenHeight);
    const auto& rect = layout.hotbar_slots[2];

    const auto hit = hit_test_crafting_table_screen(layout, rect.x + 1.0f, rect.y + 1.0f);

    EXPECT_EQ(hit.region, CraftingTableScreenRegion::kHotbar);
    EXPECT_EQ(hit.index, 2u);
}

TEST(HitTestCraftingTableScreen, ReturnsNoneOutsideAnySlot) {
    const CraftingTableScreenLayout layout = crafting_table_screen_layout(kScreenWidth, kScreenHeight);

    const auto hit = hit_test_crafting_table_screen(layout, 0.0f, 0.0f);

    EXPECT_EQ(hit.region, CraftingTableScreenRegion::kNone);
}

TEST(CraftingTableScreenConstants, RealSlotCountMatchesA3x3Grid) {
    EXPECT_EQ(kCraftingTableGridSlotCount, 9u);
}
