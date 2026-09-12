#include "lcu/ui/text_renderer.h"

#include "lcu/assets/font_atlas.h"
#include "lcu/rendering/renderer.h"

namespace lcu::ui {

void TextRenderer::draw_text(rendering::Renderer& renderer, const std::string& text, f32 x, f32 y,
                              const math::Vec4& color, f32 scale) {
    const f32 cell_w = kGlyphCellWidth * scale;
    const f32 cell_h = kGlyphCellHeight * scale;
    f32 cursor_x = x;
    for (char c : text) {
        const assets::GlyphUvRange uv = assets::glyph_uv_range(c);
        renderer.submit_text_glyph_quad(cursor_x, y, cell_w, cell_h, color, uv.u0, uv.v0, uv.u1, uv.v1);
        cursor_x += cell_w;
    }
}

f32 TextRenderer::measure_text_width(const std::string& text, f32 scale) {
    return static_cast<f32>(text.size()) * kGlyphCellWidth * scale;
}

}  // namespace lcu::ui
