#pragma once

#include "lcu/core/types.h"
#include "lcu/ui/inventory_screen.h"

namespace lcu::rendering {
class Renderer;
}

namespace lcu::ui {

// Queues every real inventory-screen quad (Phase 49.1) - a dim
// full-screen backdrop (same convention queue_menu_backdrop already
// uses), each slot's border/fill/item-icon, and the cursor-held stack's
// icon following the mouse. Does not flush itself - call before
// Renderer::flush_ui_quads(), same "queue now, flush once per frame"
// contract every other *_renderer.h queue function here follows.
void queue_inventory_screen_quads(rendering::Renderer& renderer, const InventoryScreenLayout& layout,
                                   const InventoryScreenState& state, u32 screen_width, u32 screen_height);

// Draws each slot's real held-item count (and the cursor stack's own
// count) via bgfx debug text, same mechanism draw_hud_labels/
// draw_menu_labels already use. Does not call Renderer::clear_debug_text()
// itself - client/main.cpp owns the single per-frame clear call (see
// hud_renderer.h's own doc comment for why).
void draw_inventory_screen_labels(rendering::Renderer& renderer, const InventoryScreenLayout& layout,
                                   const InventoryScreenState& state);

}  // namespace lcu::ui
