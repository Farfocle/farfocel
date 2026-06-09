#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_uv;
in vec4 v_clip_pos;

layout(binding = 0) uniform sampler2D u_albedo;
layout(binding = 3) uniform sampler2D u_scene_depth;

void main() {
    vec2 screen_uv = (v_clip_pos.xy / v_clip_pos.w) * 0.5 + 0.5;

    if (screen_uv.x < 0.0 || screen_uv.x > 1.0 ||
        screen_uv.y < 0.0 || screen_uv.y > 1.0) {
        discard;
    }

    float scene_depth = texture(u_scene_depth, screen_uv).r;
    float frag_depth = (v_clip_pos.z / v_clip_pos.w) * 0.5 + 0.5;

    if (frag_depth > scene_depth + 0.00001) {
        discard;
    }

    vec4 albedo = texture(u_albedo, v_uv);

    if (albedo.a <= 0.001) {
        discard;
    }

    FragColor = albedo;
}
