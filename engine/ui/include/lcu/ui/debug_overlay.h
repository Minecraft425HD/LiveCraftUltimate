#pragma once

#include "lcu/core/types.h"

namespace lcu::rendering {
class Renderer;
}

namespace lcu::ui {

// Draws a real on-screen HUD (Phase 12, brief section 60's debug overlay
// + Phase 10's TouchInputBackend, whose button rects previously had no
// visual representation at all - see PROJECT_STATE.md Known Limitations
// and DECISIONS.md): the current FPS, and a legend showing where each
// mobile touch-control button is and what it does, drawn at exactly the
// screen positions lcu::platform::kTouchButtonLayout defines for hit-
// testing (touch_control_layout.h) - the same source of truth
// TouchInputBackend hit-tests against, so what's drawn and what's
// tappable can never drift apart.
//
// Goes through Renderer::draw_debug_text() rather than touching bgfx
// itself - ARCHITECTURE.md restricts bgfx-header inclusion to
// engine/rendering, and this keeps that true. Call once per frame, after
// Renderer::begin_frame() and before Renderer::end_frame().
void draw_debug_overlay(rendering::Renderer& renderer, u32 screen_width, u32 screen_height, f32 fps);

}  // namespace lcu::ui
