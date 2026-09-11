$input a_position, a_color0
$output v_color0

/*
 * Sky vertex shader (Phase 27): the sun/moon billboard. Deliberately
 * separate from vs_chunk.sc - this quad has no normal/UV/lighting
 * concept (it IS a light source, not something lit by one), just a
 * position and a flat color.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_color0 = a_color0;
}
