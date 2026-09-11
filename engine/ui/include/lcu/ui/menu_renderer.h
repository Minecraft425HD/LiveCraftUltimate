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

// Draws the current top screen's title plus each row's label/value as
// real bgfx debug text (the same mechanism draw_debug_overlay already
// uses - see its own doc comment for why no font/atlas renderer exists
// yet). Calls Renderer::clear_debug_text() itself, so call this AFTER
// draw_debug_overlay() if both might run the same frame (client/
// main.cpp) - otherwise the overlay's own clear would wipe these rows.
// Deliberately separate from queue_menu_backdrop above: the quads need
// to be queued before this frame's one flush_ui_quads() call, while the
// text needs to be drawn after the debug overlay's own clear, and those
// two points aren't the same place in the frame - see client/main.cpp.
// No-op if `stack` is empty.
void draw_menu_labels(rendering::Renderer& renderer, const MenuStack& stack, u32 screen_width, u32 screen_height);

}  // namespace lcu::ui
