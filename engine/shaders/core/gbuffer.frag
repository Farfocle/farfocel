#version 450 core

layout (location = 0) out vec4 out_albedo;
layout (location = 1) out vec4 out_normal;
layout (location = 2) out vec4 out_extra;

in vec3 v_frag_pos;
in vec3 v_normal;
in vec2 v_uv;

layout (binding = 0) uniform sampler2D u_albedo_map;
layout (binding = 1) uniform sampler2D u_normal_map;
layout (binding = 2) uniform sampler2D u_extra_map;

uniform uint u_shading_model;

void main() {
    vec4 albedo = texture(u_albedo_map, v_uv);
    if (albedo.a < 0.1f) discard;
    
    vec3 normal = normalize(v_normal);
    
    vec4 extra = texture(u_extra_map, v_uv);
    
    out_albedo = albedo;
    out_normal = vec4(normal, 1.0);
    
    out_extra = vec4(extra.rgb, float(u_shading_model) / 255.0);
}
