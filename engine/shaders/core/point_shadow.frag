#version 450 core

in vec2 v_uv;
in vec3 v_world_pos;

layout(binding = 0) uniform sampler2D u_albedo_map;

struct PointShadow {
    mat4 view_proj[6];
    vec4 position_radius;
};

layout(std140, binding = 4) buffer PointShadowsSSBO {
    PointShadow u_point_shadows[];
};

layout(location = 3) uniform uint u_shadow_idx;

void main() {
    if (texture(u_albedo_map, v_uv).a < 0.1) {
        discard;
    }

    vec3 light_pos = u_point_shadows[u_shadow_idx].position_radius.xyz;
    float radius = u_point_shadows[u_shadow_idx].position_radius.w;

    float dist = length(v_world_pos - light_pos);
    gl_FragDepth = clamp(dist / radius, 0.0, 1.0);
}
