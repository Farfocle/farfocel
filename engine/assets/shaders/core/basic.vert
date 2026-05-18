// Basic vertex shadder
#version 450 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec3 a_uv;

uniform mat4 u_view_proj;
uniform mat4 u_model;

out vec3 v_normal;
out vec2 v_uv;

void main() {
    v_normal = a_normal;
    v_uv = a_uv;

    vec4 world_pos = u_model * vec4(a_position, 1.0);

    gl_Position = u_view_proj * world_position;
}
