#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "lcu/core/types.h"

namespace lcu::ui {

// One row in a MenuScreen (Phase 46's pause/options/controls menus).
// `value_text` is an optional right-hand-side display (a bound key's
// name, a numeric setting, "AN"/"AUS") - empty for a plain navigation
// row like "Zurueck". `on_activate` fires on Enter/click (navigation,
// toggles, "waiting for input" capture); `on_adjust` fires on Left(-1)/
// Right(+1) arrow for a row with a real adjustable value (mouse
// sensitivity, FOV) - a row that only makes sense as one or the other
// simply leaves the unused callback unset, which MenuStack treats as a
// real no-op, not a crash (see activate_selected/adjust_selected).
struct MenuItem {
    std::string label;
    std::string value_text;
    std::function<void()> on_activate;
    std::function<void(i32 direction)> on_adjust;
};

// One stacked screen (Phase 46: pause -> options/controls, each its own
// MenuScreen pushed on top of the pause screen). Owns its own
// `selected_index` so popping back to a parent screen naturally
// restores whatever row was selected there, with zero extra bookkeeping
// in MenuStack itself.
struct MenuScreen {
    std::string title;
    std::vector<MenuItem> items;
    usize selected_index = 0;
};

// Real stacked-screen menu navigation (Phase 46, brief section 60's UI
// framework's pause/options/controls menus) - pure logic, no rendering
// and no SDL dependency at all, so it builds and is unit-tested in both
// the bgfx and non-bgfx configs (see engine/ui/CMakeLists.txt), the
// same separation TouchInputBackend's pure logic vs. its drawn
// kTouchButtonLayout rects already established. A screen's on-screen
// pixel layout and mouse hit-testing are a drawing-time concern kept
// separate - see menu_item_layout()/menu_item_at_point() below and
// menu_renderer.h's real bgfx drawing on top of them.
class MenuStack {
   public:
    void push(MenuScreen screen) { screens_.push_back(std::move(screen)); }

    void pop() {
        if (!screens_.empty()) {
            screens_.pop_back();
        }
    }

    void clear() { screens_.clear(); }

    bool empty() const { return screens_.empty(); }

    MenuScreen& top() { return screens_.back(); }
    const MenuScreen& top() const { return screens_.back(); }

    // Moves the current screen's selection by `delta` rows, wrapping
    // around both ends (so pressing Up on the first row lands on the
    // last, matching Minecraft's own menu wraparound) rather than
    // clamping and getting visibly stuck at an edge.
    void move_selection(i32 delta) {
        if (screens_.empty() || screens_.back().items.empty()) {
            return;
        }
        MenuScreen& screen = screens_.back();
        const i32 count = static_cast<i32>(screen.items.size());
        i32 next = (static_cast<i32>(screen.selected_index) + delta) % count;
        if (next < 0) {
            next += count;
        }
        screen.selected_index = static_cast<usize>(next);
    }

    // Used by real mouse hover/click hit-testing (client/main.cpp) to
    // set the selection directly to whichever row the cursor is over,
    // rather than only ever moving it relatively. A no-op for an
    // out-of-range index (an empty screen, or a stale index from before
    // a rebuild shrank the item list).
    void select_index(usize index) {
        if (screens_.empty() || index >= screens_.back().items.size()) {
            return;
        }
        screens_.back().selected_index = index;
    }

    void activate_selected() {
        if (screens_.empty()) {
            return;
        }
        MenuScreen& screen = screens_.back();
        if (screen.selected_index < screen.items.size() && screen.items[screen.selected_index].on_activate) {
            screen.items[screen.selected_index].on_activate();
        }
    }

    void adjust_selected(i32 direction) {
        if (screens_.empty()) {
            return;
        }
        MenuScreen& screen = screens_.back();
        if (screen.selected_index < screen.items.size() && screen.items[screen.selected_index].on_adjust) {
            screen.items[screen.selected_index].on_adjust(direction);
        }
    }

   private:
    std::vector<MenuScreen> screens_;
};

// One row's real on-screen pixel rectangle (top-left origin, y down -
// the same SDL/mouse convention submit_ui_quad already uses), computed
// purely from the screen's item count and the viewport size - no
// rendering needed to compute this, so it's the one real shared source
// of truth both menu_renderer.h's drawing and mouse hit-testing below
// read from, the same "drawn and tappable can never drift apart"
// property kTouchButtonLayout already established for touch controls.
struct MenuItemRect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 width = 0.0f;
    f32 height = 0.0f;
};

std::vector<MenuItemRect> menu_item_layout(const MenuScreen& screen, u32 screen_width, u32 screen_height);

// Returns the index of whichever row's rect (see menu_item_layout)
// contains (x, y), or std::nullopt if the point is outside every row -
// a real click outside the menu's own rows (e.g. the dimmed background)
// does nothing, rather than always hitting the nearest row.
std::optional<usize> menu_item_at_point(const MenuScreen& screen, u32 screen_width, u32 screen_height, f32 x, f32 y);

}  // namespace lcu::ui
