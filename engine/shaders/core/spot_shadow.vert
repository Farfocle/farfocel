#version 450 core

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_uv;

out vec2 v_uv;

layout(std140, binding = 0) buffer TransformSSBO {
    mat4 u_models[];
};

struct SpotShadow {
    mat4 view_proj;
    vec4 position_radius;
    vec4 direction_bias;
};

layout(std140, binding = 6) buffer SpotShadowsSSBO {
    SpotShadow u_spot_shadows[];
};

layout(location = 0) uniform uint u_transform_idx;
layout(location = 3) uniform uint u_shadow_idx;

void main() {
    mat4 model = u_models[u_transform_idx];

    gl_Position =
        u_spot_shadows[u_shadow_idx].view_proj *
        model *
        vec4(a_position, 1.0);

    v_uv = a_uv;
}
