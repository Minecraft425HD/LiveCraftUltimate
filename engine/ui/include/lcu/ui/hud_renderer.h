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

// Draws each hotbar slot's real held-item count via bgfx debug text -
// the same mechanism draw_debug_overlay/draw_menu_labels already use.
// Calls Renderer::clear_debug_text() itself, so call this AFTER
// draw_debug_overlay()/draw_menu_labels() if either might run the same
// frame - otherwise their own clear would wipe these labels (or this
// would wipe theirs - see client/main.cpp for the real ordering this
// phase settled on).
void draw_hud_labels(rendering::Renderer& renderer, const HudState& state, u32 screen_width, u32 screen_height);

}  // namespace lcu::ui
