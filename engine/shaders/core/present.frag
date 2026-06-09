#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_color;

void main() {
    FragColor = texture(u_color, v_uv);
}
