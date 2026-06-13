#version 450 core

in vec2 v_uv;
in mat3 v_tbn;

layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec2 o_normal;
layout(location = 2) out vec4 o_extra;

layout(binding = 0) uniform sampler2D u_albedo;
layout(binding = 1) uniform sampler2D u_normal;
layout(binding = 2) uniform sampler2D u_extra;

layout(location = 1) uniform uint u_material_idx;

const uint MATERIAL_HAS_ALBEDO = 1u << 0u;
const uint MATERIAL_HAS_NORMAL = 1u << 1u;
const uint MATERIAL_HAS_EXTRA  = 1u << 2u;
const uint MATERIAL_BLEND_MASKED = 1u;

struct Material {
    vec4 base_color_factor;
    vec4 params0;
    uvec4 params1;
};

layout(std140, binding = 7) buffer MaterialSSBO {
    Material u_materials[];
};

/*
    params0:
        x = metallic factor
        y = roughness factor
        z = alpha multiplier
        w = alpha cutoff

    params1:
        x = shading model
        y = blend mode
        z = texture flags
        w = reserved
*/

vec2 signNotZero(vec2 v) {
    return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}

vec2 float32x3_to_oct(in vec3 v) {
    vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
}

bool material_has_texture(uint flags, uint bit) {
    return (flags & bit) != 0u;
}


void main() {
    Material material = u_materials[u_material_idx];

    uint shading_model = material.params1.x;
    uint blend_mode = material.params1.y;
    uint texture_flags = material.params1.z;

    vec4 albedo_data = texture(u_albedo, v_uv);
    albedo_data *= material.base_color_factor;
    albedo_data.a *= material.params0.z;

    if (blend_mode == MATERIAL_BLEND_MASKED && albedo_data.a < material.params0.w) {
        discard;
    }

    o_albedo = vec4(albedo_data.rgb, 1.0);

    o_albedo = albedo_data;

    vec3 normal_map = vec3(0.0, 0.0, 1.0);
    if (material_has_texture(texture_flags, MATERIAL_HAS_NORMAL)) {
        normal_map = texture(u_normal, v_uv).rgb * 2.0 - 1.0;
    }

    vec3 final_normal = normalize(v_tbn * normal_map);
    o_normal = float32x3_to_oct(final_normal);

    vec4 material_data = material_has_texture(texture_flags, MATERIAL_HAS_EXTRA)
        ? texture(u_extra, v_uv)
        : vec4(1.0);

    float metallic_or_specular = material_has_texture(texture_flags, MATERIAL_HAS_EXTRA)
        ? clamp(material_data.r * material.params0.x, 0.0, 1.0)
        : clamp(material.params0.x, 0.0, 1.0);

    float roughness = material_has_texture(texture_flags, MATERIAL_HAS_EXTRA)
        ? clamp(material_data.g * material.params0.y, 0.04, 1.0)
        : clamp(material.params0.y, 0.04, 1.0);

    float ambient_occlusion = material_has_texture(texture_flags, MATERIAL_HAS_EXTRA)
        ? clamp(material_data.b, 0.0, 1.0)
        : 1.0;

    o_extra = vec4(
        metallic_or_specular,
        roughness,
        ambient_occlusion,
        float(shading_model) / 255.0
    );
}
