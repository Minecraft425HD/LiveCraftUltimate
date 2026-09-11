#include "lcu/ui/debug_overlay.h"

#include <string>

#include "lcu/math/vec4.h"
#include "lcu/platform/touch_control_layout.h"
#include "lcu/rendering/renderer.h"
#include "lcu/ui/text_renderer.h"

namespace lcu::ui {

namespace {
// bgfx's built-in debug-text font is an 8x16-pixel VGA-style character
// cell (see bgfx::dbgTextPrintf's docs - coordinates are in character
// cells, not pixels). Only used by the legacy_debug_text path.
constexpr u32 kCharWidthPx = 8;
constexpr u32 kCharHeightPx = 16;
constexpr u8 kColorWhiteOnBlack = 0x0f;
constexpr u8 kColorYellowOnBlack = 0x0e;

// Real text-renderer colors (Phase 57) matching the legacy VGA
// attributes above.
constexpr math::Vec4 kTextWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr math::Vec4 kTextYellow{1.0f, 0.85f, 0.1f, 1.0f};
}  // namespace

void draw_debug_overlay(rendering::Renderer& renderer, u32 screen_width, u32 screen_height, f32 fps,
                         const DebugOverlayStats& stats, bool legacy_debug_text) {
    const std::string line0 = "fps=" + std::to_string(static_cast<int>(fps));
    const std::string line1 = "chunks=" + std::to_string(stats.chunks_loaded) +
                               " entities=" + std::to_string(stats.entity_count) +
                               " draws=" + std::to_string(stats.draw_calls) +
                               " jobs=" + std::to_string(stats.unfinished_jobs);

    if (legacy_debug_text) {
        renderer.draw_debug_text(0, 0, kColorWhiteOnBlack, line0);
        renderer.draw_debug_text(0, 1, kColorWhiteOnBlack, line1);
    } else {
        TextRenderer::draw_text(renderer, line0, 0.0f, 0.0f, kTextWhite);
        TextRenderer::draw_text(renderer, line1, 0.0f, kGlyphCellHeight, kTextWhite);
    }

    for (const platform::TouchButtonRect& button : platform::kTouchButtonLayout) {
        const f32 center_x = (button.x0 + button.x1) * 0.5f * static_cast<f32>(screen_width);
        const f32 center_y = (button.y0 + button.y1) * 0.5f * static_cast<f32>(screen_height);
        if (legacy_debug_text) {
            const auto cell_x = static_cast<u16>(center_x / static_cast<f32>(kCharWidthPx));
            const auto cell_y = static_cast<u16>(center_y / static_cast<f32>(kCharHeightPx));
            renderer.draw_debug_text(cell_x, cell_y, kColorYellowOnBlack, button.label);
        } else {
            TextRenderer::draw_text(renderer, button.label, center_x, center_y, kTextYellow);
        }
    }
}

}  // namespace lcu::ui
