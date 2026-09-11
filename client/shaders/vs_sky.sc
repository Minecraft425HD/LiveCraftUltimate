$input a_position, a_color0, a_texcoord0, a_texcoord1
$output v_color0, v_texcoord0, v_texcoord1

/*
 * Sky vertex shader (Phase 27): the sun/moon billboard, entity debug
 * boxes, break-progress overlay, and dropped item entities (Phase 50) -
 * every real position+flat-color-or-texture primitive that isn't
 * chunk terrain shares this one program (see renderer.cpp's own
 * per-function vertex structs). No lighting concept - these are either
 * light sources themselves or debug/UI aids, not something lit by the
 * world. Real atlas UV/flag (Phase 56) passed through unchanged -
 * fs_sky.sc does the actual sampling/mixing.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_color0 = a_color0;
    v_texcoord0 = a_texcoord0;
    v_texcoord1 = a_texcoord1;
}
