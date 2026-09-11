#pragma once

#include "lcu/core/types.h"
#include "lcu/ui/hud.h"

namespace lcu::rendering {
class Renderer;
}

namespace lcu::ui {

// Queues every real HUD quad (Phase 47) - hotbar slot borders/fills/
// item icons, health/hunger bar background+fill icons - via
// Renderer::submit_ui_quad. Does not flush itself (same "queue now,
// flush once per frame" contract submit_ui_quad's own doc comment
// establishes) - call before Renderer::flush_ui_quads() so these batch
// into the same single draw call as the crosshair/menu backdrop.
void queue_hud_quads(rendering::Renderer& renderer, const HudState& state, u32 screen_width, u32 screen_height);

// Draws each hotbar slot's real held-item count - via the real
// lcu::ui::TextRenderer bitmap-font atlas by default, or bgfx's
// built-in debug-text buffer when `legacy_debug_text` is true (Phase
// 57, same real toggle draw_debug_overlay's own doc comment describes;
// see client/main.cpp's LCU_LEGACY_DEBUG_TEXT). client/main.cpp owns
// the one real Renderer::clear_debug_text() call this frame, before any
// of the debug-text writers (this, draw_debug_overlay, draw_menu_labels)
// run - only relevant when `legacy_debug_text` is true, TextRenderer has
// no shared buffer to clear.
void draw_hud_labels(rendering::Renderer& renderer, const HudState& state, u32 screen_width, u32 screen_height,
                      bool legacy_debug_text);

}  // namespace lcu::ui
