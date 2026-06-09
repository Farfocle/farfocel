#version 450 core

layout(location = 0) out vec4 FragColor;

in vec2 v_uv;

layout(binding = 0) uniform sampler2D u_normal;
layout(binding = 1) uniform sampler2D u_depth;


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


vec2 signNotZero(vec2 v) {
    return vec2(
        (v.x >= 0.0) ? 1.0 : -1.0,
        (v.y >= 0.0) ? 1.0 : -1.0
    );
}

vec3 oct_to_float32x3(vec2 e) {
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));

    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
    }

    return normalize(v);
}

vec3 get_world_pos(vec2 uv, float depth) {
    vec4 ndc = vec4(
        uv * 2.0 - 1.0,
        depth * 2.0 - 1.0,
        1.0
    );

    vec4 wp = u_inv_view_proj * ndc;
    return wp.xyz / wp.w;
}

float get_view_depth(vec3 world_pos) {
    vec3 cam_forward = normalize(u_cam_forward.xyz);
    return max(dot(world_pos - u_cam_pos.xyz, cam_forward), 0.001);
}

float sample_occlusion(
    vec3 world_pos,
    vec3 normal,
    float center_view_depth,
    vec2 sample_uv,
    float radius,
    float bias,
    float thickness
) {
    float sample_depth = texture(u_depth, sample_uv).r;

    if (sample_depth >= 1.0) {
        return 0.0;
    }

    vec3 sample_pos = get_world_pos(sample_uv, sample_depth);
    vec3 delta = sample_pos - world_pos;

    float dist = length(delta);
    if (dist <= 0.0001 || dist > radius + thickness) {
        return 0.0;
    }

    float sample_view_depth = get_view_depth(sample_pos);
    float depth_delta = abs(sample_view_depth - center_view_depth);

    /*
        Reject samples across strong depth discontinuities. This reduces AO halos
        around object silhouettes and foreground/background edges.
    */
    float max_depth_delta = radius + thickness;
    if (depth_delta > max_depth_delta) {
        return 0.0;
    }

    vec3 dir = delta / dist;

    float normal_term = max(dot(normal, dir) - bias, 0.0);

    float range_falloff = 1.0 - smoothstep(0.0, radius + thickness, dist);
    float depth_falloff = 1.0 - smoothstep(thickness, max_depth_delta, depth_delta);

    return normal_term * range_falloff * depth_falloff;
}


vec2 get_sample_dir(int index) {
    if (index == 0) {
        return vec2(1.0, 0.0);
    }

    if (index == 1) {
        return vec2(-1.0, 0.0);
    }

    if (index == 2) {
        return vec2(0.0, 1.0);
    }

    if (index == 3) {
        return vec2(0.0, -1.0);
    }

    if (index == 4) {
        return vec2(0.707107, 0.707107);
    }

    if (index == 5) {
        return vec2(-0.707107, 0.707107);
    }

    if (index == 6) {
        return vec2(0.707107, -0.707107);
    }

    return vec2(-0.707107, -0.707107);
}

void main() {
    float depth = texture(u_depth, v_uv).r;

    if (depth >= 1.0) {
        FragColor = vec4(1.0);
        return;
    }

    vec3 world_pos = get_world_pos(v_uv, depth);
    vec3 normal = oct_to_float32x3(texture(u_normal, v_uv).xy);

    float radius = max(u_ao_params.x, 0.001);
    float intensity = max(u_ao_params.y, 0.0);
    float bias = max(u_ao_params.z, 0.0);
    float power = max(u_ao_params.w, 0.001);
    float thickness = max(u_ao_params2.x, 0.001);

    ivec2 depth_size = textureSize(u_depth, 0);
    vec2 texel_size = 1.0 / vec2(depth_size);

    float view_depth = get_view_depth(world_pos);

    float min_dim = float(min(depth_size.x, depth_size.y));
    float screen_radius = clamp(
            (radius / view_depth) * min_dim * 0.18,
            2.0,
            32.0
            );
    const int dir_count = 8;
    const int step_count = 4;

    float occlusion = 0.0;

    for (int dir_idx = 0; dir_idx < dir_count; ++dir_idx) {
        vec2 dir = get_sample_dir(dir_idx);
        float horizon = 0.0;

        for (int step_idx = 1; step_idx <= step_count; ++step_idx) {
            float step_scale = (float(step_idx) + 0.25) / float(step_count);

            vec2 sample_uv =
                v_uv + dir * texel_size * screen_radius * step_scale;

            if (sample_uv.x <= 0.0 || sample_uv.x >= 1.0 ||
                sample_uv.y <= 0.0 || sample_uv.y >= 1.0) {
                continue;
            }


            float sample_value = sample_occlusion(
                    world_pos,
                    normal,
                    view_depth,
                    sample_uv,
                    radius,
                    bias,
                    thickness
                    );


            horizon = max(horizon, sample_value);
        }

        occlusion += horizon;
    }

    occlusion /= float(dir_count);

    float ao = 1.0 - occlusion * intensity;
    ao = pow(clamp(ao, 0.0, 1.0), power);

    FragColor = vec4(vec3(ao), 1.0);
}
