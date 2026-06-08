// A basic fragment shader
#version 450 core

in vec3 v_normal;
in vec2 v_uv;

out vec4 o_color;

void main() {
    vec3 normal_color = (normalize(v_normal) * 0.5) + 0.5;
    o_color = vec4(normal_color, 1.0);
}
