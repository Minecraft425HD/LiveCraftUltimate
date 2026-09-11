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
// yet). Does NOT call Renderer::clear_debug_text() itself (Phase 47
// change - see debug_overlay.h's own updated doc comment: client/
// main.cpp now owns the one real clear per frame, since up to three
// systems share this buffer). Call this LAST among them, after
// draw_debug_overlay()/draw_hud_labels(), so the menu's own rows are
// the ones actually left on screen while it's open. Deliberately
// separate from queue_menu_backdrop above: the quads need to be queued
// before this frame's one flush_ui_quads() call, while the text is
// drawn later, after the debug-text buffer is cleared - see
// client/main.cpp for the real ordering. No-op if `stack` is empty.
void draw_menu_labels(rendering::Renderer& renderer, const MenuStack& stack, u32 screen_width, u32 screen_height);

}  // namespace lcu::ui
