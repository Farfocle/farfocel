#version 450 core

in vec2 v_uv;
in vec3 v_world_pos;

layout(binding = 0) uniform sampler2D u_albedo_map;

layout(location = 1) uniform uint u_material_idx;
layout(location = 3) uniform uint u_shadow_idx;

const uint MATERIAL_BLEND_MASKED = 1u;

struct Material {
    vec4 base_color_factor;
    vec4 params0;
    uvec4 params1;
};

layout(std140, binding = 7) buffer MaterialSSBO {
    Material u_materials[];
};

struct PointShadow {
    mat4 view_proj[6];
    vec4 position_radius;
};

layout(std140, binding = 4) buffer PointShadowsSSBO {
    PointShadow u_point_shadows[];
};

void main() {
    Material material = u_materials[u_material_idx];
    uint blend_mode = material.params1.y;

    vec4 albedo = texture(u_albedo_map, v_uv);
    albedo *= material.base_color_factor;
    albedo.a *= material.params0.z;

    if (blend_mode == MATERIAL_BLEND_MASKED && albedo.a < material.params0.w) {
        discard;
    }

    vec3 light_pos = u_point_shadows[u_shadow_idx].position_radius.xyz;
    float radius = u_point_shadows[u_shadow_idx].position_radius.w;

    float dist = length(v_world_pos - light_pos);
    gl_FragDepth = clamp(dist / radius, 0.0, 1.0);
}
