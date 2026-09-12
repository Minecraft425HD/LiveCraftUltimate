$input a_position, a_texcoord0, a_color0, a_texcoord1
$output v_texcoord0, v_color0, v_texcoord1

/*
 * 2D UI vertex shader (Phase 44, extended Phase 56): screen-space
 * position (pixels, top-left origin) mapped to clip space by
 * Renderer::flush_ui_quads' own orthographic projection (set as this
 * view's proj, with an identity view/model, so u_modelViewProj already
 * does the whole pixel-to-clip mapping - see Mat4::orthographic). No
 * lighting concept, same as vs_sky.sc - a HUD element is never "lit" by
 * the world. Real atlas UV/flag (Phase 56) passed through unchanged -
 * fs_ui2d.sc does the actual sampling/mixing.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 0.0, 1.0));
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
    v_texcoord1 = a_texcoord1;
}
