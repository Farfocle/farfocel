// cook torrance
// todo: understand everything in this
#version 450 core

out vec4 o_color;
in vec2 v_uv;

// gbuffer Textures
layout (binding = 0) uniform sampler2D u_gbuffer_albedo;
layout (binding = 1) uniform sampler2D u_gbuffer_normal;
layout (binding = 2) uniform sampler2D u_gbuffer_extra;
layout (binding = 3) uniform sampler2D u_gbuffer_depth;
layout (binding = 4) uniform sampler2D u_shadow_map;

struct PointLight {
    vec3 position; float radius; vec3 color; float intensity;
};

struct DirLight {
    vec3 direction; float intensity; vec3 color; float padding; mat4 light_view_proj;
};

layout (std140, binding = 1) buffer CameraSSBO {
    mat4 u_view_proj;
    mat4 u_inv_view_proj;
    vec4 u_cam_pos;
};

layout (std140, binding = 2) buffer LightsSSBO {
    PointLight u_point_lights[];
};

layout (std140, binding = 3) buffer DirLightsSSBO {
    DirLight u_dir_lights[];
};

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);}

    float GeometrySchlickGGX(float NdotV, float roughness) {
        float r = (roughness + 1.0);
        float k = (r * r) / 8.0;

        float num = NdotV;
        float denom = NdotV * (1.0 - k) + k;

        return num / denom;
    }

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


void main() {
    vec4 albedo_data = texture(u_gbuffer_albedo, v_uv);
    vec3 normal_data = texture(u_gbuffer_normal, v_uv).xyz;
    vec4 extra_data  = texture(u_gbuffer_extra, v_uv);
    float depth      = texture(u_gbuffer_depth, v_uv).r;

    if (length(normal_data) < 0.1) {
        o_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    uint shading_model = uint(extra_data.a * 255.0 + 0.5);

    if (shading_model == 0) {
        o_color = vec4(albedo_data.rgb, 1.0);
        return;
    }

    vec4 ndc = vec4(v_uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world_pos_homo = u_inv_view_proj * ndc;
    vec3 world_pos = world_pos_homo.xyz / world_pos_homo.w;

    vec3 albedo = pow(albedo_data.rgb, vec3(2.2));    vec3 N = normalize(normal_data);
    vec3 V = normalize(u_cam_pos.xyz - world_pos);

    float roughness = extra_data.g;
    float metallic  = extra_data.b;

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    DirLight sun = u_dir_lights[0];
    vec3 L_sun = normalize(-sun.direction);
    vec3 H_sun = normalize(V + L_sun);    vec3 radiance_sun = sun.color * sun.intensity;

    float NDF_sun = DistributionGGX(N, H_sun, roughness);   
    float G_sun   = GeometrySmith(N, V, L_sun, roughness);      
    vec3 F_sun    = fresnelSchlick(max(dot(H_sun, V), 0.0), F0);       

    vec3 numerator_sun    = NDF_sun * G_sun * F_sun;
    float denominator_sun = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L_sun), 0.0) + 0.0001;
    vec3 specular_sun     = numerator_sun / denominator_sun;

    vec3 kS_sun = F_sun;
    vec3 kD_sun = vec3(1.0) - kS_sun;
    kD_sun *= 1.0 - metallic;     
    float NdotL_sun = max(dot(N, L_sun), 0.0);

    vec4 light_space_pos = sun.light_view_proj * vec4(world_pos, 1.0);
    vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    float shadow = 0.0;
    if (proj_coords.z <= 1.0 && proj_coords.x >= 0.0 && proj_coords.x <= 1.0 && proj_coords.y >= 0.0 && proj_coords.y <= 1.0) {
        float bias = max(0.005 * (1.0 - dot(N, L_sun)), 0.0005);
        vec2 texel_size = 1.0 / textureSize(u_shadow_map, 0);
        for(int x = -2; x <= 2; ++x) {
            for(int y = -2; y <= 2; ++y) {
                float pcf_depth = texture(u_shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r; 
                shadow += (proj_coords.z - bias > pcf_depth) ? 1.0 : 0.0;        
            }    
        }
        shadow /= 25.0;
    }

    Lo += (kD_sun * albedo / PI + specular_sun) * radiance_sun * NdotL_sun * (1.0 - shadow);

    PointLight plight = u_point_lights[0];
    vec3 L_p = plight.position - world_pos;
    float distance_p = length(L_p);
    L_p = L_p / distance_p;
    vec3 H_p = normalize(V + L_p);

    float falloff = clamp(1.0 - (distance_p / plight.radius), 0.0, 1.0);
    float attenuation = falloff * falloff;
    vec3 radiance_p = plight.color * plight.intensity * attenuation;

    float NDF_p = DistributionGGX(N, H_p, roughness);   
    float G_p   = GeometrySmith(N, V, L_p, roughness);      
    vec3 F_p    = fresnelSchlick(max(dot(H_p, V), 0.0), F0);       

    vec3 numerator_p    = NDF_p * G_p * F_p;
    float denominator_p = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L_p), 0.0) + 0.0001;
    vec3 specular_p     = numerator_p / denominator_p;

    vec3 kS_p = F_p;
    vec3 kD_p = vec3(1.0) - kS_p;
    kD_p *= 1.0 - metallic;

    float NdotL_p = max(dot(N, L_p), 0.0);

    Lo += (kD_p * albedo / PI + specular_p) * radiance_p * NdotL_p;

    vec3 ambient = vec3(0.01) * albedo * albedo_data.a;    
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));

    color = pow(color, vec3(1.0 / 2.2));

    o_color = vec4(color, 1.0);
}
