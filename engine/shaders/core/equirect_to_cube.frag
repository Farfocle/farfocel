#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_equirectangular_map;

layout(location = 2) uniform uint u_cascade_idx;

vec2 spherical_uv(vec3 dir) {
    vec2 uv = vec2(atan(dir.z, dir.x), asin(clamp(dir.y, -1.0, 1.0)));
    uv *= vec2(0.15915494309, 0.31830988618);
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
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

void main() {
    vec3 dir = face_direction(u_cascade_idx, v_uv);
    vec3 color = texture(u_equirectangular_map, spherical_uv(dir)).rgb;

    FragColor = vec4(color, 1.0);
}
