#include "lcu/ui/crafting_table_screen_renderer.h"

#include <string>

#include "lcu/math/vec4.h"
#include "lcu/rendering/renderer.h"
#include "lcu/ui/text_renderer.h"

namespace lcu::ui {

namespace {
constexpr u32 kCharWidthPx = 8;
constexpr u32 kCharHeightPx = 16;
constexpr u8 kColorCount = 0x0f;  // white on black.

constexpr math::Vec4 kBackdropColor{0.0f, 0.0f, 0.0f, 0.55f};
constexpr math::Vec4 kSlotBorderColor{0.75f, 0.75f, 0.75f, 0.9f};
constexpr math::Vec4 kSlotFillColor{0.15f, 0.15f, 0.15f, 0.65f};
constexpr math::Vec4 kResultBorderColor{0.9f, 0.85f, 0.3f, 0.9f};
constexpr f32 kSlotBorderThickness = 2.0f;
constexpr f32 kIconInsetRatio = 0.2f;

// Real text-renderer color (Phase 57) matching kColorCount's legacy VGA
// white-on-black attribute.
constexpr math::Vec4 kTextWhite{1.0f, 1.0f, 1.0f, 1.0f};

void queue_slot(rendering::Renderer& renderer, const InventorySlotRect& rect, const InventorySlotDisplay& display,
                 bool highlight_border) {
    renderer.submit_ui_quad(rect.x - kSlotBorderThickness, rect.y - kSlotBorderThickness,
                             rect.size + kSlotBorderThickness * 2.0f, rect.size + kSlotBorderThickness * 2.0f,
                             highlight_border ? kResultBorderColor : kSlotBorderColor);
    renderer.submit_ui_quad(rect.x, rect.y, rect.size, rect.size, kSlotFillColor);
    if (display.has_item) {
        const f32 icon_size = rect.size * (1.0f - kIconInsetRatio);
        const f32 inset = (rect.size - icon_size) * 0.5f;
        if (display.texture_uv.has_value()) {
            const math::Vec4& uv = *display.texture_uv;
            renderer.submit_textured_ui_quad(rect.x + inset, rect.y + inset, icon_size, icon_size, display.icon_color,
                                              uv.x, uv.y, uv.z, uv.w);
        } else {
            renderer.submit_ui_quad(rect.x + inset, rect.y + inset, icon_size, icon_size, display.icon_color);
        }
    }
}

void draw_slot_label(rendering::Renderer& renderer, const InventorySlotRect& rect, const InventorySlotDisplay& display,
                      bool legacy_debug_text) {
    if (!display.has_item || display.count <= 1) {
        return;
    }
    const std::string text = std::to_string(display.count);
    if (legacy_debug_text) {
        const auto cell_x =
            static_cast<u16>((rect.x + rect.size - static_cast<f32>(kCharWidthPx)) / static_cast<f32>(kCharWidthPx));
        const auto cell_y = static_cast<u16>((rect.y + rect.size - static_cast<f32>(kCharHeightPx) * 0.5f) /
                                              static_cast<f32>(kCharHeightPx));
        renderer.draw_debug_text(cell_x, cell_y, kColorCount, text);
    } else {
        const f32 text_width = TextRenderer::measure_text_width(text);
        const f32 px = rect.x + rect.size - text_width;
        const f32 py = rect.y + rect.size - kGlyphCellHeight;
        TextRenderer::draw_text(renderer, text, px, py, kTextWhite);
    }
}

}  // namespace

void queue_crafting_table_screen_quads(rendering::Renderer& renderer, const CraftingTableScreenLayout& layout,
                                        const CraftingTableScreenState& state, u32 screen_width, u32 screen_height) {
    renderer.submit_ui_quad(0.0f, 0.0f, static_cast<f32>(screen_width), static_cast<f32>(screen_height),
                             kBackdropColor);

    for (usize i = 0; i < kCraftingTableGridSlotCount; ++i) {
        queue_slot(renderer, layout.grid_input[i], state.grid_input[i], false);
    }
    queue_slot(renderer, layout.result, state.result, state.result.has_item);
    for (usize i = 0; i < kInventoryMainSlotCount; ++i) {
        queue_slot(renderer, layout.main_slots[i], state.main_slots[i], false);
    }
    for (usize i = 0; i < kHotbarSlotCount; ++i) {
        queue_slot(renderer, layout.hotbar_slots[i], state.hotbar_slots[i], false);
    }

    if (state.cursor.has_item) {
        constexpr f32 kCursorIconSize = kInventorySlotSize * (1.0f - kIconInsetRatio);
        if (state.cursor.texture_uv.has_value()) {
            const math::Vec4& uv = *state.cursor.texture_uv;
            renderer.submit_textured_ui_quad(state.cursor_x - kCursorIconSize * 0.5f,
                                              state.cursor_y - kCursorIconSize * 0.5f, kCursorIconSize,
                                              kCursorIconSize, state.cursor.icon_color, uv.x, uv.y, uv.z, uv.w);
        } else {
            renderer.submit_ui_quad(state.cursor_x - kCursorIconSize * 0.5f, state.cursor_y - kCursorIconSize * 0.5f,
                                     kCursorIconSize, kCursorIconSize, state.cursor.icon_color);
        }
    }
}

void draw_crafting_table_screen_labels(rendering::Renderer& renderer, const CraftingTableScreenLayout& layout,
                                        const CraftingTableScreenState& state, bool legacy_debug_text) {
    for (usize i = 0; i < kCraftingTableGridSlotCount; ++i) {
        draw_slot_label(renderer, layout.grid_input[i], state.grid_input[i], legacy_debug_text);
    }
    draw_slot_label(renderer, layout.result, state.result, legacy_debug_text);
    for (usize i = 0; i < kInventoryMainSlotCount; ++i) {
        draw_slot_label(renderer, layout.main_slots[i], state.main_slots[i], legacy_debug_text);
    }
    for (usize i = 0; i < kHotbarSlotCount; ++i) {
        draw_slot_label(renderer, layout.hotbar_slots[i], state.hotbar_slots[i], legacy_debug_text);
    }

    if (state.cursor.has_item && state.cursor.count > 1) {
        const std::string text = std::to_string(state.cursor.count);
        if (legacy_debug_text) {
            const auto cell_x = static_cast<u16>(state.cursor_x / static_cast<f32>(kCharWidthPx));
            const auto cell_y = static_cast<u16>(state.cursor_y / static_cast<f32>(kCharHeightPx));
            renderer.draw_debug_text(cell_x, cell_y, kColorCount, text);
        } else {
            TextRenderer::draw_text(renderer, text, state.cursor_x, state.cursor_y, kTextWhite);
        }
    }
}

}  // namespace lcu::ui
