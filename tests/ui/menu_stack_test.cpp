#include "lcu/ui/menu_stack.h"

#include <gtest/gtest.h>

using lcu::ui::MenuItem;
using lcu::ui::MenuScreen;
using lcu::ui::MenuStack;

namespace {

MenuScreen make_screen(int item_count) {
    MenuScreen screen;
    screen.title = "Test";
    for (int i = 0; i < item_count; ++i) {
        screen.items.push_back({"item" + std::to_string(i), "", nullptr, nullptr});
    }
    return screen;
}

}  // namespace

TEST(MenuStack, StartsEmpty) {
    MenuStack stack;
    EXPECT_TRUE(stack.empty());
}

TEST(MenuStack, PushMakesItNonEmptyAndSetsTop) {
    MenuStack stack;
    stack.push(make_screen(3));
    ASSERT_FALSE(stack.empty());
    EXPECT_EQ(stack.top().title, "Test");
    EXPECT_EQ(stack.top().items.size(), 3u);
}

TEST(MenuStack, PopRestoresThePreviousScreen) {
    MenuStack stack;
    MenuScreen first = make_screen(2);
    first.title = "First";
    MenuScreen second = make_screen(2);
    second.title = "Second";
    stack.push(first);
    stack.push(second);

    EXPECT_EQ(stack.top().title, "Second");
    stack.pop();
    ASSERT_FALSE(stack.empty());
    EXPECT_EQ(stack.top().title, "First");
    stack.pop();
    EXPECT_TRUE(stack.empty());
}

TEST(MenuStack, PopOnEmptyStackIsSafe) {
    MenuStack stack;
    stack.pop();  // must not crash.
    EXPECT_TRUE(stack.empty());
}

TEST(MenuStack, ClearRemovesEveryScreen) {
    MenuStack stack;
    stack.push(make_screen(1));
    stack.push(make_screen(1));
    stack.clear();
    EXPECT_TRUE(stack.empty());
}

TEST(MenuStack, MoveSelectionWrapsAtBothEnds) {
    MenuStack stack;
    stack.push(make_screen(3));

    EXPECT_EQ(stack.top().selected_index, 0u);
    stack.move_selection(-1);
    EXPECT_EQ(stack.top().selected_index, 2u) << "moving up from the first row should wrap to the last";
    stack.move_selection(1);
    EXPECT_EQ(stack.top().selected_index, 0u);
    stack.move_selection(1);
    stack.move_selection(1);
    stack.move_selection(1);
    EXPECT_EQ(stack.top().selected_index, 0u) << "3 -> 0,1,2,0 (3 items, wraps once)";
}

TEST(MenuStack, MoveSelectionOnEmptyScreenIsSafe) {
    MenuStack stack;
    stack.push(make_screen(0));
    stack.move_selection(1);  // must not crash.
}

TEST(MenuStack, PoppingRestoresThePreviousScreensOwnSelection) {
    MenuStack stack;
    stack.push(make_screen(3));
    stack.move_selection(2);
    ASSERT_EQ(stack.top().selected_index, 2u);

    stack.push(make_screen(3));
    stack.move_selection(1);
    ASSERT_EQ(stack.top().selected_index, 1u);

    stack.pop();
    EXPECT_EQ(stack.top().selected_index, 2u) << "the parent screen's own selection must survive the child screen's";
}

TEST(MenuStack, SelectIndexSetsSelectionDirectly) {
    MenuStack stack;
    stack.push(make_screen(4));
    stack.select_index(3);
    EXPECT_EQ(stack.top().selected_index, 3u);
}

TEST(MenuStack, SelectIndexOutOfRangeIsIgnored) {
    MenuStack stack;
    stack.push(make_screen(3));
    stack.select_index(1);
    stack.select_index(99);
    EXPECT_EQ(stack.top().selected_index, 1u) << "an out-of-range index must not corrupt the real selection";
}

TEST(MenuStack, ActivateSelectedCallsTheRealCallback) {
    MenuStack stack;
    bool activated = false;
    MenuScreen screen;
    screen.items.push_back({"item", "", [&]() { activated = true; }, nullptr});
    stack.push(std::move(screen));

    stack.activate_selected();
    EXPECT_TRUE(activated);
}

TEST(MenuStack, ActivateSelectedWithNoCallbackIsSafe) {
    MenuStack stack;
    stack.push(make_screen(1));  // nullptr on_activate.
    stack.activate_selected();   // must not crash.
}

TEST(MenuStack, ActivateSelectedOnEmptyStackIsSafe) {
    MenuStack stack;
    stack.activate_selected();  // must not crash.
}

TEST(MenuStack, AdjustSelectedCallsTheRealCallbackWithDirection) {
    MenuStack stack;
    lcu::i32 last_direction = 0;
    MenuScreen screen;
    screen.items.push_back({"item", "", nullptr, [&](lcu::i32 dir) { last_direction = dir; }});
    stack.push(std::move(screen));

    stack.adjust_selected(-1);
    EXPECT_EQ(last_direction, -1);
    stack.adjust_selected(1);
    EXPECT_EQ(last_direction, 1);
}

TEST(MenuStack, AdjustSelectedWithNoCallbackIsSafe) {
    MenuStack stack;
    stack.push(make_screen(1));  // nullptr on_adjust.
    stack.adjust_selected(1);    // must not crash.
}

TEST(MenuItemLayout, ProducesOneRectPerItemInOrder) {
    const MenuScreen screen = make_screen(4);
    const auto rects = lcu::ui::menu_item_layout(screen, 1280, 720);
    ASSERT_EQ(rects.size(), 4u);
    for (lcu::usize i = 1; i < rects.size(); ++i) {
        EXPECT_LT(rects[i - 1].y, rects[i].y) << "rows must be laid out top-to-bottom, in item order";
        EXPECT_EQ(rects[i - 1].x, rects[i].x) << "every row shares the same real x/width";
        EXPECT_EQ(rects[i - 1].width, rects[i].width);
    }
}

TEST(MenuItemLayout, RectsAreNonOverlappingAndOnScreen) {
    const MenuScreen screen = make_screen(3);
    const auto rects = lcu::ui::menu_item_layout(screen, 1280, 720);
    ASSERT_EQ(rects.size(), 3u);
    for (lcu::usize i = 1; i < rects.size(); ++i) {
        EXPECT_GE(rects[i].y, rects[i - 1].y + rects[i - 1].height)
            << "consecutive rows must not overlap vertically";
    }
    for (const auto& rect : rects) {
        EXPECT_GE(rect.x, 0.0f);
        EXPECT_LE(rect.x + rect.width, 1280.0f);
    }
}

TEST(MenuItemAtPoint, FindsTheRowContainingThePoint) {
    const MenuScreen screen = make_screen(3);
    const auto rects = lcu::ui::menu_item_layout(screen, 1280, 720);
    ASSERT_EQ(rects.size(), 3u);

    const auto& middle_rect = rects[1];
    const lcu::f32 point_x = middle_rect.x + middle_rect.width * 0.5f;
    const lcu::f32 point_y = middle_rect.y + middle_rect.height * 0.5f;

    const auto hit = lcu::ui::menu_item_at_point(screen, 1280, 720, point_x, point_y);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, 1u);
}

TEST(MenuItemAtPoint, ReturnsNulloptOutsideEveryRow) {
    const MenuScreen screen = make_screen(3);
    const auto hit = lcu::ui::menu_item_at_point(screen, 1280, 720, -100.0f, -100.0f);
    EXPECT_FALSE(hit.has_value());
}
