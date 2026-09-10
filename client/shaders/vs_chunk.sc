$input a_position, a_normal, a_texcoord0, a_color0
$output v_normal, v_texcoord0, v_color0, v_localpos

/*
 * Chunk vertex shader: transforms position, passes the vertex normal,
 * UV, and per-block/per-face color (Phase 26 - see
 * lcu::voxel::BlockDefinition::color/side_color/bottom_color) through
 * to fs_chunk.sc, plus the untransformed chunk-local position for its
 * procedural noise pattern. No texture atlas yet (Phase 12) - color is
 * carried per-vertex instead.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = a_normal;
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
    v_localpos = a_position;
}
