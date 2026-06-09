#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_albedo;
layout(binding = 1) uniform sampler2D u_normal;
layout(binding = 2) uniform sampler2D u_extra;
layout(binding = 3) uniform sampler2D u_depth;
layout(binding = 4) uniform sampler2DShadow u_shadow_map;

layout(binding = 5) uniform samplerCube u_environment_map;
layout(binding = 6) uniform sampler2D u_hbao;
layout(binding = 7) uniform samplerCube u_point_shadow_maps[4];

layout(binding = 11) uniform sampler2DShadow u_spot_shadow_map;
layout(binding = 12) uniform samplerCube u_irradiance_map;
layout(binding = 13) uniform samplerCube u_prefiltered_map;
layout(binding = 14) uniform sampler2D u_brdf_lut;

layout(std140, binding = 1) buffer CameraSSBO {
    mat4 u_view_proj;
    mat4 u_inv_view_proj;
    vec4 u_cam_pos;
    vec4 u_cam_forward;

    uvec4 u_counts_debug;
    uvec4 u_flags_reserved;

    vec4 u_lighting_params;
    vec4 u_ao_params;
    vec4 u_ao_params2;

    vec4 u_ibl_params;
    vec4 u_ibl_params2;
    vec4 u_ibl_params3;
};

struct PointLight {
    vec3 position;
    float radius;

    vec3 color;
    float intensity;

    int shadow_index;
    float shadow_strength;
    float shadow_bias;
    float padding;
};

layout(std140, binding = 2) buffer PointLightsSSBO {
    PointLight u_point_lights[];
};

struct DirLight {
    vec3 direction;
    float intensity;

    vec3 color;
    float padding;

    mat4 light_view_proj[4];
    vec4 cascade_splits;
    vec4 shadow_params;
    vec4 shadow_filter_params;
};

layout(std140, binding = 3) buffer DirLightsSSBO {
    DirLight u_dir_lights[];
};

struct SpotLight {
    vec4 position_radius;
    vec4 direction_intensity;
    vec4 color_inner_cos;
    vec4 shadow_params;
};

layout(std140, binding = 5) buffer SpotLightsSSBO {
    SpotLight u_spot_lights[];
};

struct SpotShadow {
    mat4 view_proj;
    vec4 position_radius;
    vec4 direction_bias;
};

layout(std140, binding = 6) buffer SpotShadowsSSBO {
    SpotShadow u_spot_shadows[];
};

const float PI = 3.14159265359;

const uint DEBUG_FINAL = 0u;
const uint DEBUG_ALBEDO = 1u;
const uint DEBUG_NORMAL = 2u;
const uint DEBUG_METALLIC_SPECULAR = 3u;
const uint DEBUG_ROUGHNESS = 4u;
const uint DEBUG_AO = 5u;
const uint DEBUG_SHADING_MODEL = 6u;
const uint DEBUG_SHADOW = 7u;
const uint DEBUG_HBAO = 8u;

uint num_point_lights() {
    return u_counts_debug.x;
}

uint num_spot_lights() {
    return u_counts_debug.y;
}

uint num_dir_lights() {
    return u_counts_debug.z;
}

uint debug_mode() {
    return u_counts_debug.w;
}

bool environment_ready() {
    return u_ibl_params.x > 0.5;
}

bool irradiance_ready() {
    return u_ibl_params.y > 0.5;
}

float ibl_diffuse_strength() {
    return max(u_ibl_params.z, 0.0);
}

float ibl_specular_strength() {
    return max(u_ibl_params.w, 0.0);
}

bool ibl_enabled() {
    return u_ibl_params2.w > 0.5;
}

float ibl_occlusion_strength() {
    return clamp(u_ibl_params2.x, 0.0, 1.0);
}

float ibl_occlusion_power() {
    return max(u_ibl_params2.y, 0.001);
}

float ibl_sky_visibility_strength() {
    return clamp(u_ibl_params2.z, 0.0, 1.0);
}

bool prefiltered_ready() {
    return u_ibl_params3.x > 0.5;
}

bool brdf_lut_ready() {
    return u_ibl_params3.y > 0.5;
}

float ibl_max_lod() {
    return max(u_ibl_params3.z, 0.0);
}

vec2 signNotZero(vec2 v) {
    return vec2(
        (v.x >= 0.0) ? 1.0 : -1.0,
        (v.y >= 0.0) ? 1.0 : -1.0
    );
}

vec3 oct_to_float32x3(vec2 e) {
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));

    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    }

    return normalize(v);
}

vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 get_world_pos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = u_inv_view_proj * ndc;
    return wp.xyz / wp.w;
}

float get_exposure() {
    return max(u_lighting_params.x, 0.0);
}

float get_pbr_ambient_strength() {
    return max(u_lighting_params.y, 0.0);
}

float get_standard_ambient_strength() {
    return max(u_lighting_params.z, 0.0);
}

float get_standard_specular_default() {
    return clamp(u_lighting_params.w, 0.0, 1.0);
}

vec3 spot_position(SpotLight light) {
    return light.position_radius.xyz;
}

float spot_radius(SpotLight light) {
    return light.position_radius.w;
}

vec3 spot_direction(SpotLight light) {
    return normalize(light.direction_intensity.xyz);
}

float spot_intensity(SpotLight light) {
    return light.direction_intensity.w;
}

vec3 spot_color(SpotLight light) {
    return light.color_inner_cos.xyz;
}

float spot_inner_cos(SpotLight light) {
    return light.color_inner_cos.w;
}

float spot_outer_cos(SpotLight light) {
    return light.shadow_params.x;
}

int spot_shadow_index(SpotLight light) {
    return int(round(light.shadow_params.y));
}

float spot_shadow_strength(SpotLight light) {
    return clamp(light.shadow_params.z, 0.0, 1.0);
}

float spot_shadow_bias(SpotLight light) {
    return max(light.shadow_params.w, 0.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    vec3 one_minus_roughness = vec3(max(1.0 - roughness, 0.0));
    return F0 + (max(one_minus_roughness, F0) - F0) *
                pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return a2 / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / max(NdotV * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float ggx_v = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggx_l = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    return ggx_v * ggx_l;
}

float standard_shininess(float roughness) {
    return mix(256.0, 8.0, clamp(roughness, 0.04, 1.0));
}

float standard_specular_strength(float value) {
    return value > 0.001 ? clamp(value, 0.0, 1.0) : get_standard_specular_default();
}

float local_light_falloff(float dist, float radius) {
    float attenuation = 1.0 / (dist * dist + 0.0001);
    float falloff = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
    return attenuation * falloff * falloff;
}

float spot_light_factor(SpotLight light, vec3 L) {
    float spot_cos = dot(-L, spot_direction(light));
    return smoothstep(spot_outer_cos(light), spot_inner_cos(light), spot_cos);
}

float evaluate_ibl_visibility(vec3 N, float ambient_ao) {
    float ao = clamp(ambient_ao, 0.0, 1.0);
    float ao_visibility = pow(ao, ibl_occlusion_power());

    float sky_visibility = smoothstep(-0.15, 0.65, N.y);
    sky_visibility = mix(1.0, sky_visibility, ibl_sky_visibility_strength());

    float visibility = ao_visibility * sky_visibility;
    return mix(1.0, visibility, ibl_occlusion_strength());
}

float evaluate_specular_occlusion(float ambient_ao, float roughness, float ndotv) {
    float ao = clamp(ambient_ao, 0.0, 1.0);
    float visibility = clamp(ndotv + ao, 0.0, 1.0);
    float roughness_factor = clamp(roughness, 0.0, 1.0);

    return mix(pow(ao, 2.0), visibility, roughness_factor);
}

vec3 evaluate_diffuse_ibl(vec3 N, vec3 albedo, float metallic, float ambient_ao) {
    if (!ibl_enabled() || !irradiance_ready()) {
        return vec3(0.0);
    }

    vec3 irradiance = texture(u_irradiance_map, N).rgb;

    vec3 kD = vec3(1.0) - vec3(0.04);
    kD *= 1.0 - metallic;

    float visibility = evaluate_ibl_visibility(N, ambient_ao);

    return irradiance * albedo * kD * visibility * ibl_diffuse_strength();
}

vec3 evaluate_specular_ibl(vec3 N, vec3 V, vec3 F0, float roughness, float ambient_ao) {
    if (!ibl_enabled() || !prefiltered_ready() || !brdf_lut_ready()) {
        return vec3(0.0);
    }

    float ndotv = max(dot(N, V), 0.0);
    vec3 R = reflect(-V, N);

    vec3 F = fresnelSchlickRoughness(ndotv, F0, roughness);

    vec3 prefiltered_color =
        textureLod(u_prefiltered_map, R, roughness * ibl_max_lod()).rgb;

    vec2 brdf = texture(u_brdf_lut, vec2(ndotv, roughness)).rg;

    float specular_occlusion =
        evaluate_specular_occlusion(ambient_ao, roughness, ndotv);

    return prefiltered_color *
           (F * brdf.x + brdf.y) *
           specular_occlusion *
           ibl_specular_strength();
}

vec2 directional_poisson_offset(int index) {
    if (index == 0) {
        return vec2(-0.942016, -0.399062);
    }

    if (index == 1) {
        return vec2(0.945586, -0.768907);
    }

    if (index == 2) {
        return vec2(-0.094184, -0.929389);
    }

    if (index == 3) {
        return vec2(0.344959, 0.293878);
    }

    if (index == 4) {
        return vec2(-0.915886, 0.457714);
    }

    if (index == 5) {
        return vec2(-0.815442, -0.879125);
    }

    if (index == 6) {
        return vec2(-0.382775, 0.276768);
    }

    if (index == 7) {
        return vec2(0.974844, 0.756484);
    }

    if (index == 8) {
        return vec2(0.443233, -0.975116);
    }

    if (index == 9) {
        return vec2(0.537430, -0.473734);
    }

    if (index == 10) {
        return vec2(-0.264969, -0.418930);
    }

    if (index == 11) {
        return vec2(0.791975, 0.190902);
    }

    if (index == 12) {
        return vec2(-0.241888, 0.997065);
    }

    if (index == 13) {
        return vec2(-0.814100, 0.914376);
    }

    if (index == 14) {
        return vec2(0.199841, 0.786414);
    }

    return vec2(0.143832, -0.141008);
}

float sample_directional_shadow_atlas(vec2 atlas_uv, float depth) {
    return texture(u_shadow_map, vec3(atlas_uv, depth));
}

float calculate_shadow(vec3 world_pos, vec3 N, vec3 light_dir, float view_depth) {
    uint cascade = 0u;

    if (view_depth > u_dir_lights[0].cascade_splits.x) {
        cascade = 1u;
    }

    if (view_depth > u_dir_lights[0].cascade_splits.y) {
        cascade = 2u;
    }

    vec4 light_space_pos = u_dir_lights[0].light_view_proj[cascade] * vec4(world_pos, 1.0);
    vec3 cascade_coords = light_space_pos.xyz / light_space_pos.w;
    cascade_coords = cascade_coords * 0.5 + 0.5;

    if (cascade_coords.x < 0.0 || cascade_coords.x > 1.0 ||
        cascade_coords.y < 0.0 || cascade_coords.y > 1.0 ||
        cascade_coords.z < 0.0 || cascade_coords.z > 1.0) {
        return 0.0;
    }

    vec2 tile_offset = vec2(float(cascade % 2u) * 0.5, float(cascade / 2u) * 0.5);
    vec2 tile_size = vec2(0.5);
    vec2 atlas_uv = tile_offset + cascade_coords.xy * tile_size;

    vec4 shadow_params = u_dir_lights[0].shadow_params;

    float min_bias = max(shadow_params.x, 0.0);
    float slope_bias = max(shadow_params.y, 0.0);
    float cascade_bias_scale = max(shadow_params.z, 0.0);
    float shadow_strength = clamp(shadow_params.w, 0.0, 1.0);

    float ndotl = clamp(dot(N, light_dir), 0.0, 1.0);

    float bias = max(slope_bias * (1.0 - ndotl), min_bias);
    bias *= 1.0 + float(cascade) * cascade_bias_scale;

    float compare_depth = cascade_coords.z - bias;

    ivec2 atlas_size = textureSize(u_shadow_map, 0);
    vec2 texel_size = 1.0 / vec2(atlas_size);

    vec2 tile_min = tile_offset + texel_size * 2.0;
    vec2 tile_max = tile_offset + tile_size - texel_size * 2.0;

    vec4 filter_params = u_dir_lights[0].shadow_filter_params;

    float base_radius = max(filter_params.x, 0.0);
    float cascade_radius_scale = max(filter_params.y, 0.0);
    float radius = clamp(base_radius + float(cascade) * cascade_radius_scale, 0.0, 8.0);

    float visibility = 0.0;

    for (int i = 0; i < 16; ++i) {
        vec2 sample_uv = atlas_uv + directional_poisson_offset(i) * texel_size * radius;
        sample_uv = clamp(sample_uv, tile_min, tile_max);
        visibility += sample_directional_shadow_atlas(sample_uv, compare_depth);
    }

    visibility *= 1.0 / 16.0;

    return (1.0 - visibility) * shadow_strength;
}

float sample_point_shadow_map(int shadow_index, vec3 direction) {
    if (shadow_index == 0) {
        return texture(u_point_shadow_maps[0], direction).r;
    }

    if (shadow_index == 1) {
        return texture(u_point_shadow_maps[1], direction).r;
    }

    if (shadow_index == 2) {
        return texture(u_point_shadow_maps[2], direction).r;
    }

    if (shadow_index == 3) {
        return texture(u_point_shadow_maps[3], direction).r;
    }

    return 1.0;
}

vec3 point_shadow_offset(int index) {
    if (index == 0) {
        return vec3(0.0, 0.0, 0.0);
    }

    if (index == 1) {
        return vec3(1.0, 1.0, 1.0);
    }

    if (index == 2) {
        return vec3(-1.0, 1.0, 1.0);
    }

    if (index == 3) {
        return vec3(1.0, -1.0, 1.0);
    }

    if (index == 4) {
        return vec3(-1.0, -1.0, 1.0);
    }

    if (index == 5) {
        return vec3(1.0, 1.0, -1.0);
    }

    if (index == 6) {
        return vec3(-1.0, 1.0, -1.0);
    }

    if (index == 7) {
        return vec3(1.0, -1.0, -1.0);
    }

    if (index == 8) {
        return vec3(-1.0, -1.0, -1.0);
    }

    if (index == 9) {
        return vec3(1.0, 0.0, 0.0);
    }

    if (index == 10) {
        return vec3(-1.0, 0.0, 0.0);
    }

    if (index == 11) {
        return vec3(0.0, 1.0, 0.0);
    }

    if (index == 12) {
        return vec3(0.0, -1.0, 0.0);
    }

    if (index == 13) {
        return vec3(0.0, 0.0, 1.0);
    }

    if (index == 14) {
        return vec3(0.0, 0.0, -1.0);
    }

    return vec3(0.57735, -0.57735, 0.57735);
}

float calculate_point_shadow(PointLight light, vec3 world_pos) {
    if (light.shadow_index < 0) {
        return 0.0;
    }

    vec3 light_to_frag = world_pos - light.position;
    float dist = length(light_to_frag);

    if (dist <= 0.001 || dist >= light.radius) {
        return 0.0;
    }

    float current_depth = dist / light.radius;
    float bias = max(light.shadow_bias, 0.0);
    float strength = clamp(light.shadow_strength, 0.0, 1.0);

    float filter_radius = light.radius * mix(0.0025, 0.0100, clamp(current_depth, 0.0, 1.0));

    float visibility = 0.0;
    const int sample_count = 16;

    for (int i = 0; i < sample_count; ++i) {
        vec3 offset = normalize(point_shadow_offset(i)) * filter_radius;
        vec3 sample_dir = light_to_frag + offset;

        float closest_depth = sample_point_shadow_map(light.shadow_index, sample_dir);
        visibility += (current_depth - bias <= closest_depth) ? 1.0 : 0.0;
    }

    visibility /= float(sample_count);

    return (1.0 - visibility) * strength;
}

vec4 spot_shadow_tile(int shadow_index) {
    int tile = max(shadow_index, 0);
    int col = tile % 2;
    int row = tile / 2;

    vec2 offset = vec2(float(col), float(row)) * 0.5;
    return vec4(offset, 0.5, 0.5);
}

float calculate_spot_shadow(SpotLight light, vec3 world_pos) {
    int shadow_index = spot_shadow_index(light);
    if (shadow_index < 0) {
        return 0.0;
    }

    vec4 light_space_pos = u_spot_shadows[shadow_index].view_proj * vec4(world_pos, 1.0);
    vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    if (proj_coords.x < 0.0 || proj_coords.x > 1.0 ||
        proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
        proj_coords.z < 0.0 || proj_coords.z > 1.0) {
        return 0.0;
    }

    vec4 tile = spot_shadow_tile(shadow_index);
    vec2 atlas_uv = tile.xy + proj_coords.xy * tile.zw;

    float bias = max(spot_shadow_bias(light), u_spot_shadows[shadow_index].direction_bias.w);
    float compare_depth = proj_coords.z - bias;

    ivec2 atlas_size = textureSize(u_spot_shadow_map, 0);
    vec2 texel_size = 1.0 / vec2(atlas_size);

    vec2 tile_min = tile.xy + texel_size * 2.0;
    vec2 tile_max = tile.xy + tile.zw - texel_size * 2.0;

    float visibility = 0.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texel_size * 1.5;
            vec2 sample_uv = clamp(atlas_uv + offset, tile_min, tile_max);

            visibility += texture(u_spot_shadow_map, vec3(sample_uv, compare_depth));
        }
    }

    visibility /= 9.0;

    return (1.0 - visibility) * spot_shadow_strength(light);
}

vec3 evaluate_pbr_light(
    vec3 L,
    vec3 V,
    vec3 N,
    vec3 F0,
    vec3 albedo,
    float roughness,
    float metallic,
    vec3 radiance
) {
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0) {
        return vec3(0.0);
    }

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;

    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    float depth = texture(u_depth, v_uv).r;

    if (depth == 1.0) {
        if (debug_mode() != DEBUG_FINAL) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        if (!environment_ready()) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        vec4 view_space_pos = u_inv_view_proj * vec4(v_uv * 2.0 - 1.0, 1.0, 1.0);
        vec3 world_dir = normalize(view_space_pos.xyz / view_space_pos.w - u_cam_pos.xyz);

        vec3 sky_color = textureLod(u_environment_map, world_dir, 0.0).rgb;

        vec3 mapped = ACESFilm(max(sky_color * get_exposure(), vec3(0.0)));
        FragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);
        return;
    }

    vec3 albedo = texture(u_albedo, v_uv).rgb;
    vec3 N = oct_to_float32x3(texture(u_normal, v_uv).xy);
    vec4 extra = texture(u_extra, v_uv);

    float metallic_or_specular = clamp(extra.r, 0.0, 1.0);
    float roughness = clamp(extra.g, 0.04, 1.0);
    float ao = clamp(extra.b, 0.0, 1.0);

    float hbao = clamp(texture(u_hbao, v_uv).r, 0.0, 1.0);
    float ambient_ao = ao * hbao;

    int shading_model = int(round(extra.a * 255.0));

    vec3 world_pos = get_world_pos(v_uv, depth);
    vec3 view_dir = u_cam_pos.xyz - world_pos;
    vec3 V = length(view_dir) > 0.001 ? normalize(view_dir) : vec3(0.0, 0.0, 1.0);

    float view_depth = max(dot(world_pos - u_cam_pos.xyz, normalize(u_cam_forward.xyz)), 0.0);

    if (debug_mode() != DEBUG_FINAL) {
        if (debug_mode() == DEBUG_ALBEDO) {
            FragColor = vec4(pow(clamp(albedo, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2)), 1.0);
            return;
        }

        if (debug_mode() == DEBUG_NORMAL) {
            FragColor = vec4(N * 0.5 + 0.5, 1.0);
            return;
        }

        if (debug_mode() == DEBUG_METALLIC_SPECULAR) {
            FragColor = vec4(vec3(metallic_or_specular), 1.0);
            return;
        }

        if (debug_mode() == DEBUG_ROUGHNESS) {
            FragColor = vec4(vec3(roughness), 1.0);
            return;
        }

        if (debug_mode() == DEBUG_AO) {
            FragColor = vec4(vec3(ao), 1.0);
            return;
        }

        if (debug_mode() == DEBUG_SHADING_MODEL) {
            vec3 model_color = vec3(1.0, 0.25, 0.15);

            if (shading_model == 0) {
                model_color = vec3(0.2, 0.4, 1.0);
            } else if (shading_model == 1) {
                model_color = vec3(0.2, 1.0, 0.2);
            }

            FragColor = vec4(model_color, 1.0);
            return;
        }

        if (debug_mode() == DEBUG_SHADOW) {
            float shadow = 0.0;

            if (num_dir_lights() > 0u) {
                vec3 L = normalize(-u_dir_lights[0].direction);
                shadow = calculate_shadow(world_pos, N, L, view_depth);
            }

            FragColor = vec4(vec3(shadow), 1.0);
            return;
        }

        if (debug_mode() == DEBUG_HBAO) {
            FragColor = vec4(vec3(hbao), 1.0);
            return;
        }
    }

    vec3 color_out = vec3(0.0);

    if (shading_model == 0) {
        color_out = albedo * ao;
    } else if (shading_model == 1) {
        vec3 ambient = vec3(get_standard_ambient_strength()) * albedo * ambient_ao;
        vec3 Lo = vec3(0.0);

        float shininess = standard_shininess(roughness);
        float specular_strength = standard_specular_strength(metallic_or_specular);

        if (num_dir_lights() > 0u) {
            vec3 L = normalize(-u_dir_lights[0].direction);
            vec3 H = normalize(V + L);

            float diff = max(dot(N, L), 0.0);
            float spec = pow(max(dot(N, H), 0.0), shininess) * specular_strength;

            vec3 radiance = u_dir_lights[0].color * u_dir_lights[0].intensity;
            float shadow = calculate_shadow(world_pos, N, L, view_depth);

            Lo += (albedo * diff + vec3(spec)) * radiance * (1.0 - shadow);
        }

        for (uint i = 0u; i < num_point_lights(); ++i) {
            PointLight light = u_point_lights[i];

            vec3 L = light.position - world_pos;
            float dist = length(L);
            L = dist > 0.001 ? L / dist : vec3(0.0, 1.0, 0.0);

            if (dist < light.radius) {
                vec3 radiance =
                    light.color * light.intensity * local_light_falloff(dist, light.radius);

                vec3 H = normalize(V + L);
                float diff = max(dot(N, L), 0.0);
                float spec = pow(max(dot(N, H), 0.0), shininess) * specular_strength;

                float shadow = calculate_point_shadow(light, world_pos);
                Lo += (albedo * diff + vec3(spec)) * radiance * (1.0 - shadow);
            }
        }

        for (uint i = 0u; i < num_spot_lights(); ++i) {
            SpotLight light = u_spot_lights[i];

            vec3 L = spot_position(light) - world_pos;
            float dist = length(L);
            L = dist > 0.001 ? L / dist : vec3(0.0, 1.0, 0.0);

            float radius = spot_radius(light);

            if (dist < radius) {
                float spot = spot_light_factor(light, L);

                if (spot > 0.0) {
                    vec3 radiance =
                        spot_color(light) *
                        spot_intensity(light) *
                        local_light_falloff(dist, radius) *
                        spot;

                    vec3 H = normalize(V + L);
                    float diff = max(dot(N, L), 0.0);
                    float spec = pow(max(dot(N, H), 0.0), shininess) * specular_strength;

                    float shadow = calculate_spot_shadow(light, world_pos);
                    Lo += (albedo * diff + vec3(spec)) * radiance * (1.0 - shadow);
                }
            }
        }

        color_out = ambient + Lo;
    } else {
        float metallic = metallic_or_specular;

        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 Lo = vec3(0.0);

        if (num_dir_lights() > 0u) {
            vec3 L = normalize(-u_dir_lights[0].direction);
            vec3 radiance = u_dir_lights[0].color * u_dir_lights[0].intensity;

            vec3 light_eval =
                evaluate_pbr_light(L, V, N, F0, albedo, roughness, metallic, radiance);

            float shadow = calculate_shadow(world_pos, N, L, view_depth);
            Lo += light_eval * (1.0 - shadow);
        }

        for (uint i = 0u; i < num_point_lights(); ++i) {
            PointLight light = u_point_lights[i];

            vec3 L = light.position - world_pos;
            float dist = length(L);
            L = dist > 0.001 ? L / dist : vec3(0.0, 1.0, 0.0);

            if (dist < light.radius) {
                vec3 radiance =
                    light.color * light.intensity * local_light_falloff(dist, light.radius);

                vec3 light_eval =
                    evaluate_pbr_light(L, V, N, F0, albedo, roughness, metallic, radiance);

                float shadow = calculate_point_shadow(light, world_pos);
                Lo += light_eval * (1.0 - shadow);
            }
        }

        for (uint i = 0u; i < num_spot_lights(); ++i) {
            SpotLight light = u_spot_lights[i];

            vec3 L = spot_position(light) - world_pos;
            float dist = length(L);
            L = dist > 0.001 ? L / dist : vec3(0.0, 1.0, 0.0);

            float radius = spot_radius(light);

            if (dist < radius) {
                float spot = spot_light_factor(light, L);

                if (spot > 0.0) {
                    vec3 radiance =
                        spot_color(light) *
                        spot_intensity(light) *
                        local_light_falloff(dist, radius) *
                        spot;

                    vec3 light_eval =
                        evaluate_pbr_light(L, V, N, F0, albedo, roughness, metallic, radiance);

                    float shadow = calculate_spot_shadow(light, world_pos);
                    Lo += light_eval * (1.0 - shadow);
                }
            }
        }

        vec3 fallback_ambient = vec3(get_pbr_ambient_strength()) * albedo * ambient_ao;
        vec3 diffuse_ibl = evaluate_diffuse_ibl(N, albedo, metallic, ambient_ao);
        vec3 specular_ibl = evaluate_specular_ibl(N, V, F0, roughness, ambient_ao);

        color_out = fallback_ambient + diffuse_ibl + specular_ibl + Lo;
    }

    vec3 mapped = ACESFilm(max(color_out * get_exposure(), vec3(0.0)));
    FragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);
}
