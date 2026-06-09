#version 450 core
layout (location = 0) in vec3 a_position;
layout (location = 2) in vec2 a_uv;          // UV attribute (matching vertex layout)

out vec2 v_uv;                               // passed to fragment shader

layout (std140, binding = 0) buffer TransformSSBO { mat4 u_models[]; };

struct DirLight {
    vec3 direction;
    float intensity;

    vec3 color;
    float padding;

    mat4 light_view_proj[4];
    vec4 cascade_splits;
    vec4 shadow_params;
};

layout (std140, binding = 3) buffer DirLightsSSBO { DirLight u_dir_lights[]; };

layout (location = 0) uniform uint u_transform_idx;
layout (location = 2) uniform uint u_cascade_idx;

void main() {
    mat4 model = u_models[u_transform_idx];
    gl_Position = u_dir_lights[0].light_view_proj[u_cascade_idx] * model * vec4(a_position, 1.0);
    v_uv = a_uv;   // simply forward the UVs
}
