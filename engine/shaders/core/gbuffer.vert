#version 450 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;

out vec3 v_frag_pos;
out vec3 v_normal;
out vec2 v_uv;

layout (std140, binding = 0) buffer TransformSSBO {
    mat4 u_models[];
};

layout (std140, binding = 1) buffer CameraSSBO {
    mat4 u_view_proj;
};

uniform uint u_transform_idx;

void main() {
    mat4 model = u_models[u_transform_idx];
    
    vec4 world_pos = model * vec4(a_position, 1.0);
    v_frag_pos = world_pos.xyz;
    v_normal = mat3(model) * a_normal; 
    v_uv = a_uv;
    
    gl_Position = u_view_proj * world_pos;
}
