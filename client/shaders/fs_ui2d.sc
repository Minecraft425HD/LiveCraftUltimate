$input v_texcoord0, v_color0

/*
 * 2D UI fragment shader (Phase 44): a flat, unlit vertex color (real
 * alpha, blended via BGFX_STATE_BLEND_ALPHA - see
 * Renderer::flush_ui_quads) - the same "position+color, no lighting"
 * approach fs_sky.sc already established for the sun/moon. v_texcoord0
 * is carried through for a future per-quad pattern/atlas lookup (see
 * DECISIONS.md for why item-icon rendering itself, the one thing that
 * would actually consume it, is deferred past this phase) - real,
 * already-plumbed vertex data with no consumer yet, not a promise this
 * shader doesn't keep, since nothing claims it does more than tint a
 * flat quad today.
 */

#include <bgfx_shader.sh>

void main()
{
    gl_FragColor = v_color0;
}
