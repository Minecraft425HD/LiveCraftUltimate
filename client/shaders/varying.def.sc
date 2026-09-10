vec3 a_position  : POSITION;
vec3 a_normal    : NORMAL;
vec2 a_texcoord0 : TEXCOORD0;
vec3 a_color0    : COLOR0;

vec3 v_normal    : NORMAL;
vec2 v_texcoord0 : TEXCOORD0;
vec3 v_color0    : COLOR0;
// Chunk-local (pre-model-transform) position (Phase 26) - a cheap,
// deterministic-per-voxel coordinate for the fragment shader's
// procedural noise pattern. Doesn't need to be true world space (see
// fs_chunk.sc): it only needs to vary per voxel, which chunk-local
// coordinates already do.
vec3 v_localpos  : TEXCOORD1;
