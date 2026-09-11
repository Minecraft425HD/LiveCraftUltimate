#include "lcu/ui/menu_stack.h"

namespace lcu::ui {

namespace {
// bgfx's built-in debug-text font (Phase 46's own label rendering, see
// menu_renderer.cpp) is an 8x16-pixel character cell - the same
// constants debug_overlay.cpp already hardcodes. Each menu row is
// exactly one text row tall, real pixels, not a made-up spacing value.
constexpr f32 kRowHeight = 16.0f;
constexpr f32 kRowWidth = 400.0f;
constexpr f32 kTitleRows = 2.0f;  // one row for the title, one blank row beneath it.
}  // namespace

std::vector<MenuItemRect> menu_item_layout(const MenuScreen& screen, u32 screen_width, u32 screen_height) {
    std::vector<MenuItemRect> rects;
    rects.reserve(screen.items.size());

    const f32 total_height = (kTitleRows + static_cast<f32>(screen.items.size())) * kRowHeight;
    const f32 start_x = (static_cast<f32>(screen_width) - kRowWidth) * 0.5f;
    const f32 start_y = (static_cast<f32>(screen_height) - total_height) * 0.5f + kTitleRows * kRowHeight;

    for (usize i = 0; i < screen.items.size(); ++i) {
        rects.push_back({start_x, start_y + static_cast<f32>(i) * kRowHeight, kRowWidth, kRowHeight});
    }
    return rects;
}

std::optional<usize> menu_item_at_point(const MenuScreen& screen, u32 screen_width, u32 screen_height, f32 x,
                                         f32 y) {
    const std::vector<MenuItemRect> rects = menu_item_layout(screen, screen_width, screen_height);
    for (usize i = 0; i < rects.size(); ++i) {
        const MenuItemRect& rect = rects[i];
        if (x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height) {
            return i;
        }
    }
    return std::nullopt;
}

}  // namespace lcu::ui
