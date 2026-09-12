$input v_texcoord0, v_color0, v_texcoord1

/*
 * 2D UI fragment shader (Phase 44, extended Phase 56/57): a real
 * per-vertex tri-state mode (`v_texcoord1`, set by
 * Renderer::submit_ui_quad/submit_textured_ui_quad/
 * submit_text_glyph_quad) picks between three real ways to shade this
 * quad within the SAME draw call/batch:
 *
 *   0 - flat, unlit vertex color (a chrome quad: borders, backgrounds,
 *       health/hunger bars), real alpha blended via
 *       BGFX_STATE_BLEND_ALPHA (see Renderer::flush_ui_quads).
 *   1 - a real block/item atlas sample (`s_atlas`, an item-icon quad) -
 *       the sampled texture's own RGB is drawn as-is (item content
 *       already has real baked-in color), only its alpha is multiplied
 *       by `v_color0`'s own alpha, so a real per-icon fade/tint stays
 *       possible and e.g. a torch icon's real transparent cutout
 *       background composites correctly.
 *   2 - a real font-atlas sample (`s_font`, a text-glyph quad, Phase
 *       57) - the font atlas is deliberately colorless (opaque white
 *       "on" pixels, transparent "off" pixels - see
 *       lcu::assets::generate_glyph_pixels), so BOTH the sampled RGB
 *       AND its alpha are multiplied by `v_color0`, letting one glyph
 *       texture render in any real text color a caller asks for.
 *
 * Implemented as two chained mix() calls rather than a branch - see the
 * inline comments below for why this is correct for all three integer
 * mode values.
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_atlas, 0);
SAMPLER2D(s_font, 1);

void main()
{
    vec4 tex_sample = texture2D(s_atlas, v_texcoord0);
    vec4 font_sample = texture2D(s_font, v_texcoord0);
    vec4 icon_textured = vec4(tex_sample.rgb, tex_sample.a * v_color0.a);
    vec4 font_textured = vec4(font_sample.rgb * v_color0.rgb, font_sample.a * v_color0.a);

    // mode 0 -> clamp(0,0,1)=0 -> stays v_color0.
    // mode 1 -> clamp(1,0,1)=1 -> becomes icon_textured.
    // mode 2 -> clamp(2,0,1)=1 -> becomes icon_textured too (intermediate).
    vec4 result = mix(v_color0, icon_textured, clamp(v_texcoord1, 0.0, 1.0));

    // mode 0 -> max(0-1,0)=0 -> result stays whatever it was above (v_color0).
    // mode 1 -> max(1-1,0)=0 -> result stays icon_textured.
    // mode 2 -> max(2-1,0)=1 -> result becomes font_textured.
    result = mix(result, font_textured, max(v_texcoord1 - 1.0, 0.0));

    gl_FragColor = result;
}
