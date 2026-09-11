vec2 a_position  : POSITION;
vec2 a_texcoord0 : TEXCOORD0;
vec4 a_color0    : COLOR0;
// Real per-vertex "sample the atlas at a_texcoord0" flag (Phase 56) -
// 0 for a flat-color quad (Renderer::submit_ui_quad, unchanged since
// Phase 44), 1 for a real item-icon quad (Renderer::
// submit_textured_ui_quad) whose a_texcoord0 above is a real atlas
// sample rect, not a per-quad-local 0..1 UV. See fs_ui2d.sc.
float a_texcoord1 : TEXCOORD1;

vec2 v_texcoord0 : TEXCOORD0;
vec4 v_color0    : COLOR0;
float v_texcoord1 : TEXCOORD1;
