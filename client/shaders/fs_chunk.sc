$input v_normal, v_texcoord0, v_color0, v_localpos

/*
 * Chunk fragment shader (Phase 26): real per-block/per-face color
 * (v_color0, from lcu::voxel::BlockDefinition - see vs_chunk.sc) lit by
 * one fixed directional light plus ambient, multiplied by a subtle
 * procedural noise pattern so a flat-colored block doesn't read as a
 * single flat swatch (no texture atlas exists yet - Phase 12 - so this
 * noise is the stand-in "surface detail" until one does). Still no
 * texturing/UV sampling: v_texcoord0 is carried through but unused here
 * today, kept for parity with the vertex data it's read from.
 */

#include <bgfx_shader.sh>

// A standard cheap 3D hash (Dave Hoskins-style) - deterministic per
// input, no texture lookup needed. Used purely for a subtle per-voxel
// brightness jitter, not anything that needs to be cryptographically
// distributed.
float hash3(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

void main()
{
    vec3 n = normalize(v_normal);
    vec3 light_dir = normalize(vec3(0.3, 1.0, 0.2));
    float diffuse = max(dot(n, light_dir), 0.0);
    // Top-facing surfaces (normal.y > 0) already receive more diffuse
    // light from this mostly-upward light_dir - an explicit small extra
    // lift keeps that "brighter on top" read unambiguous even under a
    // grazing light angle, per Phase 26's ask to use the normal for it.
    float top_lift = max(n.y, 0.0) * 0.08;
    float light = 0.25 + 0.75 * diffuse + top_lift;

    // Subtle per-voxel noise (+/-7.5% brightness) from the un-transformed
    // chunk-local position - varies per voxel, not per fragment, so it
    // doesn't crawl as the camera moves, and needs no extra vertex data
    // beyond what's already carried.
    float n3 = hash3(floor(v_localpos * 4.0));
    float noise_factor = 1.0 + (n3 - 0.5) * 0.15;

    vec3 final_color = v_color0 * light * noise_factor;
    gl_FragColor = vec4(final_color, 1.0);
}
