#include "lcu/ui/hud.h"

#include <gtest/gtest.h>

using namespace lcu::ui;

TEST(HotbarSlotLayout, ProducesNineSlotsLeftToRight) {
    const auto slots = hotbar_slot_layout(1280, 720);
    ASSERT_EQ(slots.size(), kHotbarSlotCount);
    for (lcu::usize i = 1; i < slots.size(); ++i) {
        EXPECT_GT(slots[i].x, slots[i - 1].x) << "slots must be laid out left-to-right in order";
        EXPECT_EQ(slots[i].y, slots[i - 1].y) << "every slot shares the same real row";
    }
}

TEST(HotbarSlotLayout, IsHorizontallyCenteredOnScreen) {
    const auto slots = hotbar_slot_layout(1280, 720);
    const lcu::f32 left_edge = slots.front().x;
    const lcu::f32 right_edge = slots.back().x + slots.back().size;
    const lcu::f32 center = (left_edge + right_edge) * 0.5f;
    EXPECT_NEAR(center, 640.0f, 0.5f);
}

TEST(HotbarSlotLayout, SitsNearTheBottomOfTheScreen) {
    const auto slots = hotbar_slot_layout(1280, 720);
    EXPECT_GT(slots.front().y, 700.0f - kHotbarSlotSize - kHotbarBottomMargin - 1.0f);
    EXPECT_LT(slots.front().y + kHotbarSlotSize, 720.0f);
}

TEST(HotbarSlotLayout, RectsAreNonOverlapping) {
    const auto slots = hotbar_slot_layout(1280, 720);
    for (lcu::usize i = 1; i < slots.size(); ++i) {
        EXPECT_GE(slots[i].x, slots[i - 1].x + slots[i - 1].size) << "consecutive slots must not overlap";
    }
}

TEST(StatBarLayout, ProducesTenIconsLeftToRight) {
    const auto icons = stat_bar_layout(1280, 700.0f, 20.0f, 20.0f);
    ASSERT_EQ(icons.size(), kStatBarIconCount);
    for (lcu::usize i = 1; i < icons.size(); ++i) {
        EXPECT_GT(icons[i].x, icons[i - 1].x);
        EXPECT_EQ(icons[i].y, icons[i - 1].y);
    }
}

TEST(StatBarLayout, SitsDirectlyAboveTheGivenBottomY) {
    const auto icons = stat_bar_layout(1280, 700.0f, 20.0f, 20.0f);
    EXPECT_FLOAT_EQ(icons.front().y + kStatBarIconSize, 700.0f);
}

TEST(StatBarLayout, FullValueFillsEveryIconCompletely) {
    const auto icons = stat_bar_layout(1280, 700.0f, 20.0f, 20.0f);
    for (const auto& icon : icons) {
        EXPECT_FLOAT_EQ(icon.fill_fraction, 1.0f);
    }
}

TEST(StatBarLayout, ZeroValueLeavesEveryIconEmpty) {
    const auto icons = stat_bar_layout(1280, 700.0f, 0.0f, 20.0f);
    for (const auto& icon : icons) {
        EXPECT_FLOAT_EQ(icon.fill_fraction, 0.0f);
    }
}

TEST(StatBarLayout, OddValueProducesARealHalfIcon) {
    // 20/1 point health left over 10 full-value icons (2 points each):
    // 9 full hearts (18 points) + one real half heart (1 point).
    const auto icons = stat_bar_layout(1280, 700.0f, 19.0f, 20.0f);
    for (lcu::usize i = 0; i < 9; ++i) {
        EXPECT_FLOAT_EQ(icons[i].fill_fraction, 1.0f) << "icon " << i;
    }
    EXPECT_FLOAT_EQ(icons[9].fill_fraction, 0.5f);
}

TEST(StatBarLayout, ZeroMaxValueLeavesEveryIconEmptyWithoutDividingByZero) {
    const auto icons = stat_bar_layout(1280, 700.0f, 0.0f, 0.0f);
    for (const auto& icon : icons) {
        EXPECT_FLOAT_EQ(icon.fill_fraction, 0.0f);
    }
}

TEST(StatBarLayout, AlignsWithTheHotbarsOwnLeftEdge) {
    const auto slots = hotbar_slot_layout(1280, 720);
    const auto icons = stat_bar_layout(1280, slots.front().y, 20.0f, 20.0f);
    EXPECT_FLOAT_EQ(slots.front().x, icons.front().x);
}
