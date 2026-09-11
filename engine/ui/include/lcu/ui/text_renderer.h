#pragma once

#include <string>

#include "lcu/core/types.h"
#include "lcu/math/vec4.h"

namespace lcu::rendering {
class Renderer;
}

namespace lcu::ui {

// Real bitmap-font glyph cell size at scale 1.0, in pixels - mirrors
// lcu::assets::font_atlas's own kGlyphWidth/kGlyphHeight (a 5x7 glyph
// plus 1px right/bottom spacing) without engine/ui needing to include
// that header itself; text_renderer.cpp is the one translation unit
// that actually looks up real glyph UVs.
constexpr f32 kGlyphCellWidth = 6.0f;
constexpr f32 kGlyphCellHeight = 8.0f;

// Real on-screen bitmap-font text (Phase 57, brief section "Font-Atlas
// + Real Text-Renderer") - draws one real textured quad per character
// via lcu::rendering::Renderer::submit_text_glyph_quad, sampling
// lcu::assets::font_atlas's own procedurally-generated glyph atlas and
// tinting it to any real color at draw time. Replaces bgfx's built-in
// VGA-style debug-text buffer (Renderer::draw_debug_text) as the
// default real text-drawing path for draw_debug_overlay/draw_hud_labels
// /draw_menu_labels/draw_inventory_screen_labels/
// draw_crafting_table_screen_labels - each of those still supports the
// old debug-text path too, gated behind a real `legacy_debug_text` bool
// parameter each now takes (see client/main.cpp's `LCU_LEGACY_DEBUG_
// TEXT` env toggle), so bgfx's own debug-text buffer stays available as
// a real fallback, not silently removed.
//
// Stateless by design - no persistent font/atlas handle lives here; the
// real font atlas texture is created once in client/main.cpp (mirroring
// how the block/item atlas texture is created and threaded through
// Renderer::flush_ui_quads) and only ever reaches this class indirectly,
// via Renderer::submit_text_glyph_quad queuing a quad that samples
// whichever font atlas flush_ui_quads() is handed that frame - the same
// "TextRenderer only ever queues, Renderer::flush_ui_quads actually
// draws" split every other UI-drawing free function in this namespace
// (queue_hud_quads/draw_hud_labels, etc.) already follows.
class TextRenderer {
   public:
    // Draws `text` with its top-left corner at pixel position (`x`,
    // `y`), tinted `color`, one lcu::rendering::Renderer::
    // submit_text_glyph_quad call per character advancing by
    // kGlyphCellWidth * scale each step (monospace, no kerning - a real,
    // deliberate simplification matching this font's own fixed-width
    // design, see DECISIONS.md). A character outside the real ASCII
    // 32-126 range this font covers falls back to '?' - see
    // lcu::assets::glyph_uv_range.
    static void draw_text(rendering::Renderer& renderer, const std::string& text, f32 x, f32 y,
                           const math::Vec4& color, f32 scale = 1.0f);

    // The real pixel width `text` would occupy at the given scale -
    // every glyph cell is the same fixed width (monospace), so this is
    // just `text.size() * kGlyphCellWidth * scale`, exposed here so a
    // caller that needs to right-align or center text (e.g. a slot's
    // item-count label) doesn't have to know that arithmetic itself.
    static f32 measure_text_width(const std::string& text, f32 scale = 1.0f);
};

}  // namespace lcu::ui
