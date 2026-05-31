#version 450 core

out vec4 o_color;
in vec2 v_uv;

layout (binding = 0) uniform sampler2D u_gbuffer_albedo;
layout (binding = 1) uniform sampler2D u_gbuffer_normal;
layout (binding = 2) uniform sampler2D u_gbuffer_extra;
layout (binding = 3) uniform sampler2D u_gbuffer_depth;

void main() {
    vec4 albedo_data = texture(u_gbuffer_albedo, v_uv);
    vec3 normal      = texture(u_gbuffer_normal, v_uv).xyz;
    vec4 extra_data  = texture(u_gbuffer_extra, v_uv);
    
    uint shading_model = uint(extra_data.a * 255.0 + 0.5);

    vec3 light_dir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normal, light_dir), 0.0);
    vec3 ambient = albedo_data.rgb * 0.05;
    if (shading_model == 0) {
        o_color = vec4(albedo_data.rgb, 1.0);
        
    } else if (shading_model == 1) {
        vec3 diffuse_light = albedo_data.rgb * diff;
        o_color = vec4(ambient + diffuse_light, 1.0);
        
    } else {
        vec3 diffuse_light = albedo_data.rgb * diff;
        o_color = vec4(ambient + diffuse_light, 1.0);
    }
    
    bool debug_normals = false;
    if (debug_normals) {
        o_color = vec4(normal * 0.5 + 0.5, 1.0);
    }
}
