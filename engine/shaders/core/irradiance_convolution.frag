#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_uv;

layout(binding = 0) uniform samplerCube u_environment_map;

layout(location = 2) uniform uint u_cascade_idx;

const float PI = 3.14159265359;

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

void main() {
    vec3 normal = face_direction(u_cascade_idx, v_uv);

    vec3 up = vec3(0.0, 1.0, 0.0);
    if (abs(dot(up, normal)) > 0.999) {
        up = vec3(1.0, 0.0, 0.0);
    }

    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    vec3 irradiance = vec3(0.0);

    float sample_delta = 0.025;
    float sample_count = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sample_delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sample_delta) {
            vec3 tangent_sample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );

            vec3 sample_vec =
                tangent_sample.x * right +
                tangent_sample.y * up +
                tangent_sample.z * normal;

            irradiance += texture(u_environment_map, sample_vec).rgb *
                          cos(theta) *
                          sin(theta);

            sample_count += 1.0;
        }
    }

    irradiance = 3.14159265359 * irradiance * (1.0 / max(sample_count, 1.0));

    FragColor = vec4(irradiance, 1.0);
}
