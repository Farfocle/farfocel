/**
 * @file main.cpp
 * @author Tfoedy
 * WARNING: this is temporary code that will be removed. likely the wrost code you will see in this
 * project
 */

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <string>

#include "fr/asscooker/asscooker.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/time.hpp"
#include "fr/data/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/platform/input.hpp"
#include "fr/platform/keycode.hpp"
#include "fr/platform/window.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/renderer/renderer.hpp"
#include "fr/scene/camera_system.hpp"
#include "fr/scene/render_parts.hpp"
#include "fr/scene/render_system.hpp"

namespace fs = std::filesystem;

enum class EditorJob : U8 {
    None,
    CookModel,
    CookSkybox,
};

struct EditorState {
    bool running{true};
    bool wireframe{false};

    EditorJob job{EditorJob::None};
    std::future<bool> cook_future{};

    char gltf_path[512]{""};
    char hdr_path[512]{""};
    char status[512]{"Ready."};

    fr::String pending_model_path{};
    fr::String pending_skybox_path{};

    fr::Thing camera_entity{fr::Thing::nil()};
    fr::Thing model_entity{fr::Thing::nil()};
    fr::Thing sun_entity{fr::Thing::nil()};
    fr::Thing point_light_entity{fr::Thing::nil()};
    fr::Thing spot_light_entity{fr::Thing::nil()};

    fr::MeshAssetHandle model_handle{};
    fr::TextureAssetHandle skybox_handle{};

    fr::ShadingModel shading_model{fr::ShadingModel::PBR};
    fr::RenderDebugMode debug_mode{fr::RenderDebugMode::Final};

    F32 exposure{1.0f};
    F32 pbr_ambient_strength{0.03f};
    F32 standard_ambient_strength{0.035f};
    F32 standard_specular_default{0.25f};

    fr::DirectionalShadowSettings shadow_settings{};

    glm::vec3 sun_rotation_deg{-60.0f, 30.0f, 0.0f};
    glm::vec3 point_light_position{5.0f, 15.0f, 0.0f};

    glm::vec3 spot_light_position{0.0f, 5.0f, 5.0f};
    glm::vec3 spot_light_rotation_deg{-45.0f, 180.0f, 0.0f};

    bool enable_hbao{false};
    F32 hbao_radius{1.5f};
    F32 hbao_intensity{1.2f};
    F32 hbao_bias{0.05f};
    F32 hbao_power{1.5f};
    F32 hbao_thickness{1.0f};

    bool enable_ibl{true};
    F32 ibl_diffuse_strength{0.10f};
    F32 ibl_specular_strength{1.0f};
    F32 ibl_occlusion_strength{1.0f};
    F32 ibl_occlusion_power{2.0f};
    F32 ibl_sky_visibility_strength{0.75f};

    fr::RenderStats geometry_stats{};
    fr::RenderStats shadow_stats{};
};

static fr::RenderDevice *g_device{nullptr};

static fr::ShaderHandle g_gbuffer_shader{};
static fr::ShaderHandle g_lighting_shader{};
static fr::ShaderHandle g_shadow_shader{};
static fr::ShaderHandle g_point_shadow_shader{};
static fr::ShaderHandle g_spot_shadow_shader{};
static fr::ShaderHandle g_hbao_shader{};
static fr::ShaderHandle g_equirect_to_cube_shader{};
static fr::ShaderHandle g_irradiance_shader{};
static fr::ShaderHandle g_prefilter_env_shader{};
static fr::ShaderHandle g_brdf_lut_shader{};
static fr::ShaderHandle g_present_shader{};
static fr::ShaderHandle g_forward_transparent_shader{};

static fr::RenderPipelineHandle g_gbuffer_pipe{};
static fr::RenderPipelineHandle g_gbuffer_wire_pipe{};
static fr::RenderPipelineHandle g_lighting_pipe{};
static fr::RenderPipelineHandle g_shadow_pipe{};
static fr::RenderPipelineHandle g_point_shadow_pipe{};
static fr::RenderPipelineHandle g_spot_shadow_pipe{};
static fr::RenderPipelineHandle g_hbao_pipe{};
static fr::RenderPipelineHandle g_equirect_to_cube_pipe{};
static fr::RenderPipelineHandle g_irradiance_pipe{};
static fr::RenderPipelineHandle g_prefilter_env_pipe{};
static fr::RenderPipelineHandle g_brdf_lut_pipe{};
static fr::RenderPipelineHandle g_present_pipe{};
static fr::RenderPipelineHandle g_forward_transparent_pipe{};

static void imgui_event_callback(void *event_data, void *user_data) {
    (void)user_data;
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(event_data));
}

static void set_status(EditorState &state, const char *message) {
    std::snprintf(state.status, sizeof(state.status), "%s", message ? message : "");
}

static void set_status_path(EditorState &state, const char *prefix, const char *path) {
    std::snprintf(state.status, sizeof(state.status), "%s%s", prefix ? prefix : "",
                  path ? path : "");
}

static const char *job_name(EditorJob job) {
    switch (job) {
    case EditorJob::None:
        return "Idle";
    case EditorJob::CookModel:
        return "Cooking Model";
    case EditorJob::CookSkybox:
        return "Cooking Skybox";
    default:
        return "Unknown";
    }
}

static const char *debug_mode_name(fr::RenderDebugMode mode) {
    switch (mode) {
    case fr::RenderDebugMode::Final:
        return "Final";
    case fr::RenderDebugMode::Albedo:
        return "Albedo";
    case fr::RenderDebugMode::Normal:
        return "Normal";
    case fr::RenderDebugMode::MetallicSpecular:
        return "Metallic / Specular";
    case fr::RenderDebugMode::Roughness:
        return "Roughness";
    case fr::RenderDebugMode::AmbientOcclusion:
        return "Ambient Occlusion";
    case fr::RenderDebugMode::ShadingModel:
        return "Shading Model";
    case fr::RenderDebugMode::Shadow:
        return "Shadow";
    case fr::RenderDebugMode::Hbao:
        return "HBAO";
    default:
        return "Unknown";
    }
}

static fr::String cooked_path_from_source(const char *path, const char *extension) {
    fs::path fs_path(path);
    fs_path.replace_extension(extension);

    std::string native_path = fs_path.string();
    return fr::String::from_chars(native_path.c_str());
}

static fr::String read_text_file(fr::StringView path) {
    fr::String path_str = fr::String::from_view(path);

    std::ifstream file(path_str.data(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::fprintf(stderr, "[Editor] Failed to open file: %s\n", path_str.data());
        return fr::String{};
    }

    const std::streamoff file_size = file.tellg();
    if (file_size <= 0) {
        std::fprintf(stderr, "[Editor] Empty or invalid file: %s\n", path_str.data());
        return fr::String{};
    }

    file.seekg(0, std::ios::beg);

    fr::String content = fr::String::with_capacity(static_cast<USize>(file_size));
    content.grow_default(static_cast<USize>(file_size));

    if (!file.read(content.data(), file_size)) {
        std::fprintf(stderr, "[Editor] Failed to read file: %s\n", path_str.data());
        return fr::String{};
    }

    return content;
}

static fr::ShaderHandle create_shader_from_files(fr::RenderDevice *device, const char *vert_path,
                                                 const char *frag_path) {
    fr::String vert = read_text_file(fr::StringView(vert_path));
    fr::String frag = read_text_file(fr::StringView(frag_path));

    if (vert.size() == 0 || frag.size() == 0) {
        std::fprintf(stderr, "[Editor] Failed to read shader pair:\n  VS: %s\n  FS: %s\n",
                     vert_path, frag_path);
        return {};
    }

    fr::ShaderHandle shader = device->create_shader(vert.view(), frag.view());

    if (!shader.is_valid()) {
        std::fprintf(stderr, "[Editor] Failed to create shader:\n  VS: %s\n  FS: %s\n", vert_path,
                     frag_path);
    }

    return shader;
}

static fr::RenderPipelineHandle create_pipeline(fr::ShaderHandle shader, fr::CullMode cull_mode,
                                                bool depth_test, bool depth_write, bool wireframe,
                                                fr::BlendMode blend_mode = fr::BlendMode::None) {
    fr::RenderPipelineProperties props{};
    props.shader = shader;
    props.cull_mode = cull_mode;
    props.depth_test = depth_test;
    props.depth_write = depth_write;
    props.wireframe = wireframe;
    props.blend_mode = blend_mode;

    return g_device->create_render_pipeline(props);
}

static bool create_gpu_resources() {
    g_gbuffer_shader = create_shader_from_files(g_device, "engine/shaders/core/gbuffer.vert",
                                                "engine/shaders/core/gbuffer.frag");

    g_lighting_shader = create_shader_from_files(g_device, "engine/shaders/core/lighting.vert",
                                                 "engine/shaders/core/lighting.frag");

    g_shadow_shader = create_shader_from_files(g_device, "engine/shaders/core/shadow.vert",
                                               "engine/shaders/core/shadow.frag");

    g_point_shadow_shader = create_shader_from_files(
        g_device, "engine/shaders/core/point_shadow.vert", "engine/shaders/core/point_shadow.frag");

    g_spot_shadow_shader = create_shader_from_files(
        g_device, "engine/shaders/core/spot_shadow.vert", "engine/shaders/core/spot_shadow.frag");

    g_hbao_shader = create_shader_from_files(g_device, "engine/shaders/core/lighting.vert",
                                             "engine/shaders/core/hbao.frag");

    g_equirect_to_cube_shader = create_shader_from_files(
        g_device, "engine/shaders/core/lighting.vert", "engine/shaders/core/equirect_to_cube.frag");

    g_irradiance_shader =
        create_shader_from_files(g_device, "engine/shaders/core/lighting.vert",
                                 "engine/shaders/core/irradiance_convolution.frag");

    g_prefilter_env_shader = create_shader_from_files(g_device, "engine/shaders/core/lighting.vert",
                                                      "engine/shaders/core/prefilter_env.frag");

    g_brdf_lut_shader = create_shader_from_files(g_device, "engine/shaders/core/lighting.vert",
                                                 "engine/shaders/core/brdf_lut.frag");

    g_present_shader = create_shader_from_files(g_device, "engine/shaders/core/lighting.vert",
                                                "engine/shaders/core/present.frag");

    g_forward_transparent_shader =
        create_shader_from_files(g_device, "engine/shaders/core/forward_transparent.vert",
                                 "engine/shaders/core/forward_transparent.frag");

    if (!g_gbuffer_shader.is_valid() || !g_lighting_shader.is_valid() ||
        !g_shadow_shader.is_valid() || !g_point_shadow_shader.is_valid() ||
        !g_spot_shadow_shader.is_valid() || !g_hbao_shader.is_valid() ||
        !g_equirect_to_cube_shader.is_valid() || !g_irradiance_shader.is_valid() ||
        !g_prefilter_env_shader.is_valid() || !g_brdf_lut_shader.is_valid() ||
        !g_present_shader.is_valid() || !g_forward_transparent_shader.is_valid()) {
        return false;
    }

    g_gbuffer_pipe = create_pipeline(g_gbuffer_shader, fr::CullMode::Back, true, true, false);
    g_gbuffer_wire_pipe = create_pipeline(g_gbuffer_shader, fr::CullMode::None, true, true, true);

    g_lighting_pipe = create_pipeline(g_lighting_shader, fr::CullMode::None, false, false, false);

    g_shadow_pipe = create_pipeline(g_shadow_shader, fr::CullMode::None, true, true, false);

    g_point_shadow_pipe =
        create_pipeline(g_point_shadow_shader, fr::CullMode::None, true, true, false);

    g_spot_shadow_pipe =
        create_pipeline(g_spot_shadow_shader, fr::CullMode::None, true, true, false);

    g_hbao_pipe = create_pipeline(g_hbao_shader, fr::CullMode::None, false, false, false);

    g_equirect_to_cube_pipe =
        create_pipeline(g_equirect_to_cube_shader, fr::CullMode::None, false, false, false);

    g_irradiance_pipe =
        create_pipeline(g_irradiance_shader, fr::CullMode::None, false, false, false);

    g_prefilter_env_pipe =
        create_pipeline(g_prefilter_env_shader, fr::CullMode::None, false, false, false);

    g_brdf_lut_pipe = create_pipeline(g_brdf_lut_shader, fr::CullMode::None, false, false, false);

    g_present_pipe = create_pipeline(g_present_shader, fr::CullMode::None, false, false, false);

    g_forward_transparent_pipe = create_pipeline(g_forward_transparent_shader, fr::CullMode::None,
                                                 false, false, false, fr::BlendMode::Alpha);

    return g_gbuffer_pipe.is_valid() && g_gbuffer_wire_pipe.is_valid() &&
           g_lighting_pipe.is_valid() && g_shadow_pipe.is_valid() &&
           g_point_shadow_pipe.is_valid() && g_spot_shadow_pipe.is_valid() &&
           g_hbao_pipe.is_valid() && g_equirect_to_cube_pipe.is_valid() &&
           g_irradiance_pipe.is_valid() && g_prefilter_env_pipe.is_valid() &&
           g_brdf_lut_pipe.is_valid() && g_present_pipe.is_valid() &&
           g_forward_transparent_pipe.is_valid();
}

static void destroy_gpu_resources() {
    if (!g_device) {
        return;
    }

    if (g_forward_transparent_pipe.is_valid()) {
        g_device->destroy_pipeline(g_forward_transparent_pipe);
        g_forward_transparent_pipe = {};
    }

    if (g_present_pipe.is_valid()) {
        g_device->destroy_pipeline(g_present_pipe);
        g_present_pipe = {};
    }

    if (g_brdf_lut_pipe.is_valid()) {
        g_device->destroy_pipeline(g_brdf_lut_pipe);
        g_brdf_lut_pipe = {};
    }

    if (g_prefilter_env_pipe.is_valid()) {
        g_device->destroy_pipeline(g_prefilter_env_pipe);
        g_prefilter_env_pipe = {};
    }

    if (g_irradiance_pipe.is_valid()) {
        g_device->destroy_pipeline(g_irradiance_pipe);
        g_irradiance_pipe = {};
    }

    if (g_equirect_to_cube_pipe.is_valid()) {
        g_device->destroy_pipeline(g_equirect_to_cube_pipe);
        g_equirect_to_cube_pipe = {};
    }

    if (g_hbao_pipe.is_valid()) {
        g_device->destroy_pipeline(g_hbao_pipe);
        g_hbao_pipe = {};
    }

    if (g_spot_shadow_pipe.is_valid()) {
        g_device->destroy_pipeline(g_spot_shadow_pipe);
        g_spot_shadow_pipe = {};
    }

    if (g_point_shadow_pipe.is_valid()) {
        g_device->destroy_pipeline(g_point_shadow_pipe);
        g_point_shadow_pipe = {};
    }

    if (g_shadow_pipe.is_valid()) {
        g_device->destroy_pipeline(g_shadow_pipe);
        g_shadow_pipe = {};
    }

    if (g_lighting_pipe.is_valid()) {
        g_device->destroy_pipeline(g_lighting_pipe);
        g_lighting_pipe = {};
    }

    if (g_gbuffer_wire_pipe.is_valid()) {
        g_device->destroy_pipeline(g_gbuffer_wire_pipe);
        g_gbuffer_wire_pipe = {};
    }

    if (g_gbuffer_pipe.is_valid()) {
        g_device->destroy_pipeline(g_gbuffer_pipe);
        g_gbuffer_pipe = {};
    }

    if (g_forward_transparent_shader.is_valid()) {
        g_device->destroy_shader(g_forward_transparent_shader);
        g_forward_transparent_shader = {};
    }

    if (g_present_shader.is_valid()) {
        g_device->destroy_shader(g_present_shader);
        g_present_shader = {};
    }

    if (g_brdf_lut_shader.is_valid()) {
        g_device->destroy_shader(g_brdf_lut_shader);
        g_brdf_lut_shader = {};
    }

    if (g_prefilter_env_shader.is_valid()) {
        g_device->destroy_shader(g_prefilter_env_shader);
        g_prefilter_env_shader = {};
    }

    if (g_irradiance_shader.is_valid()) {
        g_device->destroy_shader(g_irradiance_shader);
        g_irradiance_shader = {};
    }

    if (g_equirect_to_cube_shader.is_valid()) {
        g_device->destroy_shader(g_equirect_to_cube_shader);
        g_equirect_to_cube_shader = {};
    }

    if (g_hbao_shader.is_valid()) {
        g_device->destroy_shader(g_hbao_shader);
        g_hbao_shader = {};
    }

    if (g_spot_shadow_shader.is_valid()) {
        g_device->destroy_shader(g_spot_shadow_shader);
        g_spot_shadow_shader = {};
    }

    if (g_point_shadow_shader.is_valid()) {
        g_device->destroy_shader(g_point_shadow_shader);
        g_point_shadow_shader = {};
    }

    if (g_shadow_shader.is_valid()) {
        g_device->destroy_shader(g_shadow_shader);
        g_shadow_shader = {};
    }

    if (g_lighting_shader.is_valid()) {
        g_device->destroy_shader(g_lighting_shader);
        g_lighting_shader = {};
    }

    if (g_gbuffer_shader.is_valid()) {
        g_device->destroy_shader(g_gbuffer_shader);
        g_gbuffer_shader = {};
    }
}

static void create_camera(fr::World &world, EditorState &state) {
    state.camera_entity = world.spawn();

    fr::WorldTransformPart &transform = world.emplace_now<fr::WorldTransformPart>(state.camera_entity);
    transform.position = glm::vec3(0.0f, 2.0f, 0.0f);
    transform.rotation = glm::quat(glm::radians(glm::vec3(0.0f, -90.0f, 0.0f)));
    transform.scale = glm::vec3(1.0f);

    fr::CameraPart &camera = world.emplace_now<fr::CameraPart>(state.camera_entity);
    camera.fov = 70.0f;
    camera.near_plane = 0.1f;
    camera.far_plane = 1000.0f;
    camera.is_main = true;

    fr::FPSControllerPart &fps = world.emplace_now<fr::FPSControllerPart>(state.camera_entity);
    fps.pitch = 0.0f;
    fps.yaw = -90.0f;
    fps.move_speed = 15.0f;
    fps.mouse_sensitivity = 0.1f;
}

static void create_lights(fr::World &world, EditorState &state) {
    state.sun_entity = world.spawn();

    fr::WorldTransformPart &sun_transform = world.emplace_now<fr::WorldTransformPart>(state.sun_entity);
    sun_transform.position = glm::vec3(0.0f);
    sun_transform.rotation = glm::quat(glm::radians(state.sun_rotation_deg));
    sun_transform.scale = glm::vec3(1.0f);

    fr::DirectionalLightPart &sun = world.emplace_now<fr::DirectionalLightPart>(state.sun_entity);
    sun.color = glm::vec3(1.0f, 0.95f, 0.9f);
    sun.intensity = 3.0f;

    state.point_light_entity = world.spawn();

    fr::WorldTransformPart &point_transform = world.emplace_now<fr::WorldTransformPart>(state.point_light_entity);
    point_transform.position = state.point_light_position;
    point_transform.rotation = glm::quat(glm::vec3(0.0f));
    point_transform.scale = glm::vec3(1.0f);

    fr::PointLightPart &point = world.emplace_now<fr::PointLightPart>(state.point_light_entity);
    point.color = glm::vec3(1.0f, 0.9f, 0.7f);
    point.intensity = 5.0f;
    point.radius = 50.0f;
    point.casts_shadow = false;
    point.shadow_strength = 1.0f;
    point.shadow_bias = 0.005f;

    state.spot_light_entity = world.spawn();

    fr::WorldTransformPart &spot_transform = world.emplace_now<fr::WorldTransformPart>(state.spot_light_entity);
    spot_transform.position = state.spot_light_position;
    spot_transform.rotation = glm::quat(glm::radians(state.spot_light_rotation_deg));
    spot_transform.scale = glm::vec3(1.0f);

    fr::SpotLightPart &spot = world.emplace_now<fr::SpotLightPart>(state.spot_light_entity);
    spot.color = glm::vec3(1.0f, 0.95f, 0.85f);
    spot.intensity = 0.0f;
    spot.radius = 30.0f;
    spot.inner_angle_deg = 20.0f;
    spot.outer_angle_deg = 35.0f;
    spot.casts_shadow = false;
    spot.shadow_strength = 1.0f;
    spot.shadow_bias = 0.002f;
}

static void unload_model(fr::World &world, fr::AssetManager &assets, EditorState &state) {
    if (!state.model_entity.is_nil()) {
        world.kill(state.model_entity);
        state.model_entity = fr::Thing::nil();
    }

    if (state.model_handle.is_valid()) {
        assets.unload_mesh(state.model_handle);
        state.model_handle = {};
    }
}

static void unload_skybox(fr::AssetManager &assets, EditorState &state) {
    if (state.skybox_handle.is_valid()) {
        assets.unload_texture(state.skybox_handle);
        state.skybox_handle = {};
    }
}

static void load_cooked_model(fr::World &world, fr::AssetManager &assets, EditorState &state,
                              fr::StringView path) {
    fr::MeshAssetHandle mesh = assets.load_mesh(path);

    if (!mesh.is_valid()) {
        set_status_path(state, "Failed to load model: ", fr::String::from_view(path).data());
        return;
    }

    unload_model(world, assets, state);

    state.model_handle = mesh;
    state.model_entity = world.spawn();

    fr::WorldTransformPart &transform = world.emplace_now<fr::WorldTransformPart>(state.model_entity);
    transform.position = glm::vec3(0.0f);
    transform.rotation = glm::quat(glm::vec3(0.0f));
    transform.scale = glm::vec3(1.0f);

    world.emplace_now<fr::MeshPart>(state.model_entity, state.model_handle);

    fr::MaterialPart &material = world.emplace_now<fr::MaterialPart>(state.model_entity);
    material.shading_model = state.shading_model;

    set_status_path(state, "Loaded model: ", fr::String::from_view(path).data());
}

static void load_cooked_skybox(fr::AssetManager &assets, EditorState &state, fr::StringView path) {
    fr::TextureAssetHandle texture = assets.load_texture(path);

    if (!texture.is_valid()) {
        set_status_path(state, "Failed to load skybox: ", fr::String::from_view(path).data());
        return;
    }

    unload_skybox(assets, state);

    state.skybox_handle = texture;
    set_status_path(state, "Loaded skybox: ", fr::String::from_view(path).data());
}

static void start_model_job(EditorState &state, fr::World &world, fr::AssetManager &assets) {
    if (state.job != EditorJob::None) {
        return;
    }

    state.pending_model_path = cooked_path_from_source(state.gltf_path, ".fmesh");

    if (fs::exists(state.pending_model_path.data())) {
        load_cooked_model(world, assets, state, state.pending_model_path.view());
        return;
    }

    fr::String input_path = fr::String::from_chars(state.gltf_path);
    fr::String output_path = state.pending_model_path;

    state.job = EditorJob::CookModel;
    set_status_path(state, "Cooking model: ", input_path.data());

    state.cook_future = std::async(std::launch::async, [input_path, output_path]() {
        return fr::asscooker::cook_mesh(input_path.view(), output_path.view());
    });
}

static void start_skybox_job(EditorState &state, fr::AssetManager &assets) {
    if (state.job != EditorJob::None) {
        return;
    }

    state.pending_skybox_path = cooked_path_from_source(state.hdr_path, ".ftex");

    if (fs::exists(state.pending_skybox_path.data())) {
        load_cooked_skybox(assets, state, state.pending_skybox_path.view());
        return;
    }

    fr::String input_path = fr::String::from_chars(state.hdr_path);
    fr::String output_path = state.pending_skybox_path;

    state.job = EditorJob::CookSkybox;
    set_status_path(state, "Cooking HDR: ", input_path.data());

    state.cook_future = std::async(std::launch::async, [input_path, output_path]() {
        return fr::asscooker::cook_texture(input_path.view(), output_path.view(), false);
    });
}

static void poll_jobs(EditorState &state, fr::World &world, fr::AssetManager &assets) {
    if (state.job == EditorJob::None || !state.cook_future.valid()) {
        return;
    }

    if (state.cook_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }

    const bool success = state.cook_future.get();

    if (!success) {
        set_status(state, "Cooking failed.");
        state.job = EditorJob::None;
        return;
    }

    if (state.job == EditorJob::CookModel) {
        load_cooked_model(world, assets, state, state.pending_model_path.view());
    } else if (state.job == EditorJob::CookSkybox) {
        load_cooked_skybox(assets, state, state.pending_skybox_path.view());
    }

    state.job = EditorJob::None;
}

static void shutdown_editor_state(fr::World &world, fr::AssetManager &assets, EditorState &state) {
    if (state.cook_future.valid()) {
        state.cook_future.wait();
        (void)state.cook_future.get();
    }

    unload_model(world, assets, state);
    unload_skybox(assets, state);
}

static void draw_stats(const char *label, const fr::RenderStats &stats) {
    ImGui::Text("%s Total: %u", label, stats.total_submeshes);
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s Visible: %u", label,
                       stats.visible_submeshes);
    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s Culled/Skipped: %u", label,
                       stats.culled_submeshes);

    if (stats.total_submeshes > 0) {
        const float percent = 100.0f * static_cast<float>(stats.culled_submeshes) /
                              static_cast<float>(stats.total_submeshes);
        ImGui::Text("Cull/Skip Ratio: %.2f%%", percent);
    }
}

static void draw_asset_panel(EditorState &state, fr::World &world, fr::AssetManager &assets) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("Asset Manager");

    ImGui::Text("State: %s", job_name(state.job));
    ImGui::TextWrapped("Status: %s", state.status);
    ImGui::Separator();

    const bool busy = state.job != EditorJob::None;
    ImGui::BeginDisabled(busy);

    ImGui::TextUnformatted("GLTF Model");
    ImGui::InputText("##GLTF", state.gltf_path, sizeof(state.gltf_path));

    if (ImGui::Button("Cook & Load GLTF", ImVec2(-1, 28))) {
        start_model_job(state, world, assets);
    }

    ImGui::Separator();

    ImGui::TextUnformatted("HDR Skybox");
    ImGui::InputText("##HDR", state.hdr_path, sizeof(state.hdr_path));

    if (ImGui::Button("Cook & Load HDR", ImVec2(-1, 28))) {
        start_skybox_job(state, assets);
    }

    ImGui::EndDisabled();
    ImGui::End();
}

static void draw_renderer_panel(EditorState &state, fr::World &world) {
    ImGui::SetNextWindowPos(ImVec2(10, 285), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 430), ImGuiCond_FirstUseEver);
    ImGui::Begin("Renderer Diagnostics");

    ImGui::Checkbox("Wireframe", &state.wireframe);

    if (ImGui::CollapsingHeader("Debug View", ImGuiTreeNodeFlags_DefaultOpen)) {
        fr::RenderDebugMode modes[] = {
            fr::RenderDebugMode::Final,        fr::RenderDebugMode::Albedo,
            fr::RenderDebugMode::Normal,       fr::RenderDebugMode::MetallicSpecular,
            fr::RenderDebugMode::Roughness,    fr::RenderDebugMode::AmbientOcclusion,
            fr::RenderDebugMode::ShadingModel, fr::RenderDebugMode::Shadow,
            fr::RenderDebugMode::Hbao,
        };

        if (ImGui::BeginCombo("Mode", debug_mode_name(state.debug_mode))) {
            for (fr::RenderDebugMode mode : modes) {
                const bool selected = state.debug_mode == mode;

                if (ImGui::Selectable(debug_mode_name(mode), selected)) {
                    state.debug_mode = mode;
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
    }

    if (ImGui::CollapsingHeader("Lighting Calibration", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Exposure", &state.exposure, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::DragFloat("PBR Ambient", &state.pbr_ambient_strength, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Standard Ambient", &state.standard_ambient_strength, 0.001f, 0.0f, 1.0f,
                         "%.3f");
        ImGui::DragFloat("Standard Specular", &state.standard_specular_default, 0.01f, 0.0f, 1.0f,
                         "%.2f");

        if (ImGui::Button("Reset Lighting Calibration", ImVec2(-1, 26))) {
            state.exposure = 1.0f;
            state.pbr_ambient_strength = 0.03f;
            state.standard_ambient_strength = 0.035f;
            state.standard_specular_default = 0.25f;
        }
    }

    if (ImGui::CollapsingHeader("HBAO", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable HBAO", &state.enable_hbao);

        ImGui::DragFloat("HBAO Radius", &state.hbao_radius, 0.05f, 0.05f, 10.0f, "%.2f");
        ImGui::DragFloat("HBAO Intensity", &state.hbao_intensity, 0.05f, 0.0f, 5.0f, "%.2f");
        ImGui::DragFloat("HBAO Bias", &state.hbao_bias, 0.005f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("HBAO Power", &state.hbao_power, 0.05f, 0.1f, 5.0f, "%.2f");
        ImGui::DragFloat("HBAO Thickness", &state.hbao_thickness, 0.05f, 0.01f, 5.0f, "%.2f");

        if (ImGui::Button("Reset HBAO", ImVec2(-1, 26))) {
            state.enable_hbao = false;
            state.hbao_radius = 1.5f;
            state.hbao_intensity = 1.2f;
            state.hbao_bias = 0.05f;
            state.hbao_power = 1.5f;
            state.hbao_thickness = 1.0f;
        }
    }

    if (ImGui::CollapsingHeader("IBL", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable IBL", &state.enable_ibl);

        ImGui::DragFloat("IBL Diffuse", &state.ibl_diffuse_strength, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::DragFloat("IBL Specular", &state.ibl_specular_strength, 0.01f, 0.0f, 2.0f, "%.2f");

        ImGui::Separator();
        ImGui::TextUnformatted("IBL Visibility");

        ImGui::SliderFloat("Occlusion Strength", &state.ibl_occlusion_strength, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("Occlusion Power", &state.ibl_occlusion_power, 0.05f, 0.1f, 8.0f, "%.2f");
        ImGui::SliderFloat("Sky Visibility", &state.ibl_sky_visibility_strength, 0.0f, 1.0f,
                           "%.2f");

        if (ImGui::Button("Reset IBL", ImVec2(-1, 26))) {
            state.enable_ibl = true;
            state.ibl_diffuse_strength = 0.10f;
            state.ibl_specular_strength = 1.0f;
            state.ibl_occlusion_strength = 1.0f;
            state.ibl_occlusion_power = 2.0f;
            state.ibl_sky_visibility_strength = 0.75f;
        }
    }

    if (ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_stats("Geometry", state.geometry_stats);
    }

    if (ImGui::CollapsingHeader("Shadow Casters", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_stats("Shadow", state.shadow_stats);
    }

    if (ImGui::CollapsingHeader("Material Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        const int mode = static_cast<int>(state.shading_model);

        if (ImGui::RadioButton("Unlit", mode == static_cast<int>(fr::ShadingModel::Unlit))) {
            state.shading_model = fr::ShadingModel::Unlit;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Standard", mode == static_cast<int>(fr::ShadingModel::Standard))) {
            state.shading_model = fr::ShadingModel::Standard;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("PBR", mode == static_cast<int>(fr::ShadingModel::PBR))) {
            state.shading_model = fr::ShadingModel::PBR;
        }

        fr::MaterialPart *material = world.try_get<fr::MaterialPart>(state.model_entity);
        if (material) {
            material->shading_model = state.shading_model;
        }
    }

    ImGui::End();
}

static void draw_directional_light_controls(EditorState &state, fr::World &world) {
    if (!ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    fr::DirectionalLightPart *light = world.try_get<fr::DirectionalLightPart>(state.sun_entity);
    fr::WorldTransformPart *transform = world.try_get<fr::WorldTransformPart>(state.sun_entity);

    if (!light || !transform) {
        return;
    }

    ImGui::ColorEdit3("Sun Color", &light->color.x);
    ImGui::DragFloat("Sun Intensity", &light->intensity, 0.05f, 0.0f, 50.0f);

    if (ImGui::DragFloat3("Sun Rotation", &state.sun_rotation_deg.x, 0.5f, -360.0f, 360.0f)) {
        transform->rotation = glm::quat(glm::radians(state.sun_rotation_deg));
    }

    ImGui::Separator();

    if (ImGui::TreeNodeEx("Cascaded Shadows", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Cascade Splits", &state.shadow_settings.cascade_splits.x, 0.5f, 1.0f,
                          1000.0f);
        ImGui::DragFloat3("Cascade Half Extents", &state.shadow_settings.cascade_half_extents.x,
                          0.5f, 1.0f, 1000.0f);
        ImGui::DragFloat3("Cascade Depth Ranges", &state.shadow_settings.cascade_depth_ranges.x,
                          0.5f, 1.0f, 2000.0f);

        ImGui::Separator();
        ImGui::TextUnformatted("Shadow Sampling");

        ImGui::DragFloat("Min Bias", &state.shadow_settings.min_bias, 0.00001f, 0.0f, 0.01f,
                         "%.6f");
        ImGui::DragFloat("Slope Bias", &state.shadow_settings.slope_bias, 0.00005f, 0.0f, 0.05f,
                         "%.6f");
        ImGui::DragFloat("Cascade Bias Scale", &state.shadow_settings.cascade_bias_scale, 0.05f,
                         0.0f, 10.0f, "%.3f");
        ImGui::SliderFloat("Shadow Strength", &state.shadow_settings.shadow_strength, 0.0f, 1.0f,
                           "%.2f");
        ImGui::DragFloat("Filter Radius", &state.shadow_settings.filter_radius_texels, 0.05f, 0.0f,
                         8.0f, "%.2f");
        ImGui::DragFloat("Cascade Filter Scale", &state.shadow_settings.cascade_filter_scale, 0.05f,
                         0.0f, 4.0f, "%.2f");

        if (ImGui::Button("Reset CSM Settings", ImVec2(-1, 26))) {
            state.shadow_settings = fr::DirectionalShadowSettings{};
        }

        ImGui::TreePop();
    }
}

static void draw_point_light_controls(EditorState &state, fr::World &world) {
    if (!ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    fr::PointLightPart *light = world.try_get<fr::PointLightPart>(state.point_light_entity);
    fr::WorldTransformPart *transform = world.try_get<fr::WorldTransformPart>(state.point_light_entity);

    if (!light || !transform) {
        return;
    }

    ImGui::ColorEdit3("Point Color", &light->color.x);
    ImGui::DragFloat("Point Intensity", &light->intensity, 0.1f, 0.0f, 10000.0f);
    ImGui::DragFloat("Point Radius", &light->radius, 0.1f, 0.0f, 1000.0f);

    if (ImGui::DragFloat3("Point Position", &state.point_light_position.x, 0.1f)) {
        transform->position = state.point_light_position;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Point Shadows");

    ImGui::Checkbox("Cast Shadow", &light->casts_shadow);
    ImGui::DragFloat("Point Shadow Bias", &light->shadow_bias, 0.0005f, 0.0f, 0.1f, "%.5f");
    ImGui::SliderFloat("Point Shadow Strength", &light->shadow_strength, 0.0f, 1.0f, "%.2f");
}

static void draw_spot_light_controls(EditorState &state, fr::World &world) {
    if (!ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    fr::SpotLightPart *light = world.try_get<fr::SpotLightPart>(state.spot_light_entity);
    fr::WorldTransformPart *transform = world.try_get<fr::WorldTransformPart>(state.spot_light_entity);

    if (!light || !transform) {
        return;
    }

    ImGui::ColorEdit3("Spot Color", &light->color.x);
    ImGui::DragFloat("Spot Intensity", &light->intensity, 0.1f, 0.0f, 10000.0f);
    ImGui::DragFloat("Spot Radius", &light->radius, 0.1f, 0.0f, 1000.0f);

    ImGui::DragFloat("Inner Angle", &light->inner_angle_deg, 0.25f, 0.1f, 89.0f);
    ImGui::DragFloat("Outer Angle", &light->outer_angle_deg, 0.25f, 0.1f, 89.5f);

    if (light->outer_angle_deg < light->inner_angle_deg + 0.1f) {
        light->outer_angle_deg = light->inner_angle_deg + 0.1f;
    }

    if (ImGui::DragFloat3("Spot Position", &state.spot_light_position.x, 0.1f)) {
        transform->position = state.spot_light_position;
    }

    if (ImGui::DragFloat3("Spot Rotation", &state.spot_light_rotation_deg.x, 0.5f, -360.0f,
                          360.0f)) {
        transform->rotation = glm::quat(glm::radians(state.spot_light_rotation_deg));
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Spot Shadows");

    ImGui::Checkbox("Spot Cast Shadow", &light->casts_shadow);
    ImGui::DragFloat("Spot Shadow Bias", &light->shadow_bias, 0.0005f, 0.0f, 0.1f, "%.5f");
    ImGui::SliderFloat("Spot Shadow Strength", &light->shadow_strength, 0.0f, 1.0f, "%.2f");
}

static void draw_lighting_panel(EditorState &state, fr::World &world) {
    ImGui::SetNextWindowPos(ImVec2(455, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lighting");

    draw_directional_light_controls(state, world);
    draw_point_light_controls(state, world);
    draw_spot_light_controls(state, world);

    ImGui::End();
}

static void draw_scene_panel(const EditorState &state) {
    ImGui::SetNextWindowPos(ImVec2(455, 550), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene");

    ImGui::Text("Model: %s", state.model_handle.is_valid() ? "Loaded" : "None");
    ImGui::Text("Skybox: %s", state.skybox_handle.is_valid() ? "Loaded" : "None");
    ImGui::TextWrapped("Hold RMB to look around. Use WASD + Space/LShift to move.");

    ImGui::End();
}

static void draw_ui(EditorState &state, fr::World &world, fr::AssetManager &assets) {
    draw_asset_panel(state, world, assets);
    draw_renderer_panel(state, world);
    draw_lighting_panel(state, world);
    draw_scene_panel(state);
}

static void update_camera(fr::Window &window, fr::World &world, fr::WindowInput &input,
                          EditorState &state, float dt) {
    ImGuiIO &io = ImGui::GetIO();
    SDL_Window *sdl_window = static_cast<SDL_Window *>(window.get_native_window());

    const bool mouse_look =
        state.job == EditorJob::None && ImGui::IsMouseDown(ImGuiMouseButton_Right);

    if (mouse_look) {
        SDL_SetWindowRelativeMouseMode(sdl_window, true);

        if (!io.WantCaptureKeyboard) {
            fr::CameraSystem::update_fps_cameras(world, input, dt);
        }

        return;
    }

    SDL_SetWindowRelativeMouseMode(sdl_window, false);
    input.mouse_delta_x = 0.0f;
    input.mouse_delta_y = 0.0f;

    if (state.job == EditorJob::None && !io.WantCaptureKeyboard && !io.WantCaptureMouse) {
        fr::CameraSystem::update_fps_cameras(world, input, dt);
    }
}

static fr::RenderFrameDesc build_frame_desc(EditorState &editor, fr::RenderQueue &geom_queue,
                                            fr::RenderQueue &shadow_queue, const fr::CamData &cam,
                                            U32 width, U32 height,
                                            fr::TextureHandle skybox_texture) {
    fr::RenderFrameDesc frame{};
    frame.geom_queue = &geom_queue;
    frame.shadow_queue = &shadow_queue;

    frame.viewport.width = width;
    frame.viewport.height = height;

    frame.camera.view_proj = cam.view_proj;
    frame.camera.position = cam.pos;
    frame.camera.forward = cam.dir;

    frame.pipelines.lighting = g_lighting_pipe;
    frame.pipelines.forward_transparent = g_forward_transparent_pipe;
    frame.pipelines.present = g_present_pipe;

    frame.pipelines.shadow = g_shadow_pipe;
    frame.pipelines.point_shadow = g_point_shadow_pipe;
    frame.pipelines.spot_shadow = g_spot_shadow_pipe;

    frame.pipelines.hbao = g_hbao_pipe;

    frame.pipelines.equirect_to_cube = g_equirect_to_cube_pipe;
    frame.pipelines.irradiance = g_irradiance_pipe;
    frame.pipelines.prefilter_env = g_prefilter_env_pipe;
    frame.pipelines.brdf_lut = g_brdf_lut_pipe;

    frame.skybox_map = skybox_texture;

    frame.debug.mode = editor.debug_mode;
    frame.debug.flags = 0;

    frame.lighting.exposure = editor.exposure;
    frame.lighting.pbr_ambient_strength = editor.pbr_ambient_strength;
    frame.lighting.standard_ambient_strength = editor.standard_ambient_strength;
    frame.lighting.standard_specular_default = editor.standard_specular_default;

    frame.ao.enabled = editor.enable_hbao;
    frame.ao.radius = editor.hbao_radius;
    frame.ao.intensity = editor.hbao_intensity;
    frame.ao.bias = editor.hbao_bias;
    frame.ao.power = editor.hbao_power;
    frame.ao.thickness = editor.hbao_thickness;

    frame.ibl.enabled = editor.enable_ibl;
    frame.ibl.diffuse_strength = editor.ibl_diffuse_strength;
    frame.ibl.specular_strength = editor.ibl_specular_strength;
    frame.ibl.occlusion_strength = editor.ibl_occlusion_strength;
    frame.ibl.occlusion_power = editor.ibl_occlusion_power;
    frame.ibl.sky_visibility_strength = editor.ibl_sky_visibility_strength;

    return frame;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    fr::init_core_ctx();

    fr::Alloc *alloc = fr::get_ambient_ctx().alloc;

    fr::Window window{};
    fr::WindowProperties props{};
    props.title = "Farfocel Engine Testbed";
    props.width = 1600;
    props.height = 900;
    props.vsync = true;
    props.fullscreen = false;
    props.api = fr::GRAPHICS_API::OPENGL;

    if (!window.init(props)) {
        fr::shutdown_core_ctx();
        return EXIT_FAILURE;
    }

    window.set_event_callback(imgui_event_callback, nullptr);

    g_device = fr::create_opengl_render_device(alloc);
    if (!g_device) {
        window.close();
        fr::shutdown_core_ctx();
        return EXIT_FAILURE;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(static_cast<SDL_Window *>(window.get_native_window()),
                                 static_cast<SDL_GLContext>(window.get_native_context()));
    ImGui_ImplOpenGL3_Init("#version 450 core");

    const bool gpu_ready = create_gpu_resources();
    FR_ASSERT(gpu_ready, "failed to create renderer testbed GPU resources");

    {
        fr::Renderer renderer(g_device);
        fr::AssetManager assets(g_device, alloc);
        fr::World world{};

        fr::RenderQueue geom_queue(alloc);
        fr::RenderQueue shadow_queue(alloc);

        fr::WindowInput input{};
        EditorState editor{};

        create_camera(world, editor);
        create_lights(world, editor);

        U64 last_time = fr::time::get_steady_now_ms();

        while (editor.running) {
            poll_jobs(editor, world, assets);

            U64 now = fr::time::get_steady_now_ms();
            float dt = static_cast<float>(now - last_time) / 1000.0f;
            last_time = now;

            if (!window.poll_events(input) || input.is_key_pressed(fr::Key::Escape)) {
                editor.running = false;
                break;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            update_camera(window, world, input, editor, dt);
            draw_ui(editor, world, assets);

            if (!window.is_minimized() && window.get_width() > 0 && window.get_height() > 0) {
                geom_queue.clear_leftover();
                shadow_queue.clear_leftover();

                const float aspect = static_cast<float>(window.get_width()) /
                                     static_cast<float>(window.get_height());

                fr::CamData cam = fr::RenderSystem::extract_cam_data(world, aspect);

                fr::RenderPipelineHandle active_gbuffer =
                    editor.wireframe ? g_gbuffer_wire_pipe : g_gbuffer_pipe;

                editor.geometry_stats = fr::RenderSystem::submit_meshes(
                    world, geom_queue, assets, active_gbuffer, cam.view_proj);

                editor.shadow_stats = fr::RenderSystem::submit_shadow_casters(
                    world, shadow_queue, assets, g_shadow_pipe);

                fr::RenderSystem::submit_lights(world, geom_queue);
                fr::RenderSystem::submit_directional_lights(world, geom_queue, cam.pos, cam.dir,
                                                            editor.shadow_settings);

                geom_queue.sort();
                shadow_queue.sort();

                fr::TextureHandle skybox_texture = assets.get_texture_handle(editor.skybox_handle);

                fr::RenderFrameDesc frame =
                    build_frame_desc(editor, geom_queue, shadow_queue, cam, window.get_width(),
                                     window.get_height(), skybox_texture);

                renderer.render(frame);
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            window.swap_buffers();
        }

        shutdown_editor_state(world, assets, editor);
    }

    destroy_gpu_resources();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    fr::destroy_opengl_render_device(g_device);
    g_device = nullptr;

    window.close();

    fr::shutdown_core_ctx();
    return EXIT_SUCCESS;
}
