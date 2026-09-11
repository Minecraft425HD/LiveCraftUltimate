#pragma once

#include "lcu/core/types.h"

namespace lcu::rendering {
class Renderer;
}

namespace lcu::ui {

// Real, currently-available numbers for the extended debug overlay
// (Phase 36, brief section 60: "CPU/GPU/RAM/chunks/entities/ping/
// bandwidth/draw-calls/jobs"). Deliberately only the subset this
// codebase actually has a real data source for today - CPU/GPU/RAM
// need per-platform OS/driver queries this project doesn't have yet
// (a Linux-only reader would leave every other target platform
// unequal - see DECISIONS.md), and ping/bandwidth need per-connection
// RTT/byte-counters `engine/network::Connection` doesn't track yet.
// Adding a field here is adding a real, wired-up number, never a
// placeholder - see BUILD_STATUS.md/PROJECT_STATE.md for what's
// genuinely available at any given phase.
struct DebugOverlayStats {
    u32 chunks_loaded = 0;
    u32 entity_count = 0;
    u32 draw_calls = 0;
    u64 unfinished_jobs = 0;
};

// Draws a real on-screen HUD (Phase 12, brief section 60's debug overlay
// + Phase 10's TouchInputBackend, whose button rects previously had no
// visual representation at all - see PROJECT_STATE.md Known Limitations
// and DECISIONS.md): the current FPS plus (Phase 36) chunks/entities/
// draw-calls/jobs, and a legend showing where each mobile touch-control
// button is and what it does, drawn at exactly the screen positions
// lcu::platform::kTouchButtonLayout defines for hit-testing
// (touch_control_layout.h) - the same source of truth TouchInputBackend
// hit-tests against, so what's drawn and what's tappable can never drift
// apart.
//
// Goes through Renderer::draw_debug_text() rather than touching bgfx
// itself - ARCHITECTURE.md restricts bgfx-header inclusion to
// engine/rendering, and this keeps that true. Call once per frame, after
// Renderer::begin_frame() and before Renderer::end_frame().
//
// Does NOT call Renderer::clear_debug_text() itself (Phase 47 change):
// the debug-text buffer is now shared by up to three real writers this
// same frame (this overlay, hud_renderer's item-count labels,
// menu_renderer's row labels) - see client/main.cpp for the one real
// clear_debug_text() call that now owns clearing it once per frame,
// before any of the three run.
void draw_debug_overlay(rendering::Renderer& renderer, u32 screen_width, u32 screen_height, f32 fps,
                         const DebugOverlayStats& stats);

}  // namespace lcu::ui
