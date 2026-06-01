#version 450 core

layout (location = 0) in vec3 a_position;
layout (location = 2) in vec2 a_uv;

out vec2 v_uv;

layout (std140, binding = 0) buffer TransformSSBO { mat4 u_models[]; };

struct DirLight {
    vec3 direction; float intensity; vec3 color; float padding; mat4 light_view_proj;
};
layout (std140, binding = 3) buffer DirLightsSSBO { DirLight u_dir_lights[]; };

uniform uint u_transform_idx;

void main() {
    mat4 model = u_models[u_transform_idx];
    v_uv = a_uv;

    gl_Position = u_dir_lights[0].light_view_proj * model * vec4(a_position, 1.0);
}
