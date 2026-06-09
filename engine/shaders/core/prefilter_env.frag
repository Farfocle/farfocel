#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_uv;

layout(binding = 0) uniform samplerCube u_environment_map;

layout(location = 2) uniform uint u_cascade_idx;
layout(location = 3) uniform uint u_shadow_idx;

const float PI = 3.14159265359;
const float MAX_PREFILTER_LOD = 4.0;

float radical_inverse_vdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint count) {
    return vec2(float(i) / float(count), radical_inverse_vdc(i));
}

vec3 face_direction(uint face, vec2 uv) {
    vec2 p = uv * 2.0 - 1.0;

    if (face == 0u) {
        return normalize(vec3(1.0, -p.y, -p.x));
    }

    if (face == 1u) {
        return normalize(vec3(-1.0, -p.y, p.x));
    }

    if (face == 2u) {
        return normalize(vec3(p.x, 1.0, p.y));
    }

    if (face == 3u) {
        return normalize(vec3(p.x, -1.0, -p.y));
    }

    if (face == 4u) {
        return normalize(vec3(p.x, -p.y, 1.0));
    }

    return normalize(vec3(-p.x, -p.y, -1.0));
}

vec3 importance_sample_ggx(vec2 xi, vec3 N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));

    vec3 H;
    H.x = cos(phi) * sin_theta;
    H.y = sin(phi) * sin_theta;
    H.z = cos_theta;

    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

void main() {
    float roughness = clamp(float(u_shadow_idx) / MAX_PREFILTER_LOD, 0.0, 1.0);

    vec3 N = face_direction(u_cascade_idx, v_uv);
    vec3 R = N;
    vec3 V = R;

    const uint sample_count = 1024u;

    vec3 prefiltered_color = vec3(0.0);
    float total_weight = 0.0;

    for (uint i = 0u; i < sample_count; ++i) {
        vec2 xi = hammersley(i, sample_count);
        vec3 H = importance_sample_ggx(xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float ndotl = max(dot(N, L), 0.0);

        if (ndotl > 0.0) {
            prefiltered_color += textureLod(u_environment_map, L, 0.0).rgb * ndotl;
            total_weight += ndotl;
        }
    }

    prefiltered_color /= max(total_weight, 0.00001);

    FragColor = vec4(prefiltered_color, 1.0);
}
