vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec3 a_color0    : COLOR0;
// Packed per-voxel light (Phase 28) - low nibble sky, high nibble
// block, each 0-15 (see lcu::voxel::MeshVertex::light). A single Uint8
// vertex attribute, not normalized, so it arrives here as the raw 0-255
// byte value for fs_chunk.sc to unpack itself.
float a_color1   : COLOR1;

vec3 v_normal    : NORMAL;
vec2 v_texcoord0 : TEXCOORD0;
vec3 v_color0    : COLOR0;
float v_color1   : COLOR1;
// Chunk-local (pre-model-transform) position (Phase 26) - a cheap,
// deterministic-per-voxel coordinate for the fragment shader's
// procedural noise pattern. Doesn't need to be true world space (see
// fs_chunk.sc): it only needs to vary per voxel, which chunk-local
// coordinates already do.
vec3 v_localpos  : TEXCOORD1;
