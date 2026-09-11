#include "lcu/ui/hud_renderer.h"

#include <string>

#include "lcu/math/vec4.h"
#include "lcu/rendering/renderer.h"

namespace lcu::ui {

namespace {
constexpr u32 kCharWidthPx = 8;
constexpr u32 kCharHeightPx = 16;
constexpr u8 kColorCount = 0x0f;  // white on black.

constexpr math::Vec4 kSlotBorderColor{0.75f, 0.75f, 0.75f, 0.9f};
constexpr math::Vec4 kSlotBorderSelectedColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr math::Vec4 kSlotFillColor{0.15f, 0.15f, 0.15f, 0.65f};
constexpr math::Vec4 kHealthEmptyColor{0.25f, 0.05f, 0.05f, 0.8f};
constexpr math::Vec4 kHealthFullColor{0.85f, 0.1f, 0.1f, 1.0f};
constexpr math::Vec4 kHungerEmptyColor{0.2f, 0.15f, 0.05f, 0.8f};
constexpr math::Vec4 kHungerFullColor{0.65f, 0.45f, 0.15f, 1.0f};

constexpr f32 kSlotBorderThickness = 2.0f;
}  // namespace

void queue_hud_quads(rendering::Renderer& renderer, const HudState& state, u32 screen_width, u32 screen_height) {
    const std::array<HotbarSlotRect, kHotbarSlotCount> slots = hotbar_slot_layout(screen_width, screen_height);

    for (usize i = 0; i < kHotbarSlotCount; ++i) {
        const HotbarSlotRect& slot = slots[i];
        const bool selected = (i == state.selected_hotbar_slot);
        renderer.submit_ui_quad(slot.x - kSlotBorderThickness, slot.y - kSlotBorderThickness,
                                 slot.size + kSlotBorderThickness * 2.0f, slot.size + kSlotBorderThickness * 2.0f,
                                 selected ? kSlotBorderSelectedColor : kSlotBorderColor);
        renderer.submit_ui_quad(slot.x, slot.y, slot.size, slot.size, kSlotFillColor);

        const HotbarItem& item = state.hotbar[i];
        if (item.has_item) {
            const f32 icon_inset = (slot.size - kHotbarIconSize) * 0.5f;
            renderer.submit_ui_quad(slot.x + icon_inset, slot.y + icon_inset, kHotbarIconSize, kHotbarIconSize,
                                     item.icon_color);
        }
    }

    const f32 hotbar_top_y = slots.front().y;

    const std::array<StatBarIconRect, kStatBarIconCount> hunger_icons =
        stat_bar_layout(screen_width, hotbar_top_y - kHotbarSlotGap, state.hunger, state.max_hunger);
    for (const StatBarIconRect& icon : hunger_icons) {
        renderer.submit_ui_quad(icon.x, icon.y, icon.size, icon.size, kHungerEmptyColor);
        if (icon.fill_fraction > 0.0f) {
            renderer.submit_ui_quad(icon.x, icon.y, icon.size * icon.fill_fraction, icon.size, kHungerFullColor);
        }
    }

    const f32 hunger_top_y = hunger_icons.front().y;
    const std::array<StatBarIconRect, kStatBarIconCount> health_icons =
        stat_bar_layout(screen_width, hunger_top_y - kStatBarIconGap, state.health, state.max_health);
    for (const StatBarIconRect& icon : health_icons) {
        renderer.submit_ui_quad(icon.x, icon.y, icon.size, icon.size, kHealthEmptyColor);
        if (icon.fill_fraction > 0.0f) {
            renderer.submit_ui_quad(icon.x, icon.y, icon.size * icon.fill_fraction, icon.size, kHealthFullColor);
        }
    }
}

void draw_hud_labels(rendering::Renderer& renderer, const HudState& state, u32 screen_width, u32 screen_height) {
    const std::array<HotbarSlotRect, kHotbarSlotCount> slots = hotbar_slot_layout(screen_width, screen_height);
    for (usize i = 0; i < kHotbarSlotCount; ++i) {
        const HotbarItem& item = state.hotbar[i];
        if (!item.has_item) {
            continue;
        }
        const HotbarSlotRect& slot = slots[i];
        // Bottom-right corner of the slot, in whichever real character
        // cell that pixel position falls into - bgfx's debug-text font
        // is a coarse 8x16 grid (see debug_overlay.cpp's own comment),
        // so this is an approximation, not pixel-perfect alignment, the
        // same honest trade-off every other debug-text HUD element here
        // already makes.
        const auto cell_x = static_cast<u16>((slot.x + slot.size - static_cast<f32>(kCharWidthPx)) /
                                              static_cast<f32>(kCharWidthPx));
        const auto cell_y = static_cast<u16>((slot.y + slot.size - static_cast<f32>(kCharHeightPx) * 0.5f) /
                                              static_cast<f32>(kCharHeightPx));
        renderer.draw_debug_text(cell_x, cell_y, kColorCount, std::to_string(item.count));
    }
}

}  // namespace lcu::ui
