#version 450 core

in vec2 v_uv;
in mat3 v_tbn;

layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec2 o_normal;
layout(location = 2) out vec4 o_extra;

layout(binding = 0) uniform sampler2D u_albedo;
layout(binding = 1) uniform sampler2D u_normal;
layout(binding = 2) uniform sampler2D u_extra;

layout(location = 1) uniform uint u_shading_model;

/*
    GBuffer layout:

    o_albedo:
        RGB = linear albedo
        A   = alpha / alpha mask

    o_normal:
        RG = octahedral encoded world-space normal

    o_extra:
        R = metallic / specular parameter
        G = roughness
        B = ambient occlusion
        A = shading model encoded as shading_model / 255.0

    Notes:
    - Albedo textures may be stored as sRGB textures on the GPU.
      Sampling them here returns linear RGB values.
    - The GBuffer albedo render target stores linear color data.
    - Normal and extra textures are data textures and should not be sampled as sRGB.
*/

vec2 signNotZero(vec2 v) {
    return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}

vec2 float32x3_to_oct(in vec3 v) {
    vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
}

void main() {
    vec4 albedo_data = texture(u_albedo, v_uv);

    if (albedo_data.a < 0.1) {
        discard;
    }

    o_albedo = albedo_data;

    vec3 normal_map = texture(u_normal, v_uv).rgb * 2.0 - 1.0;
    vec3 final_normal = normalize(v_tbn * normal_map);
    o_normal = float32x3_to_oct(final_normal);

    vec4 material_data = texture(u_extra, v_uv);

    float metallic_or_specular = clamp(material_data.r, 0.0, 1.0);
    float roughness = clamp(material_data.g, 0.04, 1.0);
    float ambient_occlusion = clamp(material_data.b, 0.0, 1.0);

    o_extra = vec4(
        metallic_or_specular,
        roughness,
        ambient_occlusion,
        float(u_shading_model) / 255.0
    );
}
