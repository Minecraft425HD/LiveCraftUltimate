vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec3 a_color0    : COLOR0;
// Real atlas tile index (Phase 53, see lcu::voxel::MeshVertex::
// texture_index). A single Uint16 vertex attribute, not normalized, so
// it arrives here as the raw 0-65535 tile index for fs_chunk.sc to
// decompose into atlas grid coordinates itself - same "raw value,
// unpacked shader-side" convention a_color1 below already establishes.
float a_texcoord1 : TEXCOORD1;
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
// Atlas tile index, passed through unchanged from a_texcoord1 (Phase
// 53) - constant across one quad's 4 vertices (a quad is one flat block
// face, see ChunkMeshLayer::add_quad's own doc comment), so per-
// fragment interpolation is a real no-op here, not lossy. A distinct
// TEXCOORD2 semantic from a_texcoord1's own TEXCOORD1 - input vertex
// attributes and output/varying interpolants are separate namespaces in
// this exact shader toolchain (v_normal/a_normal already both reuse
// NORMAL this same way), but a fresh, not-yet-used slot avoids any
// doubt.
float v_texindex : TEXCOORD2;
