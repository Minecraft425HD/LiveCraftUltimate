$input v_normal, v_texcoord0, v_color0, v_color1, v_localpos, v_texindex

/*
 * Chunk fragment shader: real per-block/per-face color (v_color0, from
 * lcu::voxel::BlockDefinition - see Phase 26) OR a real atlas texture
 * sample (Phase 53, when u_useTextures.x != 0 - see fs_chunk.sc's own
 * atlas-UV math below), lit by the real per-voxel sky/block light
 * computed in engine/lighting and packed into v_color1 by
 * mesh_chunk_greedy (Phase 28) - never a per-frame shader computation,
 * matching the brief's "Licht wird NIEMALS pro Frame neu berechnet"
 * (only the sky_scale uniform below changes per frame, one float set
 * once per draw call, not per pixel/per voxel). Replaces the Phase 26
 * fixed fake directional light entirely: that light had no relationship
 * to Phase 27's real sun/moon position, so keeping it alongside real
 * per-voxel light would have double-counted "daylight" and never
 * actually darken at night. Still multiplied by a subtle procedural
 * noise pattern either way (unrelated to lighting - real surface detail
 * both the flat-color path and the textured path share).
 */

#include <bgfx_shader.sh>

// x = DayNightCycle::sky_light_scale() for the frame being drawn (see
// Renderer::submit_chunk_mesh) - the same real time signal Phase 27's
// skybox color already reuses, not a second lighting clock.
uniform vec4 u_skyLightScale;
// Phase 53 - x=1 to sample s_atlas and use it instead of v_color0, x=0
// to stay exactly the Phase 26-52 flat-color/noise path. Set from
// whether Renderer::submit_chunk_mesh was actually handed a valid atlas
// texture this draw - see its own doc comment.
uniform vec4 u_useTextures;
// x,y = one atlas tile's real pitch (grid-cell step) in normalized UV
// space (1/kTilesPerRow); z,w = that same tile's real INSET visible
// width/height (excludes the half-texel border each tile keeps free to
// avoid nearest-filter neighbor bleeding - see lcu::assets::
// tile_uv_range's own doc comment for why this is a UV inset rather
// than literal padding pixels).
uniform vec4 u_tileStep;
// x,y = the per-tile inset offset itself (half a texel, in normalized
// UV), added to each tile's own grid origin below.
uniform vec4 u_tileInset;
SAMPLER2D(s_atlas, 0);

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

    // Real atlas UV mapping (Phase 53.4): v_texcoord0 spans 0..width/
    // 0..height in *block* units across a greedy-meshed quad (see
    // ChunkMeshLayer::add_quad's own doc comment) - fract() wraps it
    // back into 0..1 per block, so a merged multi-block quad tiles the
    // same texture repeatedly instead of stretching one tile across the
    // whole run. v_texindex selects which atlas tile (row-major, see
    // lcu::assets::tile_uv_range) that wrapped-local UV is read from.
    vec2 local_uv = fract(v_texcoord0);
    float tx = mod(v_texindex, 16.0);
    float ty = floor(v_texindex / 16.0);
    vec2 tile_origin = vec2(tx, ty) * u_tileStep.xy + u_tileInset.xy;
    vec2 atlas_uv = tile_origin + local_uv * u_tileStep.zw;
    vec3 tex_color = texture2D(s_atlas, atlas_uv).rgb;

    vec3 base_color = mix(v_color0, tex_color, u_useTextures.x);
    vec3 final_color = base_color * light * noise_factor;
    gl_FragColor = vec4(final_color, 1.0);
}
