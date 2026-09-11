vec3 a_position  : POSITION;
vec3 a_color0    : COLOR0;
// Real atlas UV + "sample the atlas" flag (Phase 56) - always (0,0)/0
// for submit_billboard/submit_wireframe_box/submit_solid_box (unlit
// flat-color debug/sky primitives, unchanged since Phase 27/36/48), a
// real atlas sample rect + 1 for submit_world_billboard's own real
// dropped-item textures (Phase 50/56). See fs_sky.sc.
vec2 a_texcoord0 : TEXCOORD0;
float a_texcoord1 : TEXCOORD1;

vec3 v_color0    : COLOR0;
vec2 v_texcoord0 : TEXCOORD0;
float v_texcoord1 : TEXCOORD1;
