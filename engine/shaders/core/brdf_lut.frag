#version 450 core

layout(location = 0) out vec2 FragColor;

in vec2 v_uv;

const float PI = 3.14159265359;

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

vec3 importance_sample_ggx(vec2 xi, vec3 N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));

    vec3 H;
    H.x = cos(phi) * sin_theta;
    H.y = sin(phi) * sin_theta;
    H.z = cos_theta;

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float geometry_schlick_ggx(float ndotv, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;

    float denom = ndotv * (1.0 - k) + k;
    return ndotv / max(denom, 0.00001);
}

float geometry_smith(float ndotv, float ndotl, float roughness) {
    float ggx_v = geometry_schlick_ggx(ndotv, roughness);
    float ggx_l = geometry_schlick_ggx(ndotl, roughness);
    return ggx_v * ggx_l;
}

vec2 integrate_brdf(float ndotv, float roughness) {
    vec3 V;
    V.x = sqrt(max(1.0 - ndotv * ndotv, 0.0));
    V.y = 0.0;
    V.z = ndotv;

    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;

    const uint sample_count = 1024u;

    for (uint i = 0u; i < sample_count; ++i) {
        vec2 xi = hammersley(i, sample_count);
        vec3 H = importance_sample_ggx(xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float ndotl = max(L.z, 0.0);
        float ndoth = max(H.z, 0.0);
        float vdoth = max(dot(V, H), 0.0);

        if (ndotl > 0.0) {
            float G = geometry_smith(ndotv, ndotl, roughness);
            float G_Vis = (G * vdoth) / max(ndoth * ndotv, 0.00001);
            float Fc = pow(1.0 - vdoth, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(sample_count);
    B /= float(sample_count);

    return vec2(A, B);
}

void main() {
    float ndotv = clamp(v_uv.x, 0.0, 1.0);
    float roughness = clamp(v_uv.y, 0.0, 1.0);

    FragColor = integrate_brdf(ndotv, roughness);
}
