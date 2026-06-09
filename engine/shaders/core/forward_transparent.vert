#version 450 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;

out vec2 v_uv;
out vec4 v_clip_pos;

layout(std140, binding = 0) buffer TransformSSBO {
    mat4 u_transforms[];
};

layout(std140, binding = 1) buffer CameraSSBO {
    mat4 u_view_proj;
    mat4 u_inv_view_proj;
    vec4 u_cam_pos;
    vec4 u_cam_forward;

    uvec4 u_counts_debug;
    uvec4 u_flags_reserved;

    vec4 u_lighting_params;
    vec4 u_ao_params;
    vec4 u_ao_params2;

    vec4 u_ibl_params;
    vec4 u_ibl_params2;
    vec4 u_ibl_params3;
};

layout(location = 0) uniform uint u_transform_idx;

void main() {
    mat4 model = u_transforms[u_transform_idx];

    vec4 world_pos = model * vec4(a_pos, 1.0);
    vec4 clip_pos = u_view_proj * world_pos;

    v_uv = a_uv;
    v_clip_pos = clip_pos;

    gl_Position = clip_pos;
}
