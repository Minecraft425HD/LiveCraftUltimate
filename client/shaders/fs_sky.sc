$input v_color0

/*
 * Sky fragment shader (Phase 27): the sun/moon billboard - a flat,
 * unlit color, no directional light or noise (see vs_sky.sc). Rendered
 * with depth test/write off into its own bgfx view, which executes
 * before the terrain view so terrain naturally draws over it wherever
 * a block actually occludes it (see Renderer::submit_billboard).
 */

#include <bgfx_shader.sh>

void main()
{
    gl_FragColor = vec4(v_color0, 1.0);
}
