#include "lcu/ui/menu_renderer.h"

#include <string>

#include "lcu/math/vec4.h"
#include "lcu/rendering/renderer.h"

namespace lcu::ui {

namespace {
constexpr u32 kCharWidthPx = 8;
constexpr u32 kCharHeightPx = 16;
constexpr u8 kColorSelectedRow = 0x0e;  // yellow on black - matches debug_overlay's own highlight color.
constexpr u8 kColorNormalRow = 0x0f;    // white on black.

constexpr math::Vec4 kBackdropColor{0.0f, 0.0f, 0.0f, 0.55f};
constexpr math::Vec4 kSelectedRowColor{0.3f, 0.3f, 0.3f, 0.6f};
}  // namespace

void queue_menu_backdrop(rendering::Renderer& renderer, const MenuStack& stack, u32 screen_width,
                          u32 screen_height) {
    if (stack.empty()) {
        return;
    }
    const MenuScreen& screen = stack.top();

    renderer.submit_ui_quad(0.0f, 0.0f, static_cast<f32>(screen_width), static_cast<f32>(screen_height),
                             kBackdropColor);

    const std::vector<MenuItemRect> rects = menu_item_layout(screen, screen_width, screen_height);
    if (screen.selected_index < rects.size()) {
        const MenuItemRect& selected_rect = rects[screen.selected_index];
        renderer.submit_ui_quad(selected_rect.x, selected_rect.y, selected_rect.width, selected_rect.height,
                                 kSelectedRowColor);
    }
}

void draw_menu_labels(rendering::Renderer& renderer, const MenuStack& stack, u32 screen_width, u32 screen_height) {
    if (stack.empty()) {
        return;
    }
    const MenuScreen& screen = stack.top();
    const std::vector<MenuItemRect> rects = menu_item_layout(screen, screen_width, screen_height);

    renderer.clear_debug_text();

    if (!rects.empty()) {
        const auto title_cell_x = static_cast<u16>(rects[0].x / static_cast<f32>(kCharWidthPx));
        const auto title_cell_y = static_cast<u16>((rects[0].y - 2.0f * static_cast<f32>(kCharHeightPx)) /
                                                     static_cast<f32>(kCharHeightPx));
        renderer.draw_debug_text(title_cell_x, title_cell_y, kColorNormalRow, screen.title);
    }

    for (usize i = 0; i < screen.items.size() && i < rects.size(); ++i) {
        const MenuItem& item = screen.items[i];
        const MenuItemRect& rect = rects[i];
        const auto cell_x = static_cast<u16>(rect.x / static_cast<f32>(kCharWidthPx));
        const auto cell_y = static_cast<u16>(rect.y / static_cast<f32>(kCharHeightPx));
        const u8 color = (i == screen.selected_index) ? kColorSelectedRow : kColorNormalRow;

        std::string line = item.label;
        if (!item.value_text.empty()) {
            line += ": " + item.value_text;
        }
        renderer.draw_debug_text(cell_x, cell_y, color, line);
    }
}

}  // namespace lcu::ui
