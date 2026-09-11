$input v_color0, v_texcoord0, v_texcoord1

/*
 * Sky fragment shader (Phase 27, extended Phase 56): a flat, unlit
 * color (no directional light or noise) for every caller except real
 * dropped item entities, which mix in a real atlas texture sample
 * instead (`v_texcoord1` - see Renderer::submit_world_billboard's own
 * doc comment for the real "invalid atlas_texture = stays flat color"
 * contract). Opaque only (no alpha blending state is set for any of
 * this program's real callers - see renderer.cpp) - a textured item
 * entity's own transparent-background pixels (e.g. a dropped torch)
 * render as whatever color they actually are (black, for this
 * project's own generated textures), a real, honest, documented visual
 * limitation, not a crash/undefined-behavior risk. Rendered with depth
 * test/write off (submit_billboard's own sky view) or on-but-no-write
 * (every other real caller, see their own doc comments) into whichever
 * bgfx view each caller submits to.
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_atlas, 0);

void main()
{
    vec3 tex_color = texture2D(s_atlas, v_texcoord0).rgb;
    vec3 final_color = mix(v_color0, tex_color, v_texcoord1);
    gl_FragColor = vec4(final_color, 1.0);
}
