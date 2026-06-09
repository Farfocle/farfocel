#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_uv;

out vec2 v_uv;
out vec3 v_world_pos;

layout(std140, binding = 0) buffer TransformSSBO {
    mat4 u_models[];
};

struct PointShadow {
    mat4 view_proj[6];
    vec4 position_radius;
};

layout(std140, binding = 4) buffer PointShadowsSSBO {
    PointShadow u_point_shadows[];
};

layout(location = 0) uniform uint u_transform_idx;
layout(location = 2) uniform uint u_cascade_idx;
layout(location = 3) uniform uint u_shadow_idx;

void main() {
    mat4 model = u_models[u_transform_idx];

    vec4 world_pos = model * vec4(a_position, 1.0);
    v_world_pos = world_pos.xyz;
    v_uv = a_uv;

    gl_Position =
        u_point_shadows[u_shadow_idx].view_proj[u_cascade_idx] *
        world_pos;
}

