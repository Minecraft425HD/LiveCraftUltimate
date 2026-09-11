#include "lcu/ui/inventory_screen_renderer.h"

#include <string>

#include "lcu/math/vec4.h"
#include "lcu/rendering/renderer.h"

namespace lcu::ui {

namespace {
constexpr u32 kCharWidthPx = 8;
constexpr u32 kCharHeightPx = 16;
constexpr u8 kColorCount = 0x0f;  // white on black.

constexpr math::Vec4 kBackdropColor{0.0f, 0.0f, 0.0f, 0.55f};
constexpr math::Vec4 kSlotBorderColor{0.75f, 0.75f, 0.75f, 0.9f};
constexpr math::Vec4 kSlotFillColor{0.15f, 0.15f, 0.15f, 0.65f};
constexpr math::Vec4 kCraftResultBorderColor{0.9f, 0.85f, 0.3f, 0.9f};
constexpr f32 kSlotBorderThickness = 2.0f;
constexpr f32 kIconInsetRatio = 0.2f;  // matches hud.h's 16/20 icon-to-slot ratio.

void queue_slot(rendering::Renderer& renderer, const InventorySlotRect& rect, const InventorySlotDisplay& display,
                 bool highlight_border) {
    renderer.submit_ui_quad(rect.x - kSlotBorderThickness, rect.y - kSlotBorderThickness,
                             rect.size + kSlotBorderThickness * 2.0f, rect.size + kSlotBorderThickness * 2.0f,
                             highlight_border ? kCraftResultBorderColor : kSlotBorderColor);
    renderer.submit_ui_quad(rect.x, rect.y, rect.size, rect.size, kSlotFillColor);
    if (display.has_item) {
        const f32 icon_size = rect.size * (1.0f - kIconInsetRatio);
        const f32 inset = (rect.size - icon_size) * 0.5f;
        renderer.submit_ui_quad(rect.x + inset, rect.y + inset, icon_size, icon_size, display.icon_color);
    }
}

void draw_slot_label(rendering::Renderer& renderer, const InventorySlotRect& rect, const InventorySlotDisplay& display) {
    if (!display.has_item || display.count <= 1) {
        return;
    }
    const auto cell_x =
        static_cast<u16>((rect.x + rect.size - static_cast<f32>(kCharWidthPx)) / static_cast<f32>(kCharWidthPx));
    const auto cell_y = static_cast<u16>((rect.y + rect.size - static_cast<f32>(kCharHeightPx) * 0.5f) /
                                          static_cast<f32>(kCharHeightPx));
    renderer.draw_debug_text(cell_x, cell_y, kColorCount, std::to_string(display.count));
}

}  // namespace

void queue_inventory_screen_quads(rendering::Renderer& renderer, const InventoryScreenLayout& layout,
                                   const InventoryScreenState& state, u32 screen_width, u32 screen_height) {
    renderer.submit_ui_quad(0.0f, 0.0f, static_cast<f32>(screen_width), static_cast<f32>(screen_height),
                             kBackdropColor);

    for (usize i = 0; i < kCraftGridSlotCount; ++i) {
        queue_slot(renderer, layout.craft_input[i], state.craft_input[i], false);
    }
    queue_slot(renderer, layout.craft_result, state.craft_result, state.craft_result.has_item);
    for (usize i = 0; i < kInventoryMainSlotCount; ++i) {
        queue_slot(renderer, layout.main_slots[i], state.main_slots[i], false);
    }
    for (usize i = 0; i < kHotbarSlotCount; ++i) {
        queue_slot(renderer, layout.hotbar_slots[i], state.hotbar_slots[i], false);
    }

    if (state.cursor.has_item) {
        // No slot border for the cursor stack - it's a stand-in for the
        // mouse pointer itself, not a fourth "slot", so only the icon
        // quad is drawn, centered on the real mouse position.
        constexpr f32 kCursorIconSize = kInventorySlotSize * (1.0f - kIconInsetRatio);
        renderer.submit_ui_quad(state.cursor_x - kCursorIconSize * 0.5f, state.cursor_y - kCursorIconSize * 0.5f,
                                 kCursorIconSize, kCursorIconSize, state.cursor.icon_color);
    }
}

void draw_inventory_screen_labels(rendering::Renderer& renderer, const InventoryScreenLayout& layout,
                                   const InventoryScreenState& state) {
    for (usize i = 0; i < kCraftGridSlotCount; ++i) {
        draw_slot_label(renderer, layout.craft_input[i], state.craft_input[i]);
    }
    draw_slot_label(renderer, layout.craft_result, state.craft_result);
    for (usize i = 0; i < kInventoryMainSlotCount; ++i) {
        draw_slot_label(renderer, layout.main_slots[i], state.main_slots[i]);
    }
    for (usize i = 0; i < kHotbarSlotCount; ++i) {
        draw_slot_label(renderer, layout.hotbar_slots[i], state.hotbar_slots[i]);
    }

    if (state.cursor.has_item && state.cursor.count > 1) {
        const auto cell_x = static_cast<u16>(state.cursor_x / static_cast<f32>(kCharWidthPx));
        const auto cell_y = static_cast<u16>(state.cursor_y / static_cast<f32>(kCharHeightPx));
        renderer.draw_debug_text(cell_x, cell_y, kColorCount, std::to_string(state.cursor.count));
    }
}

}  // namespace lcu::ui
