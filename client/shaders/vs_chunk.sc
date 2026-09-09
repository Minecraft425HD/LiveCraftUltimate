$input a_position, a_normal
$output v_normal

/*
 * Minimal chunk vertex shader: transforms position, passes the vertex
 * normal through untouched for a simple directional-light fragment
 * shader (fs_chunk.sc). No texturing yet - there is no texture atlas
 * (Phase 12), so this only needs to prove the pipeline (mesh -> GPU
 * buffers -> shader -> draw call) actually works.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = a_normal;
}
