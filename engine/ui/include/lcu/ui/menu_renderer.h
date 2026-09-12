#pragma once

#include "lcu/core/types.h"
#include "lcu/ui/menu_stack.h"

namespace lcu::rendering {
class Renderer;
}

namespace lcu::ui {

// Queues the current top screen's dimmed full-screen backdrop plus a
// highlighted row background as real Phase 44 2D UI quads
// (Renderer::submit_ui_quad - this does not flush itself, matching
// submit_ui_quad's own "queue now, flush once per frame" contract).
// Call before Renderer::flush_ui_quads(). No-op if `stack` is empty.
void queue_menu_backdrop(rendering::Renderer& renderer, const MenuStack& stack, u32 screen_width, u32 screen_height);

// Draws the current top screen's title plus each row's label/value -
// via the real lcu::ui::TextRenderer bitmap-font atlas by default, or
// bgfx's built-in debug-text buffer when `legacy_debug_text` is true
// (Phase 57 - see draw_debug_overlay's own doc comment and client/
// main.cpp's LCU_LEGACY_DEBUG_TEXT). When `legacy_debug_text` is true,
// this shares bgfx's debug-text buffer with draw_debug_overlay/
// draw_hud_labels (client/main.cpp owns the one real
// Renderer::clear_debug_text() call per frame, before any of them run -
// call this one LAST among them so the menu's own rows are what's
// actually left on screen while it's open); TextRenderer has no shared
// buffer to clear or fight over, so that ordering constraint doesn't
// apply when `legacy_debug_text` is false. Deliberately separate from
// queue_menu_backdrop above: the quads need to be queued before this
// frame's one flush_ui_quads() call, while the text is drawn after -
// see client/main.cpp for the real ordering. No-op if `stack` is empty.
void draw_menu_labels(rendering::Renderer& renderer, const MenuStack& stack, u32 screen_width, u32 screen_height,
                       bool legacy_debug_text);

}  // namespace lcu::ui
