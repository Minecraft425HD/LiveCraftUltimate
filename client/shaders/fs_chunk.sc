$input v_normal

/*
 * Minimal chunk fragment shader: flat gray-blue material lit by one
 * fixed directional light plus ambient, using the interpolated vertex
 * normal. A placeholder - real per-block color/texturing needs a
 * texture atlas (Phase 12), not built yet.
 */

#include <bgfx_shader.sh>

void main()
{
    vec3 n = normalize(v_normal);
    vec3 light_dir = normalize(vec3(0.3, 1.0, 0.2));
    float diffuse = max(dot(n, light_dir), 0.0);
    float light = 0.25 + 0.75 * diffuse;

    vec3 base_color = vec3(0.62, 0.62, 0.66);
    gl_FragColor = vec4(base_color * light, 1.0);
}
