/**
 * @file testbed.cpp
 * @brief Renderer testbed.
 */

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "fr/asscooker/asscooker.hpp"

#include "fr/asset/asset_manager.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/asset_storage.hpp"
#include "fr/asset/material_format.hpp"

#include "fr/core/ctx.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/thread_pool.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"

#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"

#include "fr/logger/logger.hpp"
#include "fr/logger/sinks/standard_sink.hpp"

#include "fr/platform/input.hpp"
#include "fr/platform/keycode.hpp"
#include "fr/platform/window.hpp"

#include "fr/renderer/default_renderer_setup.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_pipeline_cache.hpp"
#include "fr/renderer/renderer.hpp"

#include "fr/scene/render_extractor.hpp"
#include "fr/scene/render_parts.hpp"

namespace fs = std::filesystem;

namespace {

constexpr USize TESTBED_PATH_BUFFER_SIZE = 1024;

constexpr USize ASSET_THREAD_COUNT = 4;
constexpr USize ASYNC_UPLOAD_BUDGET_PER_FRAME = 4;

constexpr F32 CAMERA_PRECISION_SPEED_SCALE = 0.15f;

// todo input keycode
constexpr fr::MouseButton TESTBED_CAMERA_MOUSE_BUTTON = static_cast<fr::MouseButton>(3);

enum class TestbedShadingMode : U8 {
    Unlit,
    Standard,
    PBR,
};

struct DefaultShaderCookInput {
    fr::AssetId id{};
    fr::StringView vertex_path{};
    fr::StringView fragment_path{};
    fr::StringView output_path{};
};

struct TestbedRuntimeState {
    char gltf_path[TESTBED_PATH_BUFFER_SIZE]{};
    char hdr_path[TESTBED_PATH_BUFFER_SIZE]{};

    bool force_recook{false};
    bool wireframe{false};
    bool ao_enabled{false};
    bool ibl_enabled{true};

    bool camera_active{false};
    bool show_imgui_demo{false};
    bool show_debug_window{true};

    TestbedShadingMode shading_mode{TestbedShadingMode::PBR};

    F32 exposure{1.0f};
    F32 pbr_ambient_strength{0.03f};
    F32 standard_ambient_strength{0.035f};
    F32 standard_specular_default{0.25f};

    F32 ao_radius{1.5f};
    F32 ao_intensity{1.2f};
    F32 ao_bias{0.05f};
    F32 ao_power{1.5f};
    F32 ao_thickness{1.0f};

    F32 ibl_diffuse_strength{0.10f};
    F32 ibl_specular_strength{1.0f};
    F32 ibl_occlusion_strength{1.0f};
    F32 ibl_occlusion_power{2.0f};
    F32 ibl_sky_visibility_strength{0.75f};

    bool request_reload_model{false};
    bool request_reload_environment{false};
    bool request_reload_all{false};
};

struct TestbedSceneState {
    fr::Thing camera_entity{fr::Thing::nil()};
    fr::Thing model_entity{fr::Thing::nil()};

    fr::Thing directional_light_entity{fr::Thing::nil()};
    fr::Thing point_light_entity{fr::Thing::nil()};
    fr::Thing spot_light_entity{fr::Thing::nil()};

    fr::AssetId model_id{};
    fr::AssetId environment_id{};

    fr::TextureAssetHandle environment_handle{};

    bool model_loading{false};
    bool model_loaded{false};

    bool environment_loading{false};
    bool environment_loaded{false};
};

struct TestbedFrameStats {
    F32 dt{0.0f};
    F32 fps{0.0f};
    U32 width{0};
    U32 height{0};
};

[[nodiscard]] std::string to_std_string(fr::StringView value) {
    return std::string(value.data(), value.size());
}

[[nodiscard]] const char *asset_load_state_name(fr::AssetLoadState state) noexcept {
    switch (state) {
    case fr::AssetLoadState::Unloaded:
        return "unloaded";
    case fr::AssetLoadState::LoadingCpu:
        return "loading cpu";
    case fr::AssetLoadState::ReadyForGpu:
        return "ready for gpu";
    case fr::AssetLoadState::Loaded:
        return "loaded";
    case fr::AssetLoadState::Failed:
        return "failed";
    default:
        return "unknown";
    }
}

[[nodiscard]] const char *shading_mode_name(TestbedShadingMode mode) noexcept {
    switch (mode) {
    case TestbedShadingMode::Unlit:
        return "UNLIT";
    case TestbedShadingMode::Standard:
        return "STANDARD";
    case TestbedShadingMode::PBR:
        return "PBR";
    default:
        return "UNKNOWN";
    }
}

[[nodiscard]] fr::MaterialShadingModel material_shading_model(TestbedShadingMode mode) noexcept {
    switch (mode) {
    case TestbedShadingMode::Unlit:
        return fr::MaterialShadingModel::Unlit;
    case TestbedShadingMode::Standard:
        return fr::MaterialShadingModel::Standard;
    case TestbedShadingMode::PBR:
    default:
        return fr::MaterialShadingModel::PBR;
    }
}

void apply_shading_mode(fr::RenderFrameSubmission &submission, TestbedShadingMode mode) noexcept {
    const U32 shading_model = static_cast<U32>(material_shading_model(mode));

    for (USize i = 0; i < submission.materials.size(); ++i) {
        submission.materials[i].shading_model = shading_model;
    }
}

[[nodiscard]] fr::String path_buffer_to_string(fr::Alloc *alloc, const char *buffer) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (!buffer || buffer[0] == '\0') {
        return fr::String(alloc);
    }

    return fr::String::from_chars(alloc, buffer);
}

void setup_logger() {
    auto standard_sink = fr::make_unique<fr::StandardSink>(fr::StandardSink::Options{});
    fr::get_ambient_ctx().logger->add_sink(std::move(standard_sink));
}

void print_controls() {
    FR_LOG("[Testbed] Controls:");
    FR_LOG("  Hold Right Mouse    Enable FPS camera look/movement.");
    FR_LOG("  WASD                Move camera while Right Mouse is held.");
    FR_LOG("  Space               Move camera up while Right Mouse is held.");
    FR_LOG("  LShift              Precision movement while Right Mouse is held.");
    FR_LOG("  R                   Reload all current content.");
    FR_LOG("  Escape              Quit.");
}

bool register_cooked_outputs(fr::AssetRegistry &registry,
                             fr::Slice<const fr::asscooker::CookedAssetOutput> outputs) noexcept {
    bool ok = true;

    for (const fr::asscooker::CookedAssetOutput &output : outputs) {
        if (!output.id.is_valid() || output.kind == fr::AssetKind::Unknown ||
            output.path.size() == 0) {
            FR_LOG_ERR("[Testbed] Invalid cooked output record.");
            ok = false;
            continue;
        }

        ok = registry.register_loose_asset(output.id, output.kind, output.path.view(),
                                           output.content_hash) &&
             ok;

        FR_LOG("[Testbed] Registered cooked asset: id={}, kind={}, path={}", output.id.value,
               static_cast<U32>(output.kind), output.path.view());
    }

    return ok;
}

bool ensure_directory_for_path(fr::StringView path) noexcept {
    if (path.is_empty()) {
        return false;
    }

    fr::String normalized = fr::String::from_view(path);
    fr::file::normalize_unix(normalized);

    fr::StringView parent = fr::file::get_parent_path(normalized.view());
    if (parent.is_empty()) {
        return true;
    }

    std::error_code error{};
    fs::create_directories(to_std_string(parent), error);

    if (error) {
        FR_LOG_ERR("[Testbed] Failed to create directory '{}': {}", parent,
                   error.message().c_str());
        return false;
    }

    return true;
}

fr::String cooked_path_from_source(fr::Alloc *alloc, fr::StringView source_path,
                                   fr::StringView extension) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (source_path.is_empty() || extension.is_empty()) {
        return fr::String(alloc);
    }

    fr::String result = fr::String::from_view(alloc, source_path);
    fr::file::normalize_unix(result);

    fr::StringView filename = fr::file::get_filename(result.view());
    if (filename.is_empty()) {
        return fr::String(alloc);
    }

    const USize filename_offset = static_cast<USize>(filename.data() - result.data());

    USize dot_pos = fr::String::npos;

    for (USize i = filename.size(); i-- > 0;) {
        if (filename[i] == '.') {
            if (i != 0) {
                dot_pos = filename_offset + i;
            }

            break;
        }
    }

    if (dot_pos == fr::String::npos) {
        result.append(extension);
        return result;
    }

    result.shrink(dot_pos);
    result.append(extension);
    return result;
}

bool ensure_default_shader_output_dir() noexcept {
    std::error_code error{};
    fs::create_directories("assets/shaders/core", error);

    if (error) {
        FR_LOG_ERR("[Testbed] Failed to create shader output directory: {}",
                   error.message().c_str());
        return false;
    }

    return true;
}

bool cook_default_renderer_shaders(fr::Alloc *alloc, fr::AssetRegistry &registry) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (!ensure_default_shader_output_dir()) {
        return false;
    }

    fr::DynamicArray<fr::asscooker::CookedAssetOutput> outputs(alloc);

    const DefaultShaderCookInput shaders[] = {
        {FR_ASSET_ID("renderer.shader.gbuffer"), "engine/shaders/core/gbuffer.vert",
         "engine/shaders/core/gbuffer.frag", "assets/shaders/core/gbuffer.fshader"},
        {FR_ASSET_ID("renderer.shader.lighting"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/lighting.frag", "assets/shaders/core/lighting.fshader"},
        {FR_ASSET_ID("renderer.shader.shadow"), "engine/shaders/core/shadow.vert",
         "engine/shaders/core/shadow.frag", "assets/shaders/core/shadow.fshader"},
        {FR_ASSET_ID("renderer.shader.point_shadow"), "engine/shaders/core/point_shadow.vert",
         "engine/shaders/core/point_shadow.frag", "assets/shaders/core/point_shadow.fshader"},
        {FR_ASSET_ID("renderer.shader.spot_shadow"), "engine/shaders/core/spot_shadow.vert",
         "engine/shaders/core/spot_shadow.frag", "assets/shaders/core/spot_shadow.fshader"},
        {FR_ASSET_ID("renderer.shader.hbao"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/hbao.frag", "assets/shaders/core/hbao.fshader"},
        {FR_ASSET_ID("renderer.shader.equirect_to_cube"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/equirect_to_cube.frag",
         "assets/shaders/core/equirect_to_cube.fshader"},
        {FR_ASSET_ID("renderer.shader.irradiance"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/irradiance_convolution.frag",
         "assets/shaders/core/irradiance_convolution.fshader"},
        {FR_ASSET_ID("renderer.shader.prefilter_env"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/prefilter_env.frag", "assets/shaders/core/prefilter_env.fshader"},
        {FR_ASSET_ID("renderer.shader.brdf_lut"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/brdf_lut.frag", "assets/shaders/core/brdf_lut.fshader"},
        {FR_ASSET_ID("renderer.shader.present"), "engine/shaders/core/lighting.vert",
         "engine/shaders/core/present.frag", "assets/shaders/core/present.fshader"},
    };

    for (const DefaultShaderCookInput &shader : shaders) {
        fr::asscooker::CookOptions options{};
        options.output_id = shader.id;
        options.force = true;

        if (!fr::asscooker::cook_shader_ex(shader.vertex_path, shader.fragment_path,
                                           shader.output_path, &outputs, options)) {
            FR_LOG_ERR("[Testbed] Failed to cook default renderer shader: {}", shader.output_path);
            return false;
        }
    }

    if (!register_cooked_outputs(registry, outputs.slice())) {
        FR_LOG_ERR("[Testbed] Failed to register one or more cooked renderer shaders.");
        return false;
    }

    return true;
}

void update_transform_matrix(fr::WorldTransformPart &transform) noexcept {
    transform.matrix = glm::translate(glm::mat4(1.0f), transform.position) *
                       glm::mat4_cast(transform.rotation) *
                       glm::scale(glm::mat4(1.0f), transform.scale);
}

void apply_fps_controller_rotation(fr::FPSControllerPart &fps,
                                   fr::WorldTransformPart &transform) noexcept {
    fps.pitch = glm::clamp(fps.pitch, -89.0f, 89.0f);
    transform.rotation = glm::quat(glm::vec3(glm::radians(fps.pitch), glm::radians(fps.yaw), 0.0f));
    update_transform_matrix(transform);
}

void update_active_fps_cameras(fr::World &world, const fr::WindowInput &input, F32 dt,
                               bool precision_movement) noexcept {
    for (auto [thing, fps, trans] : world.query<fr::FPSControllerPart, fr::WorldTransformPart>()) {
        (void)thing;

        fps.yaw -= input.mouse_delta_x * fps.mouse_sensitivity;
        fps.pitch -= input.mouse_delta_y * fps.mouse_sensitivity;
        fps.pitch = glm::clamp(fps.pitch, -89.0f, 89.0f);

        trans.rotation = glm::quat(glm::vec3(glm::radians(fps.pitch), glm::radians(fps.yaw), 0.0f));

        glm::vec3 forward = trans.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 right = trans.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        F32 speed = fps.move_speed * dt;
        if (precision_movement) {
            speed *= CAMERA_PRECISION_SPEED_SCALE;
        }

        if (input.is_key_down(fr::Key::W)) {
            trans.position += forward * speed;
        }

        if (input.is_key_down(fr::Key::S)) {
            trans.position -= forward * speed;
        }

        if (input.is_key_down(fr::Key::A)) {
            trans.position -= right * speed;
        }

        if (input.is_key_down(fr::Key::D)) {
            trans.position += right * speed;
        }

        if (input.is_key_down(fr::Key::Space)) {
            trans.position += up * speed;
        }

        update_transform_matrix(trans);
    }
}

void sync_entity_transform_matrix(fr::World &world, fr::Thing entity) noexcept {
    if (entity.is_nil()) {
        return;
    }

    fr::WorldTransformPart *transform = world.try_get<fr::WorldTransformPart>(entity);
    if (!transform) {
        return;
    }

    update_transform_matrix(*transform);
}

void sync_transform_matrices(fr::World &world, const TestbedSceneState &state) noexcept {
    sync_entity_transform_matrix(world, state.camera_entity);
    sync_entity_transform_matrix(world, state.model_entity);
    sync_entity_transform_matrix(world, state.directional_light_entity);
    sync_entity_transform_matrix(world, state.point_light_entity);
    sync_entity_transform_matrix(world, state.spot_light_entity);
}

fr::Thing create_test_camera(fr::World &world) {
    fr::Thing camera_entity = world.spawn();

    fr::WorldTransformPart &transform = world.emplace_now<fr::WorldTransformPart>(camera_entity);
    transform.position = glm::vec3(0.0f, 2.0f, 6.0f);
    transform.scale = glm::vec3(1.0f);

    fr::FPSControllerPart &fps = world.emplace_now<fr::FPSControllerPart>(camera_entity);
    fps.pitch = -10.0f;
    fps.yaw = 180.0f;
    fps.move_speed = 15.0f;
    fps.mouse_sensitivity = 0.1f;

    apply_fps_controller_rotation(fps, transform);

    fr::CameraPart &camera = world.emplace_now<fr::CameraPart>(camera_entity);
    camera.fov = 70.0f;
    camera.near_plane = 0.1f;
    camera.far_plane = 1000.0f;
    camera.is_main = true;

    return camera_entity;
}

void create_test_lights(fr::World &world, TestbedSceneState &state) {
    {
        state.directional_light_entity = world.spawn();

        fr::WorldTransformPart &transform =
            world.emplace_now<fr::WorldTransformPart>(state.directional_light_entity);

        transform.position = glm::vec3(0.0f);
        transform.rotation = glm::quat(glm::radians(glm::vec3(-60.0f, 30.0f, 0.0f)));
        transform.scale = glm::vec3(1.0f);
        update_transform_matrix(transform);

        fr::DirectionalLightPart &sun =
            world.emplace_now<fr::DirectionalLightPart>(state.directional_light_entity);

        sun.color = glm::vec3(1.0f, 0.95f, 0.9f);
        sun.intensity = 3.0f;
    }

    {
        state.point_light_entity = world.spawn();

        fr::WorldTransformPart &transform =
            world.emplace_now<fr::WorldTransformPart>(state.point_light_entity);

        transform.position = glm::vec3(4.0f, 4.0f, 0.0f);
        transform.rotation = glm::quat(glm::vec3(0.0f));
        transform.scale = glm::vec3(1.0f);
        update_transform_matrix(transform);

        fr::PointLightPart &light = world.emplace_now<fr::PointLightPart>(state.point_light_entity);

        light.color = glm::vec3(1.0f, 0.85f, 0.65f);
        light.intensity = 10.0f;
        light.radius = 30.0f;
        light.casts_shadow = false;
    }

    {
        state.spot_light_entity = world.spawn();

        fr::WorldTransformPart &transform =
            world.emplace_now<fr::WorldTransformPart>(state.spot_light_entity);

        transform.position = glm::vec3(0.0f, 5.0f, 6.0f);
        transform.rotation = glm::quat(glm::radians(glm::vec3(-35.0f, 180.0f, 0.0f)));
        transform.scale = glm::vec3(1.0f);
        update_transform_matrix(transform);

        fr::SpotLightPart &light = world.emplace_now<fr::SpotLightPart>(state.spot_light_entity);
        light.color = glm::vec3(0.7f, 0.85f, 1.0f);
        light.intensity = 0.0f;
        light.radius = 25.0f;
        light.inner_angle_deg = 20.0f;
        light.outer_angle_deg = 35.0f;
        light.casts_shadow = false;
        light.shadow_strength = 1.0f;
        light.shadow_bias = 0.002f;
    }
}

void unload_model(fr::World &world, fr::AssetManager &assets, TestbedSceneState &state) noexcept {
    if (state.model_entity.is_nil()) {
        state.model_id = {};
        state.model_loading = false;
        state.model_loaded = false;
        return;
    }

    if (fr::MeshRendererPart *mesh = world.try_get<fr::MeshRendererPart>(state.model_entity)) {
        if (mesh->mesh_handle.is_valid()) {
            assets.unload_mesh(mesh->mesh_handle);
        }

        mesh->mesh_handle = {};
        mesh->resolved_mesh_id = {};
    }

    world.kill(state.model_entity);

    state.model_entity = fr::Thing::nil();
    state.model_id = {};
    state.model_loading = false;
    state.model_loaded = false;
}

void unload_environment(fr::AssetManager &assets, TestbedSceneState &state) noexcept {
    if (state.environment_handle.is_valid()) {
        assets.unload_texture(state.environment_handle);
        state.environment_handle = {};
    }

    state.environment_id = {};
    state.environment_loading = false;
    state.environment_loaded = false;
}

bool cook_and_request_environment(fr::Alloc *alloc, fr::AssetRegistry &registry,
                                  fr::AssetManager &assets, fr::ThreadPool &asset_pool,
                                  TestbedSceneState &state, fr::StringView source_path,
                                  bool force) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (source_path.is_empty()) {
        FR_LOG_WARN("[Testbed] Environment path is empty.");
        return true;
    }

    fr::String output_path = cooked_path_from_source(alloc, source_path, ".ftex");
    if (output_path.size() == 0) {
        FR_LOG_ERR("[Testbed] Failed to build environment cooked path.");
        return false;
    }

    if (!ensure_directory_for_path(output_path.view())) {
        return false;
    }

    fr::DynamicArray<fr::asscooker::CookedAssetOutput> outputs(alloc);

    fr::asscooker::CookOptions options{};
    options.force = force;

    FR_LOG("[Testbed] Cooking environment: {} -> {}", source_path, output_path.view());

    if (!fr::asscooker::cook_texture_ex(source_path, output_path.view(), false, &outputs,
                                        options)) {
        FR_LOG_ERR("[Testbed] Failed to cook environment texture: {}", source_path);
        return false;
    }

    if (!register_cooked_outputs(registry, outputs.slice())) {
        return false;
    }

    const fr::AssetId environment_id =
        fr::asscooker::resolve_output_asset_id(output_path.view(), options);

    unload_environment(assets, state);

    state.environment_id = environment_id;
    state.environment_loading = true;
    state.environment_loaded = false;

    if (!assets.request_texture(asset_pool, environment_id, true)) {
        FR_LOG_ERR("[Testbed] Failed to request async environment load: {}", output_path.view());
        state.environment_loading = false;
        return false;
    }

    FR_LOG("[Testbed] Environment async load requested: {}", output_path.view());
    return true;
}

bool cook_and_request_model(fr::Alloc *alloc, fr::AssetRegistry &registry, fr::AssetManager &assets,
                            fr::ThreadPool &asset_pool, fr::World &world, TestbedSceneState &state,
                            fr::StringView source_path, bool force) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (source_path.is_empty()) {
        FR_LOG_WARN("[Testbed] Model path is empty.");
        return true;
    }

    fr::String output_path = cooked_path_from_source(alloc, source_path, ".fmesh");
    if (output_path.size() == 0) {
        FR_LOG_ERR("[Testbed] Failed to build model cooked path.");
        return false;
    }

    if (!ensure_directory_for_path(output_path.view())) {
        return false;
    }

    fr::DynamicArray<fr::asscooker::CookedAssetOutput> outputs(alloc);

    fr::asscooker::CookOptions options{};
    options.force = force;

    FR_LOG("[Testbed] Cooking model: {} -> {}", source_path, output_path.view());

    if (!fr::asscooker::cook_mesh_ex(source_path, output_path.view(), &outputs, options)) {
        FR_LOG_ERR("[Testbed] Failed to cook model: {}", source_path);
        return false;
    }

    if (!register_cooked_outputs(registry, outputs.slice())) {
        return false;
    }

    const fr::AssetId mesh_id = fr::asscooker::resolve_output_asset_id(output_path.view(), options);

    unload_model(world, assets, state);

    state.model_entity = world.spawn();
    state.model_id = mesh_id;
    state.model_loading = true;
    state.model_loaded = false;

    fr::WorldTransformPart &transform =
        world.emplace_now<fr::WorldTransformPart>(state.model_entity);

    transform.position = glm::vec3(0.0f);
    transform.rotation = glm::quat(glm::vec3(0.0f));
    transform.scale = glm::vec3(1.0f);
    update_transform_matrix(transform);

    fr::MeshRendererPart &mesh = world.emplace_now<fr::MeshRendererPart>(state.model_entity);
    mesh.mesh_id = mesh_id;
    mesh.visible = true;
    mesh.casts_shadow = true;

    if (!assets.request_mesh(asset_pool, mesh_id, true)) {
        FR_LOG_ERR("[Testbed] Failed to request async model load: {}", output_path.view());
        state.model_loading = false;
        return false;
    }

    FR_LOG("[Testbed] Model async load requested: {}", output_path.view());
    return true;
}

void complete_async_asset_loads(fr::World &world, fr::AssetManager &assets,
                                TestbedSceneState &state) noexcept {
    if (state.environment_loading && state.environment_id.is_valid()) {
        fr::TextureAssetHandle handle = assets.try_get_texture(state.environment_id);
        if (handle.is_valid()) {
            state.environment_handle = handle;
            state.environment_loading = false;
            state.environment_loaded = true;

            FR_LOG_OK("[Testbed] Environment loaded asynchronously. id={}",
                      state.environment_id.value);
        } else if (assets.texture_state(state.environment_id) == fr::AssetLoadState::Failed) {
            FR_LOG_ERR("[Testbed] Async environment load failed. id={}",
                       state.environment_id.value);
            state.environment_loading = false;
            state.environment_loaded = false;
        }
    }

    if (state.model_loading && state.model_id.is_valid() && !state.model_entity.is_nil()) {
        fr::MeshAssetHandle handle = assets.try_get_mesh(state.model_id);
        if (handle.is_valid()) {
            fr::MeshRendererPart *mesh = world.try_get<fr::MeshRendererPart>(state.model_entity);
            if (mesh) {
                mesh->mesh_handle = handle;
                mesh->resolved_mesh_id = state.model_id;
            }

            state.model_loading = false;
            state.model_loaded = true;

            FR_LOG_OK("[Testbed] Model loaded asynchronously. id={}", state.model_id.value);
        } else if (assets.mesh_state(state.model_id) == fr::AssetLoadState::Failed) {
            FR_LOG_ERR("[Testbed] Async model load failed. id={}", state.model_id.value);
            state.model_loading = false;
            state.model_loaded = false;
        }
    }
}

void clamp_runtime_state(TestbedRuntimeState &state) noexcept {
    if (state.exposure < 0.05f) {
        state.exposure = 0.05f;
    }

    if (state.exposure > 16.0f) {
        state.exposure = 16.0f;
    }

    if (state.ibl_diffuse_strength < 0.0f) {
        state.ibl_diffuse_strength = 0.0f;
    }

    if (state.ibl_specular_strength < 0.0f) {
        state.ibl_specular_strength = 0.0f;
    }
}

void draw_transform_debug(fr::World &world, fr::Thing entity) {
    fr::WorldTransformPart *transform = world.try_get<fr::WorldTransformPart>(entity);
    if (!transform) {
        ImGui::TextDisabled("No transform.");
        return;
    }

    if (ImGui::DragFloat3("Position", glm::value_ptr(transform->position), 0.05f)) {
        update_transform_matrix(*transform);
    }

    glm::vec3 euler = glm::degrees(glm::eulerAngles(transform->rotation));
    if (ImGui::DragFloat3("Rotation", glm::value_ptr(euler), 0.25f)) {
        transform->rotation = glm::quat(glm::radians(euler));
        update_transform_matrix(*transform);
    }

    if (ImGui::DragFloat3("Scale", glm::value_ptr(transform->scale), 0.01f, 0.001f, 1000.0f)) {
        update_transform_matrix(*transform);
    }
}

void draw_camera_debug(fr::World &world, TestbedSceneState &state) {
    if (state.camera_entity.is_nil()) {
        ImGui::TextDisabled("No camera entity.");
        return;
    }

    if (ImGui::TreeNode("Camera Transform")) {
        draw_transform_debug(world, state.camera_entity);
        ImGui::TreePop();
    }

    fr::CameraPart *camera = world.try_get<fr::CameraPart>(state.camera_entity);
    if (camera && ImGui::TreeNode("Camera Lens")) {
        ImGui::DragFloat("FOV", &camera->fov, 0.1f, 1.0f, 160.0f);
        ImGui::DragFloat("Near", &camera->near_plane, 0.01f, 0.001f, 100.0f);
        ImGui::DragFloat("Far", &camera->far_plane, 1.0f, 1.0f, 100000.0f);
        ImGui::Checkbox("Main", &camera->is_main);
        ImGui::TreePop();
    }

    fr::FPSControllerPart *fps = world.try_get<fr::FPSControllerPart>(state.camera_entity);
    fr::WorldTransformPart *transform = world.try_get<fr::WorldTransformPart>(state.camera_entity);

    if (fps && transform && ImGui::TreeNode("FPS Controller")) {
        bool changed = false;
        changed = ImGui::DragFloat("Pitch", &fps->pitch, 0.1f, -89.0f, 89.0f) || changed;
        changed = ImGui::DragFloat("Yaw", &fps->yaw, 0.1f, -360.0f, 360.0f) || changed;
        ImGui::DragFloat("Move speed", &fps->move_speed, 0.1f, 0.1f, 500.0f);
        ImGui::DragFloat("Mouse sensitivity", &fps->mouse_sensitivity, 0.005f, 0.001f, 5.0f);

        if (changed) {
            apply_fps_controller_rotation(*fps, *transform);
        }

        ImGui::TreePop();
    }
}

void draw_light_debug(fr::World &world, TestbedSceneState &state) {
    if (ImGui::TreeNode("Directional Light")) {
        draw_transform_debug(world, state.directional_light_entity);

        fr::DirectionalLightPart *light =
            world.try_get<fr::DirectionalLightPart>(state.directional_light_entity);
        if (light) {
            ImGui::ColorEdit3("Color", glm::value_ptr(light->color));
            ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 100.0f);
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Point Light")) {
        draw_transform_debug(world, state.point_light_entity);

        fr::PointLightPart *light = world.try_get<fr::PointLightPart>(state.point_light_entity);
        if (light) {
            ImGui::ColorEdit3("Color", glm::value_ptr(light->color));
            ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 1000.0f);
            ImGui::DragFloat("Radius", &light->radius, 0.1f, 0.0f, 1000.0f);
            ImGui::Checkbox("Casts shadow", &light->casts_shadow);
            ImGui::DragFloat("Shadow strength", &light->shadow_strength, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Shadow bias", &light->shadow_bias, 0.0001f, 0.0f, 1.0f, "%.5f");
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Spot Light")) {
        draw_transform_debug(world, state.spot_light_entity);

        fr::SpotLightPart *light = world.try_get<fr::SpotLightPart>(state.spot_light_entity);
        if (light) {
            ImGui::ColorEdit3("Color", glm::value_ptr(light->color));
            ImGui::DragFloat("Intensity", &light->intensity, 0.05f, 0.0f, 1000.0f);
            ImGui::DragFloat("Radius", &light->radius, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("Inner angle", &light->inner_angle_deg, 0.1f, 0.0f, 179.0f);
            ImGui::DragFloat("Outer angle", &light->outer_angle_deg, 0.1f, 0.0f, 179.0f);

            if (light->outer_angle_deg < light->inner_angle_deg) {
                light->outer_angle_deg = light->inner_angle_deg;
            }

            ImGui::Checkbox("Casts shadow", &light->casts_shadow);
            ImGui::DragFloat("Shadow strength", &light->shadow_strength, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat("Shadow bias", &light->shadow_bias, 0.0001f, 0.0f, 1.0f, "%.5f");
        }

        ImGui::TreePop();
    }
}

void draw_shading_mode_selector(TestbedRuntimeState &runtime) {
    const char *items[] = {"UNLIT", "STANDARD", "PBR"};
    S32 selected = static_cast<S32>(runtime.shading_mode);

    if (ImGui::Combo("Material shading", &selected, items, 3)) {
        if (selected < 0) {
            selected = 0;
        }

        if (selected > 2) {
            selected = 2;
        }

        runtime.shading_mode = static_cast<TestbedShadingMode>(selected);
    }

    ImGui::TextDisabled("Overrides material shading model in the extracted frame.");
}

void draw_renderer_debug(TestbedRuntimeState &runtime) {
    ImGui::Checkbox("Wireframe", &runtime.wireframe);

    ImGui::SeparatorText("Material Shading");
    draw_shading_mode_selector(runtime);

    ImGui::SeparatorText("Lighting");
    ImGui::DragFloat("Exposure", &runtime.exposure, 0.01f, 0.05f, 16.0f);
    ImGui::DragFloat("PBR ambient", &runtime.pbr_ambient_strength, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Standard ambient", &runtime.standard_ambient_strength, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Standard specular", &runtime.standard_specular_default, 0.001f, 0.0f, 10.0f);

    ImGui::SeparatorText("AO");
    ImGui::Checkbox("AO enabled", &runtime.ao_enabled);
    ImGui::DragFloat("AO radius", &runtime.ao_radius, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("AO intensity", &runtime.ao_intensity, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("AO bias", &runtime.ao_bias, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("AO power", &runtime.ao_power, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("AO thickness", &runtime.ao_thickness, 0.01f, 0.0f, 100.0f);

    ImGui::SeparatorText("IBL");
    ImGui::Checkbox("IBL enabled", &runtime.ibl_enabled);
    ImGui::DragFloat("IBL diffuse", &runtime.ibl_diffuse_strength, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("IBL specular", &runtime.ibl_specular_strength, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("IBL occlusion strength", &runtime.ibl_occlusion_strength, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("IBL occlusion power", &runtime.ibl_occlusion_power, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Sky visibility", &runtime.ibl_sky_visibility_strength, 0.01f, 0.0f, 10.0f);

    clamp_runtime_state(runtime);
}

void draw_asset_debug(fr::AssetManager &assets, TestbedRuntimeState &runtime,
                      const TestbedSceneState &scene_state) {
    ImGui::InputText("glTF path", runtime.gltf_path, TESTBED_PATH_BUFFER_SIZE);
    ImGui::InputText("HDR path", runtime.hdr_path, TESTBED_PATH_BUFFER_SIZE);
    ImGui::Checkbox("Force recook", &runtime.force_recook);

    if (ImGui::Button("Cook/Load glTF")) {
        runtime.request_reload_model = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Cook/Load HDR")) {
        runtime.request_reload_environment = true;
    }

    if (ImGui::Button("Reload all")) {
        runtime.request_reload_all = true;
    }

    ImGui::SeparatorText("Loaded assets");
    ImGui::Text("Model loaded: %s", scene_state.model_loaded ? "yes" : "no");
    ImGui::Text("Model loading: %s", scene_state.model_loading ? "yes" : "no");
    ImGui::Text("Model id: %llu", static_cast<unsigned long long>(scene_state.model_id.value));

    if (scene_state.model_id.is_valid()) {
        ImGui::Text("Model state: %s",
                    asset_load_state_name(assets.mesh_state(scene_state.model_id)));
    }

    ImGui::Text("Environment loaded: %s", scene_state.environment_loaded ? "yes" : "no");
    ImGui::Text("Environment loading: %s", scene_state.environment_loading ? "yes" : "no");
    ImGui::Text("Environment id: %llu",
                static_cast<unsigned long long>(scene_state.environment_id.value));

    if (scene_state.environment_id.is_valid()) {
        ImGui::Text("Environment state: %s",
                    asset_load_state_name(assets.texture_state(scene_state.environment_id)));
    }
}

void draw_debug_ui(fr::Window &window, fr::AssetManager &assets, fr::World &world,
                   TestbedRuntimeState &runtime, TestbedSceneState &scene_state,
                   const TestbedFrameStats &stats) {
    if (runtime.show_imgui_demo) {
        ImGui::ShowDemoWindow(&runtime.show_imgui_demo);
    }

    if (!runtime.show_debug_window) {
        return;
    }

    ImGui::Begin("Farfocel Testbed", &runtime.show_debug_window);

    ImGui::Text("FPS: %.1f", stats.fps);
    ImGui::Text("Frame: %.3f ms", stats.dt * 1000.0f);
    ImGui::Text("Viewport: %ux%u", stats.width, stats.height);
    ImGui::Text("Focused: %s", window.is_focused() ? "yes" : "no");
    ImGui::Text("Mouse mode: %u", static_cast<U32>(window.get_mouse_mode()));
    ImGui::Text("Camera active: %s", runtime.camera_active ? "yes" : "no");
    ImGui::Text("Shading: %s", shading_mode_name(runtime.shading_mode));
    ImGui::TextDisabled("Hold Right Mouse to control camera. Hold LShift for precision movement.");

    ImGui::Checkbox("Show ImGui demo", &runtime.show_imgui_demo);

    if (ImGui::CollapsingHeader("Assets", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_asset_debug(assets, runtime, scene_state);
    }

    if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_renderer_debug(runtime);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_camera_debug(world, scene_state);
    }

    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_light_debug(world, scene_state);
    }

    ImGui::End();
}

bool process_runtime_asset_requests(fr::Alloc *alloc, fr::AssetRegistry &registry,
                                    fr::AssetManager &assets, fr::ThreadPool &asset_pool,
                                    fr::World &world, TestbedRuntimeState &runtime,
                                    TestbedSceneState &scene_state) noexcept {
    if (!runtime.request_reload_model && !runtime.request_reload_environment &&
        !runtime.request_reload_all) {
        return true;
    }

    const bool reload_all = runtime.request_reload_all;
    const bool reload_model = runtime.request_reload_model || reload_all;
    const bool reload_environment = runtime.request_reload_environment || reload_all;

    runtime.request_reload_model = false;
    runtime.request_reload_environment = false;
    runtime.request_reload_all = false;

    bool ok = true;

    if (reload_environment) {
        fr::String hdr_path = path_buffer_to_string(alloc, runtime.hdr_path);
        ok = cook_and_request_environment(alloc, registry, assets, asset_pool, scene_state,
                                          hdr_path.view(), runtime.force_recook) &&
             ok;
    }

    if (reload_model) {
        fr::String gltf_path = path_buffer_to_string(alloc, runtime.gltf_path);
        ok = cook_and_request_model(alloc, registry, assets, asset_pool, world, scene_state,
                                    gltf_path.view(), runtime.force_recook) &&
             ok;
    }

    sync_transform_matrices(world, scene_state);
    return ok;
}

fr::RenderFrameDesc build_frame_desc(const fr::RenderExtractResult &extract_result,
                                     const fr::RenderFrameSubmission &submission,
                                     fr::TextureHandle environment, U32 width, U32 height,
                                     const TestbedRuntimeState &runtime) noexcept {
    fr::RenderFrameDesc frame{};
    frame.submission = &submission;

    frame.viewport.width = width;
    frame.viewport.height = height;

    frame.camera = extract_result.camera;
    frame.environment_source = environment;

    frame.lighting.exposure = runtime.exposure;
    frame.lighting.pbr_ambient_strength = runtime.pbr_ambient_strength;
    frame.lighting.standard_ambient_strength = runtime.standard_ambient_strength;
    frame.lighting.standard_specular_default = runtime.standard_specular_default;

    frame.ao.enabled = runtime.ao_enabled;
    frame.ao.radius = runtime.ao_radius;
    frame.ao.intensity = runtime.ao_intensity;
    frame.ao.bias = runtime.ao_bias;
    frame.ao.power = runtime.ao_power;
    frame.ao.thickness = runtime.ao_thickness;

    frame.ibl.enabled = runtime.ibl_enabled;
    frame.ibl.diffuse_strength = runtime.ibl_diffuse_strength;
    frame.ibl.specular_strength = runtime.ibl_specular_strength;
    frame.ibl.occlusion_strength = runtime.ibl_occlusion_strength;
    frame.ibl.occlusion_power = runtime.ibl_occlusion_power;
    frame.ibl.sky_visibility_strength = runtime.ibl_sky_visibility_strength;

    frame.debug.mode = fr::RenderDebugMode::Final;
    frame.debug.flags = 0;

    return frame;
}

void imgui_event_callback(void *event_data, void *) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event *>(event_data));
}

bool init_imgui(fr::Window &window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    SDL_Window *sdl_window = static_cast<SDL_Window *>(window.get_native_window());
    SDL_GLContext gl_context = static_cast<SDL_GLContext>(window.get_native_context());

    if (!ImGui_ImplSDL3_InitForOpenGL(sdl_window, gl_context)) {
        FR_LOG_ERR("[Testbed] Failed to initialize ImGui SDL3 backend.");
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450")) {
        FR_LOG_ERR("[Testbed] Failed to initialize ImGui OpenGL backend.");
        return false;
    }

    window.set_event_callback(imgui_event_callback, nullptr);
    return true;
}

void shutdown_imgui(fr::Window &window) {
    window.set_event_callback(nullptr, nullptr);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

} // namespace

S32 main(S32 argc, char **argv) {
    (void)argc;
    (void)argv;

    fr::init_core_ctx();
    setup_logger();

    S32 exit_code = EXIT_SUCCESS;

    {
        fr::Alloc *alloc = fr::get_ambient_ctx().alloc;
        FR_ASSERT(alloc, "ambient allocator must be non-null");

        TestbedRuntimeState runtime{};

        FR_LOG("[Testbed] Starting Farfocel renderer testbed.");

        fr::Window window{};
        fr::RenderDevice *device = nullptr;
        bool imgui_initialized = false;

        fr::WindowProperties props{};
        props.title = "Farfocel Renderer Testbed";
        props.width = 1280;
        props.height = 720;
        props.vsync = true;
        props.fullscreen = false;
        props.api = fr::GRAPHICS_API::OPENGL;

        if (!window.init(props)) {
            FR_LOG_ERR("[Testbed] Failed to initialize window.");
            exit_code = EXIT_FAILURE;
        }

        if (exit_code == EXIT_SUCCESS) {
            device = fr::create_opengl_render_device(alloc);
            if (!device) {
                FR_LOG_ERR("[Testbed] Failed to create OpenGL render device.");
                exit_code = EXIT_FAILURE;
            }
        }

        if (exit_code == EXIT_SUCCESS) {
            imgui_initialized = init_imgui(window);
            if (!imgui_initialized) {
                exit_code = EXIT_FAILURE;
            }
        }

        if (exit_code == EXIT_SUCCESS) {
            fr::AssetRegistry registry(alloc);
            fr::AssetStorage storage(alloc);

            if (!cook_default_renderer_shaders(alloc, registry)) {
                FR_LOG_ERR("[Testbed] Failed to prepare default renderer shaders.");
                exit_code = EXIT_FAILURE;
            }

            if (exit_code == EXIT_SUCCESS) {
                fr::AssetManager assets(device, alloc, &registry, &storage);
                fr::ThreadPool asset_pool(alloc, ASSET_THREAD_COUNT);

                fr::DefaultRendererShaderIds shader_ids{};
                fr::DefaultRendererShaders shaders{};
                bool shaders_loaded = false;

                if (!fr::load_default_renderer_shaders(assets, shader_ids, shaders)) {
                    FR_LOG_ERR("[Testbed] Failed to load default renderer shaders.");
                    exit_code = EXIT_FAILURE;
                } else {
                    shaders_loaded = true;
                }

                if (exit_code == EXIT_SUCCESS) {
                    fr::RenderPipelineCache pipeline_cache(device, &assets, alloc);

                    fr::RendererPipelineSet pipelines{};
                    if (!fr::create_default_renderer_pipelines(pipeline_cache, shaders,
                                                               pipelines)) {
                        FR_LOG_ERR("[Testbed] Failed to create default renderer pipelines.");
                        exit_code = EXIT_FAILURE;
                    }

                    if (exit_code == EXIT_SUCCESS) {
                        fr::RendererCreateDesc renderer_desc{};
                        renderer_desc.alloc = alloc;
                        renderer_desc.pipelines = pipelines;

                        fr::Renderer renderer(device, renderer_desc);
                        if (!renderer.is_ready()) {
                            FR_LOG_ERR("[Testbed] Renderer failed to initialize.");
                            exit_code = EXIT_FAILURE;
                        }

                        if (exit_code == EXIT_SUCCESS) {
                            fr::World world{};

                            TestbedSceneState scene_state{};
                            scene_state.camera_entity = create_test_camera(world);
                            create_test_lights(world, scene_state);

                            sync_transform_matrices(world, scene_state);

                            fr::RenderFrameSubmission submission(alloc);
                            fr::WindowInput input{};

                            window.set_mouse_mode(fr::MouseMode::Normal);
                            print_controls();

                            using Clock = std::chrono::steady_clock;
                            Clock::time_point last_frame_time = Clock::now();

                            bool running = true;

                            while (running) {
                                const Clock::time_point now = Clock::now();
                                const std::chrono::duration<F32> frame_delta =
                                    now - last_frame_time;
                                last_frame_time = now;

                                F32 dt = frame_delta.count();
                                if (dt > 0.1f) {
                                    dt = 0.1f;
                                }

                                if (!window.poll_events(input) ||
                                    input.is_key_pressed(fr::Key::Escape)) {
                                    running = false;
                                    break;
                                }

                                ImGui_ImplOpenGL3_NewFrame();
                                ImGui_ImplSDL3_NewFrame();
                                ImGui::NewFrame();

                                TestbedFrameStats stats{};
                                stats.dt = dt;
                                stats.fps = dt > 0.0f ? 1.0f / dt : 0.0f;
                                stats.width = window.get_width();
                                stats.height = window.get_height();

                                draw_debug_ui(window, assets, world, runtime, scene_state, stats);

                                ImGuiIO &io = ImGui::GetIO();

                                if (input.is_key_pressed(fr::Key::R)) {
                                    runtime.request_reload_all = true;
                                }

                                if (!process_runtime_asset_requests(alloc, registry, assets,
                                                                    asset_pool, world, runtime,
                                                                    scene_state)) {
                                    FR_LOG_ERR("[Testbed] Runtime asset request failed.");
                                }

                                assets.process_async_uploads(ASYNC_UPLOAD_BUDGET_PER_FRAME);
                                complete_async_asset_loads(world, assets, scene_state);

                                const bool right_mouse_down =
                                    input.is_mouse_down(TESTBED_CAMERA_MOUSE_BUTTON);

                                const bool camera_should_be_active =
                                    window.is_focused() && right_mouse_down && !io.WantCaptureMouse;

                                if (camera_should_be_active != runtime.camera_active) {
                                    runtime.camera_active = camera_should_be_active;
                                    window.set_mouse_mode(runtime.camera_active
                                                              ? fr::MouseMode::Relative
                                                              : fr::MouseMode::Normal);
                                }

                                if (runtime.camera_active) {
                                    const bool precision = input.is_key_down(fr::Key::LShift);
                                    update_active_fps_cameras(world, input, dt, precision);
                                }

                                if (!window.is_minimized() && window.get_width() > 0 &&
                                    window.get_height() > 0) {
                                    const U32 width = window.get_width();
                                    const U32 height = window.get_height();

                                    const F32 aspect =
                                        static_cast<F32>(width) / static_cast<F32>(height);

                                    fr::RenderExtractDesc extract_desc{};
                                    extract_desc.aspect_ratio = aspect;
                                    extract_desc.geometry_pipeline =
                                        renderer.geometry_pipeline(runtime.wireframe);
                                    extract_desc.shadow_pipeline = renderer.shadow_pipeline();

                                    fr::RenderExtractResult extract_result =
                                        fr::extract_render_frame(world, assets, extract_desc,
                                                                 submission);

                                    apply_shading_mode(submission, runtime.shading_mode);

                                    const fr::TextureHandle environment =
                                        assets.get_texture_handle(scene_state.environment_handle);

                                    fr::RenderFrameDesc frame =
                                        build_frame_desc(extract_result, submission, environment,
                                                         width, height, runtime);

                                    renderer.render(frame);
                                }

                                ImGui::Render();
                                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                                window.swap_buffers();
                            }

                            window.set_mouse_mode(fr::MouseMode::Normal);

                            asset_pool.wait();
                            assets.process_async_uploads(static_cast<USize>(-1));
                            complete_async_asset_loads(world, assets, scene_state);

                            for (auto [thing, mesh] : world.query<fr::MeshRendererPart>()) {
                                (void)thing;

                                if (mesh.mesh_handle.is_valid()) {
                                    assets.unload_mesh(mesh.mesh_handle);
                                    mesh.mesh_handle = {};
                                    mesh.resolved_mesh_id = {};
                                }
                            }

                            unload_environment(assets, scene_state);
                        }
                    }
                }

                asset_pool.wait();
                assets.process_async_uploads(static_cast<USize>(-1));

                if (shaders_loaded) {
                    fr::unload_default_renderer_shaders(assets, shaders);
                }
            }
        }

        if (imgui_initialized) {
            shutdown_imgui(window);
        }

        if (device) {
            fr::destroy_opengl_render_device(device);
            device = nullptr;
        }

        window.close();

        if (exit_code == EXIT_SUCCESS) {
            FR_LOG_OK("[Testbed] Renderer testbed shutdown complete.");
        } else {
            FR_LOG_ERR("[Testbed] Renderer testbed shutdown after failure.");
        }
    }

    fr::shutdown_core_ctx();
    return exit_code;
}
