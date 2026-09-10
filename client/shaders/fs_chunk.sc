$input v_normal, v_texcoord0, v_color0, v_color1, v_localpos

/*
 * Chunk fragment shader: real per-block/per-face color (v_color0, from
 * lcu::voxel::BlockDefinition - see Phase 26) lit by the real per-voxel
 * sky/block light computed in engine/lighting and packed into v_color1
 * by mesh_chunk_greedy (Phase 28) - never a per-frame shader
 * computation, matching the brief's "Licht wird NIEMALS pro Frame neu
 * berechnet" (only the sky_scale uniform below changes per frame, one
 * float set once per draw call, not per pixel/per voxel). Replaces the
 * Phase 26 fixed fake directional light entirely: that light had no
 * relationship to Phase 27's real sun/moon position, so keeping it
 * alongside real per-voxel light would have double-counted "daylight"
 * and never actually darken at night. Still multiplied by a subtle
 * procedural noise pattern (unrelated to lighting - stand-in surface
 * detail until a texture atlas exists, Phase 12). No texturing/UV
 * sampling: v_texcoord0 is carried through but unused here today.
 */

#include <bgfx_shader.sh>

// x = DayNightCycle::sky_light_scale() for the frame being drawn (see
// Renderer::submit_chunk_mesh) - the same real time signal Phase 27's
// skybox color already reuses, not a second lighting clock.
uniform vec4 u_skyLightScale;

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
    // Unpack the single packed light byte (0-255): low nibble = sky
    // light, high nibble = block light, each 0-15 - the exact
    // lcu::lighting::LightStorage packing (see MeshVertex::light).
    float sky = mod(v_color1, 16.0);
    float block = floor(v_color1 / 16.0);
    float light = (sky * u_skyLightScale.x + block) / 15.0;

    // Subtle per-voxel noise (+/-7.5% brightness) from the un-transformed
    // chunk-local position - varies per voxel, not per fragment, so it
    // doesn't crawl as the camera moves, and needs no extra vertex data
    // beyond what's already carried.
    float n3 = hash3(floor(v_localpos * 4.0));
    float noise_factor = 1.0 + (n3 - 0.5) * 0.15;

    vec3 final_color = v_color0 * light * noise_factor;
    gl_FragColor = vec4(final_color, 1.0);
}
