$input v_texcoord0, v_color0, v_texcoord1

/*
 * 2D UI fragment shader (Phase 44, extended Phase 56): a flat, unlit
 * vertex color for a chrome quad (borders, backgrounds, health/hunger
 * bars - `v_texcoord1` == 0, real alpha blended via
 * BGFX_STATE_BLEND_ALPHA, see Renderer::flush_ui_quads), or a real
 * atlas texture sample for an item-icon quad (`v_texcoord1` == 1, real
 * per-vertex flag set by Renderer::submit_textured_ui_quad) - the
 * sampled texture's own alpha (not `v_color0`'s RGB) is what's drawn,
 * still multiplied by `v_color0`'s own alpha so a real, deliberate
 * per-icon fade/tint stays possible, matching real item content with
 * real transparent pixels (e.g. a torch icon's real cutout background)
 * composites correctly.
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_atlas, 0);

void main()
{
    vec4 tex_sample = texture2D(s_atlas, v_texcoord0);
    vec4 textured = vec4(tex_sample.rgb, tex_sample.a * v_color0.a);
    gl_FragColor = mix(v_color0, textured, v_texcoord1);
}
