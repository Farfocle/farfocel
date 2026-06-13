#version 450 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;
layout (location = 3) in vec4 a_tangent;

out vec2 v_uv;
out mat3 v_tbn;

layout (std140, binding = 0) buffer TransformSSBO { mat4 u_transforms[]; };
/*
    Camera buffer layout must match fr::GpuCameraData on the CPU side.

    u_cam_forward is not used by the geometry pass, but it is kept here so the
    block layout stays consistent with the lighting shader.
*/
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

layout (location = 0) uniform uint u_transform_idx;

void main() {
    mat4 model = u_transforms[u_transform_idx];
    mat3 normal_matrix = transpose(inverse(mat3(model)));

    vec3 N = normalize(normal_matrix * a_normal);
    
    vec3 t_vec = a_tangent.xyz;


    vec3 T = normalize(normal_matrix * t_vec);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt
    
    float hand = a_tangent.w != 0.0 ? a_tangent.w : 1.0;
    vec3 B = cross(N, T) * hand;
    
    v_tbn = mat3(T, B, N);
    v_uv = a_uv;
    
    gl_Position = u_view_proj * model * vec4(a_pos, 1.0);
}
