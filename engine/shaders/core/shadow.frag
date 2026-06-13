#version 450 core

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_albedo_map;

layout(location = 1) uniform uint u_material_idx;

const uint MATERIAL_BLEND_MASKED = 1u;

struct Material {
    vec4 base_color_factor;
    vec4 params0;
    uvec4 params1;
};

layout(std140, binding = 7) buffer MaterialSSBO {
    Material u_materials[];
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
}
