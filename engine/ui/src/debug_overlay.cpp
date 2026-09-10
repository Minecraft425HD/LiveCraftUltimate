#include "lcu/ui/debug_overlay.h"

#include <string>

#include "lcu/platform/touch_control_layout.h"
#include "lcu/rendering/renderer.h"

namespace lcu::ui {

namespace {
// bgfx's built-in debug-text font is an 8x16-pixel VGA-style character
// cell (see bgfx::dbgTextPrintf's docs - coordinates are in character
// cells, not pixels).
constexpr u32 kCharWidthPx = 8;
constexpr u32 kCharHeightPx = 16;
constexpr u8 kColorWhiteOnBlack = 0x0f;
constexpr u8 kColorYellowOnBlack = 0x0e;
}  // namespace

void draw_debug_overlay(rendering::Renderer& renderer, u32 screen_width, u32 screen_height, f32 fps) {
    renderer.clear_debug_text();
    renderer.draw_debug_text(0, 0, kColorWhiteOnBlack, "fps=" + std::to_string(static_cast<int>(fps)));

    for (const platform::TouchButtonRect& button : platform::kTouchButtonLayout) {
        const auto center_x = static_cast<u32>((button.x0 + button.x1) * 0.5f * static_cast<f32>(screen_width));
        const auto center_y = static_cast<u32>((button.y0 + button.y1) * 0.5f * static_cast<f32>(screen_height));
        const auto cell_x = static_cast<u16>(center_x / kCharWidthPx);
        const auto cell_y = static_cast<u16>(center_y / kCharHeightPx);
        renderer.draw_debug_text(cell_x, cell_y, kColorYellowOnBlack, button.label);
    }
}

}  // namespace lcu::ui
