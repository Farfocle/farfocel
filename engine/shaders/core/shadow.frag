#version 450 core
in vec2 v_uv;
layout (binding = 0) uniform sampler2D u_albedo_map;

void main() {
    if (texture(u_albedo_map, v_uv).a < 0.1) discard;
}
