$input a_position, a_normal, a_texcoord0, a_color0, a_texcoord1, a_color1
$output v_normal, v_texcoord0, v_color0, v_color1, v_localpos, v_texindex

/*
 * Chunk vertex shader: transforms position, passes the vertex normal,
 * UV, per-block/per-face color (Phase 26 - see
 * lcu::voxel::BlockDefinition::color/side_color/bottom_color), the real
 * atlas tile index (Phase 53 - see lcu::voxel::MeshVertex::
 * texture_index), and the packed per-voxel sky/block light byte (Phase
 * 28 - see lcu::voxel::MeshVertex::light) through to fs_chunk.sc as-is
 * (it does the actual atlas-UV/light unpacking), plus the untransformed
 * chunk-local position for the noise pattern.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = a_normal;
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
    v_color1 = a_color1;
    v_localpos = a_position;
    v_texindex = a_texcoord1;
}
