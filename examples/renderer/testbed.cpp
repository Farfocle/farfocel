/**
 * @file testbed.cpp
 * @brief Renderer testbed.
 * NOTE: This is all AI GENERATED SLOP to test the renderer. I've got no time to write this myself
 * atp, and this is all wrong and a giant anti pattern on how not to do things, especially with the
 * ECS. The actual good implementation of things in this testbed will arrive once I implement gizmos
 * and object picking.
 */

#include <chrono>
#include <cstdio>
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
#include "fr/core/dynamic_array.hpp"
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
#include "fr/renderer/primitive_meshes.hpp"
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

enum class TestbedPrimitiveKind : U8 {
    Cube,
    Plane,
    Grid,
};

struct DefaultShaderCookInput {
    fr::AssetId id{};
    fr::StringView vertex_path{};
    fr::StringView fragment_path{};
    fr::StringView output_path{};
};

struct TestbedPrimitiveMaterialSettings {
    bool use_runtime_material{true};

    F32 base_color[4]{0.8f, 0.8f, 0.8f, 1.0f};

    F32 metallic{0.0f};
    F32 roughness{0.65f};
    F32 alpha{1.0f};
    F32 alpha_cutoff{0.5f};

    S32 shading_model{static_cast<S32>(fr::MaterialShadingModel::PBR)};
    S32 blend_mode{static_cast<S32>(fr::MaterialBlendMode::Opaque)};

    char albedo_path[TESTBED_PATH_BUFFER_SIZE]{};
    char normal_path[TESTBED_PATH_BUFFER_SIZE]{};
    char extra_path[TESTBED_PATH_BUFFER_SIZE]{};

    bool albedo_srgb{true};
    bool normal_srgb{false};
    bool extra_srgb{false};

    bool dirty{false};
};

struct TestbedPrimitiveEntity {
    fr::Thing entity{fr::Thing::nil()};
    TestbedPrimitiveKind kind{TestbedPrimitiveKind::Cube};
    U32 serial{0};

    TestbedPrimitiveMaterialSettings material{};
    fr::MaterialAssetHandle material_handle{};
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
    bool show_primitives_window{true};
    bool show_lights_window{true};

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

    F32 primitive_size{1.0f};
    S32 primitive_grid_segments{16};
    bool primitive_casts_shadow{true};

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

    fr::DynamicArray<TestbedPrimitiveEntity> test_entities;

    fr::AssetId model_id{};
    fr::AssetId environment_id{};

    fr::TextureAssetHandle environment_handle{};

    bool model_loading{false};
    bool model_loaded{false};

    bool environment_loading{false};
    bool environment_loaded{false};

    U32 next_primitive_serial{1};

    explicit TestbedSceneState(fr::Alloc *alloc) noexcept
        : test_entities(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");
        test_entities.reserve(128);
    }
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

[[nodiscard]] const char *primitive_kind_name(TestbedPrimitiveKind kind) noexcept {
    switch (kind) {
    case TestbedPrimitiveKind::Cube:
        return "Cube";
    case TestbedPrimitiveKind::Plane:
        return "Plane";
    case TestbedPrimitiveKind::Grid:
        return "Grid";
    default:
        return "Unknown";
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

void copy_path_buffer(const char *src, char (&dst)[TESTBED_PATH_BUFFER_SIZE]) noexcept {
    fr::mem::set_raw_range(reinterpret_cast<Byte *>(dst), 0, TESTBED_PATH_BUFFER_SIZE);

    if (!src) {
        return;
    }

    USize length = 0;
    while (length < TESTBED_PATH_BUFFER_SIZE - 1 && src[length] != '\0') {
        ++length;
    }

    if (length > 0) {
        fr::mem::copy_raw_range(reinterpret_cast<const Byte *>(src), length,
                                reinterpret_cast<Byte *>(dst));
    }

    dst[length] = '\0';
}

void copy_material_settings(const TestbedPrimitiveMaterialSettings &src,
                            TestbedPrimitiveMaterialSettings &dst) noexcept {
    dst = src;

    copy_path_buffer(src.albedo_path, dst.albedo_path);
    copy_path_buffer(src.normal_path, dst.normal_path);
    copy_path_buffer(src.extra_path, dst.extra_path);
}

[[nodiscard]] TestbedPrimitiveMaterialSettings make_default_primitive_material_settings() noexcept {
    TestbedPrimitiveMaterialSettings settings{};
    settings.use_runtime_material = true;
    settings.base_color[0] = 0.8f;
    settings.base_color[1] = 0.8f;
    settings.base_color[2] = 0.8f;
    settings.base_color[3] = 1.0f;
    settings.metallic = 0.0f;
    settings.roughness = 0.65f;
    settings.alpha = 1.0f;
    settings.alpha_cutoff = 0.5f;
    settings.shading_model = static_cast<S32>(fr::MaterialShadingModel::PBR);
    settings.blend_mode = static_cast<S32>(fr::MaterialBlendMode::Opaque);
    settings.albedo_srgb = true;
    settings.normal_srgb = false;
    settings.extra_srgb = false;
    settings.dirty = false;
    return settings;
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
        {FR_ASSET_ID("renderer.shader.forward_transparent"),
         "engine/shaders/core/forward_transparent.vert",
         "engine/shaders/core/forward_transparent.frag",
         "assets/shaders/core/forward_transparent.fshader"},
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

    for (USize i = 0; i < state.test_entities.size(); ++i) {
        sync_entity_transform_matrix(world, state.test_entities[i].entity);
    }
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

[[nodiscard]] glm::vec3 next_test_entity_position(const TestbedSceneState &state,
                                                  F32 y_offset) noexcept {
    const USize index = state.test_entities.size();

    const S32 column = static_cast<S32>(index % 5) - 2;
    const S32 row = static_cast<S32>(index / 5);

    constexpr F32 spacing = 2.75f;

    return glm::vec3(static_cast<F32>(column) * spacing, y_offset,
                     -static_cast<F32>(row) * spacing);
}

[[nodiscard]] fr::MaterialShadingModel
primitive_shading_model(const TestbedPrimitiveMaterialSettings &settings) noexcept {
    if (settings.shading_model == static_cast<S32>(fr::MaterialShadingModel::Unlit)) {
        return fr::MaterialShadingModel::Unlit;
    }

    if (settings.shading_model == static_cast<S32>(fr::MaterialShadingModel::Standard)) {
        return fr::MaterialShadingModel::Standard;
    }

    return fr::MaterialShadingModel::PBR;
}

[[nodiscard]] fr::MaterialBlendMode
primitive_blend_mode(const TestbedPrimitiveMaterialSettings &settings) noexcept {
    if (settings.blend_mode == static_cast<S32>(fr::MaterialBlendMode::Masked)) {
        return fr::MaterialBlendMode::Masked;
    }

    if (settings.blend_mode == static_cast<S32>(fr::MaterialBlendMode::Transparent)) {
        return fr::MaterialBlendMode::Transparent;
    }

    return fr::MaterialBlendMode::Opaque;
}

[[nodiscard]] fr::RenderPass
primitive_render_pass(const TestbedPrimitiveMaterialSettings &settings) noexcept {
    const fr::MaterialBlendMode blend = primitive_blend_mode(settings);
    return blend == fr::MaterialBlendMode::Transparent ? fr::RenderPass::Transparent
                                                       : fr::RenderPass::Opaque;
}

[[nodiscard]] bool cook_texture_for_runtime_material(fr::Alloc *alloc, fr::AssetRegistry &registry,
                                                     fr::StringView source_path, bool srgb,
                                                     bool force, fr::AssetId &out_id) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    out_id = {};

    if (source_path.is_empty()) {
        return true;
    }

    fr::String output_path = cooked_path_from_source(alloc, source_path, ".ftex");
    if (output_path.size() == 0) {
        FR_LOG_ERR("[Testbed] Failed to build runtime material texture cooked path.");
        return false;
    }

    if (!ensure_directory_for_path(output_path.view())) {
        return false;
    }

    fr::DynamicArray<fr::asscooker::CookedAssetOutput> outputs(alloc);

    fr::asscooker::CookOptions options{};
    options.force = force;

    FR_LOG("[Testbed] Cooking runtime material texture: {} -> {}", source_path, output_path.view());

    if (!fr::asscooker::cook_texture_ex(source_path, output_path.view(), srgb, &outputs, options)) {
        FR_LOG_ERR("[Testbed] Failed to cook runtime material texture: {}", source_path);
        return false;
    }

    if (!register_cooked_outputs(registry, outputs.slice())) {
        return false;
    }

    out_id = fr::asscooker::resolve_output_asset_id(output_path.view(), options);
    return out_id.is_valid();
}

[[nodiscard]] fr::MaterialAssetHandle create_runtime_material_from_settings(
    fr::Alloc *alloc, fr::AssetRegistry &registry, fr::AssetManager &assets,
    const TestbedPrimitiveMaterialSettings &settings, bool force_recook) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (!settings.use_runtime_material) {
        return {};
    }

    fr::RuntimeMaterialDesc desc{};

    desc.data.base_color_factor = fr::Vec4(settings.base_color[0], settings.base_color[1],
                                           settings.base_color[2], settings.base_color[3]);

    desc.data.metallic_factor = glm::clamp(settings.metallic, 0.0f, 1.0f);
    desc.data.roughness_factor = glm::clamp(settings.roughness, 0.0f, 1.0f);
    desc.data.alpha = glm::clamp(settings.alpha, 0.0f, 1.0f);
    desc.data.alpha_cutoff = glm::clamp(settings.alpha_cutoff, 0.0f, 1.0f);

    desc.data.shading_model = primitive_shading_model(settings);
    desc.data.blend_mode = primitive_blend_mode(settings);

    fr::String albedo_path = path_buffer_to_string(alloc, settings.albedo_path);
    fr::String normal_path = path_buffer_to_string(alloc, settings.normal_path);
    fr::String extra_path = path_buffer_to_string(alloc, settings.extra_path);

    if (albedo_path.size() != 0) {
        if (!cook_texture_for_runtime_material(alloc, registry, albedo_path.view(),
                                               settings.albedo_srgb, force_recook,
                                               desc.data.albedo_texture)) {
            return {};
        }
    }

    if (normal_path.size() != 0) {
        if (!cook_texture_for_runtime_material(alloc, registry, normal_path.view(),
                                               settings.normal_srgb, force_recook,
                                               desc.data.normal_texture)) {
            return {};
        }
    }

    if (extra_path.size() != 0) {
        if (!cook_texture_for_runtime_material(alloc, registry, extra_path.view(),
                                               settings.extra_srgb, force_recook,
                                               desc.data.extra_texture)) {
            return {};
        }
    }

    fr::MaterialAssetHandle material = assets.create_runtime_material(desc);
    if (!material.is_valid()) {
        FR_LOG_ERR("[Testbed] Failed to create runtime primitive material.");
        return {};
    }

    return material;
}

void bind_entity_material_override(fr::World &world, fr::Thing entity,
                                   fr::MaterialAssetHandle material) noexcept {
    fr::MaterialOverridePart *override_part = world.try_get<fr::MaterialOverridePart>(entity);

    if (!override_part) {
        if (!material.is_valid()) {
            return;
        }

        override_part = &world.emplace_now<fr::MaterialOverridePart>(entity);
    }

    override_part->material_id = {};
    override_part->resolved_material_id = {};
    override_part->material_handle = material;
}

bool rebuild_primitive_entity_material(fr::Alloc *alloc, fr::AssetRegistry &registry,
                                       fr::AssetManager &assets, fr::World &world,
                                       bool force_recook, TestbedPrimitiveEntity &entry) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    fr::MaterialAssetHandle new_material = create_runtime_material_from_settings(
        alloc, registry, assets, entry.material, force_recook);

    if (entry.material.use_runtime_material && !new_material.is_valid()) {
        FR_LOG_ERR("[Testbed] Failed to rebuild primitive material.");
        return false;
    }

    fr::MaterialAssetHandle old_material = entry.material_handle;
    entry.material_handle = new_material;

    bind_entity_material_override(world, entry.entity, new_material);

    if (old_material.is_valid()) {
        assets.unload_material(old_material);
    }

    return true;
}

[[nodiscard]] fr::primitive_mesh::PrimitiveMeshCreateDesc
make_primitive_create_desc(const TestbedPrimitiveMaterialSettings &settings, F32 size) noexcept {
    fr::primitive_mesh::PrimitiveMeshCreateDesc desc{};
    desc.size = size > 0.001f ? size : 0.001f;
    desc.pass_type = primitive_render_pass(settings);

    return desc;
}

[[nodiscard]] fr::primitive_mesh::GridMeshCreateDesc
make_grid_create_desc(const TestbedPrimitiveMaterialSettings &settings, F32 size,
                      S32 grid_segments) noexcept {
    S32 segments = grid_segments;

    if (segments < 1) {
        segments = 1;
    }

    if (segments > 512) {
        segments = 512;
    }

    fr::primitive_mesh::GridMeshCreateDesc desc{};
    desc.size = size > 0.001f ? size : 0.001f;
    desc.x_segments = static_cast<U32>(segments);
    desc.z_segments = static_cast<U32>(segments);
    desc.pass_type = primitive_render_pass(settings);

    return desc;
}

bool spawn_runtime_mesh_entity(fr::World &world, TestbedSceneState &state,
                               fr::MeshAssetHandle mesh_handle,
                               fr::MaterialAssetHandle material_handle,
                               const TestbedPrimitiveMaterialSettings &settings,
                               TestbedPrimitiveKind kind, const glm::vec3 &position,
                               bool casts_shadow) noexcept {
    if (!mesh_handle.is_valid()) {
        return false;
    }

    fr::Thing entity = world.spawn();

    fr::WorldTransformPart &transform = world.emplace_now<fr::WorldTransformPart>(entity);
    transform.position = position;
    transform.rotation = glm::quat(glm::vec3(0.0f));
    transform.scale = glm::vec3(1.0f);
    update_transform_matrix(transform);

    fr::MeshRendererPart &mesh = world.emplace_now<fr::MeshRendererPart>(entity);
    mesh.mesh_id = {};
    mesh.resolved_mesh_id = {};
    mesh.mesh_handle = mesh_handle;
    mesh.visible = true;
    mesh.casts_shadow = casts_shadow;

    if (material_handle.is_valid()) {
        fr::MaterialOverridePart &override_part =
            world.emplace_now<fr::MaterialOverridePart>(entity);

        override_part.material_id = {};
        override_part.resolved_material_id = {};
        override_part.material_handle = material_handle;
    }

    TestbedPrimitiveEntity record{};
    record.entity = entity;
    record.kind = kind;
    record.serial = state.next_primitive_serial++;
    record.material_handle = material_handle;
    copy_material_settings(settings, record.material);

    state.test_entities.push_back(record);
    return true;
}

void spawn_test_cube(fr::Alloc *alloc, fr::AssetRegistry &registry, fr::World &world,
                     fr::AssetManager &assets, TestbedRuntimeState &runtime,
                     TestbedSceneState &state) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    const F32 size = runtime.primitive_size > 0.001f ? runtime.primitive_size : 0.001f;
    TestbedPrimitiveMaterialSettings material_settings = make_default_primitive_material_settings();

    fr::MaterialAssetHandle material = create_runtime_material_from_settings(
        alloc, registry, assets, material_settings, runtime.force_recook);

    if (material_settings.use_runtime_material && !material.is_valid()) {
        FR_LOG_ERR("[Testbed] Failed to create primitive material for cube.");
        return;
    }

    fr::primitive_mesh::PrimitiveMeshCreateDesc desc =
        make_primitive_create_desc(material_settings, size);

    fr::MeshAssetHandle mesh = fr::primitive_mesh::create_cube(assets, desc);

    if (!mesh.is_valid()) {
        if (material.is_valid()) {
            assets.unload_material(material);
        }

        FR_LOG_ERR("[Testbed] Failed to create runtime cube mesh.");
        return;
    }

    const glm::vec3 position = next_test_entity_position(state, size * 0.5f);
    if (!spawn_runtime_mesh_entity(world, state, mesh, material, material_settings,
                                   TestbedPrimitiveKind::Cube, position,
                                   runtime.primitive_casts_shadow)) {
        assets.unload_mesh(mesh);

        if (material.is_valid()) {
            assets.unload_material(material);
        }

        FR_LOG_ERR("[Testbed] Failed to spawn runtime cube entity.");
        return;
    }

    FR_LOG_OK("[Testbed] Spawned runtime cube.");
}

void spawn_test_plane(fr::Alloc *alloc, fr::AssetRegistry &registry, fr::World &world,
                      fr::AssetManager &assets, TestbedRuntimeState &runtime,
                      TestbedSceneState &state) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    const F32 size = runtime.primitive_size > 0.001f ? runtime.primitive_size : 0.001f;
    TestbedPrimitiveMaterialSettings material_settings = make_default_primitive_material_settings();

    fr::MaterialAssetHandle material = create_runtime_material_from_settings(
        alloc, registry, assets, material_settings, runtime.force_recook);

    if (material_settings.use_runtime_material && !material.is_valid()) {
        FR_LOG_ERR("[Testbed] Failed to create primitive material for plane.");
        return;
    }

    fr::primitive_mesh::PrimitiveMeshCreateDesc desc =
        make_primitive_create_desc(material_settings, size);

    fr::MeshAssetHandle mesh = fr::primitive_mesh::create_plane(assets, desc);

    if (!mesh.is_valid()) {
        if (material.is_valid()) {
            assets.unload_material(material);
        }

        FR_LOG_ERR("[Testbed] Failed to create runtime plane mesh.");
        return;
    }

    const glm::vec3 position = next_test_entity_position(state, 0.0f);
    if (!spawn_runtime_mesh_entity(world, state, mesh, material, material_settings,
                                   TestbedPrimitiveKind::Plane, position,
                                   runtime.primitive_casts_shadow)) {
        assets.unload_mesh(mesh);

        if (material.is_valid()) {
            assets.unload_material(material);
        }

        FR_LOG_ERR("[Testbed] Failed to spawn runtime plane entity.");
        return;
    }

    FR_LOG_OK("[Testbed] Spawned runtime plane.");
}

void spawn_test_grid(fr::Alloc *alloc, fr::AssetRegistry &registry, fr::World &world,
                     fr::AssetManager &assets, TestbedRuntimeState &runtime,
                     TestbedSceneState &state) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    const F32 size = runtime.primitive_size > 0.001f ? runtime.primitive_size : 0.001f;
    TestbedPrimitiveMaterialSettings material_settings = make_default_primitive_material_settings();

    fr::MaterialAssetHandle material = create_runtime_material_from_settings(
        alloc, registry, assets, material_settings, runtime.force_recook);

    if (material_settings.use_runtime_material && !material.is_valid()) {
        FR_LOG_ERR("[Testbed] Failed to create primitive material for grid.");
        return;
    }

    fr::primitive_mesh::GridMeshCreateDesc desc =
        make_grid_create_desc(material_settings, size, runtime.primitive_grid_segments);

    fr::MeshAssetHandle mesh = fr::primitive_mesh::create_grid(assets, alloc, desc);

    if (!mesh.is_valid()) {
        if (material.is_valid()) {
            assets.unload_material(material);
        }

        FR_LOG_ERR("[Testbed] Failed to create runtime grid mesh.");
        return;
    }

    const glm::vec3 position = next_test_entity_position(state, 0.0f);
    if (!spawn_runtime_mesh_entity(world, state, mesh, material, material_settings,
                                   TestbedPrimitiveKind::Grid, position,
                                   runtime.primitive_casts_shadow)) {
        assets.unload_mesh(mesh);

        if (material.is_valid()) {
            assets.unload_material(material);
        }

        FR_LOG_ERR("[Testbed] Failed to spawn runtime grid entity.");
        return;
    }

    FR_LOG_OK("[Testbed] Spawned runtime grid: segments={} size={}", desc.x_segments, desc.size);
}

void unload_test_entities(fr::World &world, fr::AssetManager &assets,
                          TestbedSceneState &state) noexcept {
    for (USize i = 0; i < state.test_entities.size(); ++i) {
        TestbedPrimitiveEntity &entry = state.test_entities[i];
        const fr::Thing entity = entry.entity;

        if (fr::MeshRendererPart *mesh = world.try_get<fr::MeshRendererPart>(entity)) {
            if (mesh->mesh_handle.is_valid()) {
                assets.unload_mesh(mesh->mesh_handle);
                mesh->mesh_handle = {};
                mesh->resolved_mesh_id = {};
            }
        }

        if (entry.material_handle.is_valid()) {
            assets.unload_material(entry.material_handle);
            entry.material_handle = {};
        }

        world.kill(entity);
    }

    state.test_entities.clear();
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

void clamp_material_settings(TestbedPrimitiveMaterialSettings &settings) noexcept {
    settings.metallic = glm::clamp(settings.metallic, 0.0f, 1.0f);
    settings.roughness = glm::clamp(settings.roughness, 0.0f, 1.0f);
    settings.alpha = glm::clamp(settings.alpha, 0.0f, 1.0f);
    settings.alpha_cutoff = glm::clamp(settings.alpha_cutoff, 0.0f, 1.0f);
    settings.shading_model = glm::clamp(settings.shading_model, 0, 2);
    settings.blend_mode = glm::clamp(settings.blend_mode, 0, 2);
}

void clamp_runtime_state(TestbedRuntimeState &state) noexcept {
    state.exposure = glm::clamp(state.exposure, 0.05f, 16.0f);
    state.ibl_diffuse_strength = glm::max(state.ibl_diffuse_strength, 0.0f);
    state.ibl_specular_strength = glm::max(state.ibl_specular_strength, 0.0f);
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

void draw_lights_window(fr::World &world, TestbedRuntimeState &runtime, TestbedSceneState &state) {
    if (!runtime.show_lights_window) {
        return;
    }

    ImGui::Begin("Lights", &runtime.show_lights_window);
    draw_light_debug(world, state);
    ImGui::End();
}

void draw_shading_mode_selector(TestbedRuntimeState &runtime) {
    const char *items[] = {"UNLIT", "STANDARD", "PBR"};
    S32 selected = static_cast<S32>(runtime.shading_mode);

    if (ImGui::Combo("Material shading", &selected, items, 3)) {
        selected = glm::clamp(selected, 0, 2);
        runtime.shading_mode = static_cast<TestbedShadingMode>(selected);
    }

    ImGui::TextDisabled("Overrides material shading model in the extracted frame.");
}

bool draw_material_settings_editor(TestbedPrimitiveMaterialSettings &settings) {
    bool request_rebuild = false;

    auto mark_after_edit = [&]() noexcept {
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            request_rebuild = true;
        }
    };

    ImGui::Checkbox("Use runtime material", &settings.use_runtime_material);
    mark_after_edit();

    ImGui::ColorEdit4("Base color", settings.base_color);
    mark_after_edit();

    const char *shading_items[] = {"UNLIT", "STANDARD", "PBR"};
    if (ImGui::Combo("Shading model", &settings.shading_model, shading_items, 3)) {
        request_rebuild = true;
    }

    const char *blend_items[] = {"OPAQUE", "MASKED", "TRANSPARENT"};
    if (ImGui::Combo("Blend mode", &settings.blend_mode, blend_items, 3)) {
        request_rebuild = true;
    }

    if (settings.blend_mode == static_cast<S32>(fr::MaterialBlendMode::Transparent)) {
        ImGui::TextDisabled("Transparent primitives are rendered by the forward pass.");
    }

    ImGui::DragFloat("Metallic", &settings.metallic, 0.01f, 0.0f, 1.0f);
    mark_after_edit();

    ImGui::DragFloat("Roughness", &settings.roughness, 0.01f, 0.0f, 1.0f);
    mark_after_edit();

    ImGui::DragFloat("Alpha", &settings.alpha, 0.01f, 0.0f, 1.0f);
    mark_after_edit();

    ImGui::DragFloat("Alpha cutoff", &settings.alpha_cutoff, 0.01f, 0.0f, 1.0f);
    mark_after_edit();

    ImGui::SeparatorText("Texture Sources");

    constexpr ImGuiInputTextFlags path_flags = ImGuiInputTextFlags_EnterReturnsTrue;

    if (ImGui::InputText("Albedo path", settings.albedo_path, TESTBED_PATH_BUFFER_SIZE,
                         path_flags)) {
        request_rebuild = true;
    }

    ImGui::Checkbox("Albedo SRGB", &settings.albedo_srgb);
    mark_after_edit();

    if (ImGui::InputText("Normal path", settings.normal_path, TESTBED_PATH_BUFFER_SIZE,
                         path_flags)) {
        request_rebuild = true;
    }

    ImGui::Checkbox("Normal SRGB", &settings.normal_srgb);
    mark_after_edit();

    if (ImGui::InputText("Extra/PBR path", settings.extra_path, TESTBED_PATH_BUFFER_SIZE,
                         path_flags)) {
        request_rebuild = true;
    }

    ImGui::Checkbox("Extra/PBR SRGB", &settings.extra_srgb);
    mark_after_edit();

    ImGui::TextDisabled("Texture paths apply on Enter or with Rebuild Material.");

    clamp_material_settings(settings);

    if (request_rebuild) {
        settings.dirty = true;
    }

    return request_rebuild;
}

void draw_primitive_entity_debug(fr::Alloc *alloc, fr::AssetRegistry &registry, fr::World &world,
                                 fr::AssetManager &assets, TestbedRuntimeState &runtime,
                                 TestbedPrimitiveEntity &entry) {
    char label[64]{};
    std::snprintf(label, sizeof(label), "%s #%u", primitive_kind_name(entry.kind), entry.serial);

    if (!ImGui::TreeNode(label)) {
        return;
    }

    draw_transform_debug(world, entry.entity);

    fr::MeshRendererPart *mesh = world.try_get<fr::MeshRendererPart>(entry.entity);
    if (mesh) {
        ImGui::Checkbox("Visible", &mesh->visible);
        ImGui::Checkbox("Casts shadow", &mesh->casts_shadow);
        ImGui::Text("Mesh handle: %s", mesh->mesh_handle.is_valid() ? "valid" : "invalid");
    } else {
        ImGui::TextDisabled("No MeshRendererPart.");
    }

    ImGui::SeparatorText("Material");

    const bool changed = draw_material_settings_editor(entry.material);

    if (entry.material.dirty) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.25f, 1.0f), "Material changed.");
    }

    bool apply = changed;

    if (ImGui::Button("Rebuild Material")) {
        apply = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset Material")) {
        entry.material = make_default_primitive_material_settings();
        apply = true;
    }

    if (apply) {
        if (rebuild_primitive_entity_material(alloc, registry, assets, world, runtime.force_recook,
                                              entry)) {
            entry.material.dirty = false;
        }
    }

    ImGui::TreePop();
}

void draw_runtime_mesh_window(fr::Alloc *alloc, fr::AssetRegistry &registry, fr::World &world,
                              fr::AssetManager &assets, TestbedRuntimeState &runtime,
                              TestbedSceneState &scene_state) {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (!runtime.show_primitives_window) {
        return;
    }

    ImGui::Begin("Runtime Primitives", &runtime.show_primitives_window);

    ImGui::SeparatorText("Spawn Geometry");
    ImGui::DragFloat("Primitive size", &runtime.primitive_size, 0.05f, 0.001f, 1000.0f);
    ImGui::DragInt("Grid segments", &runtime.primitive_grid_segments, 1.0f, 1, 512);
    ImGui::Checkbox("Primitive casts shadow", &runtime.primitive_casts_shadow);

    ImGui::SeparatorText("Spawn");

    if (ImGui::Button("Spawn Cube")) {
        spawn_test_cube(alloc, registry, world, assets, runtime, scene_state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Spawn Plane")) {
        spawn_test_plane(alloc, registry, world, assets, runtime, scene_state);
    }

    ImGui::SameLine();

    if (ImGui::Button("Spawn Grid")) {
        spawn_test_grid(alloc, registry, world, assets, runtime, scene_state);
    }

    if (ImGui::Button("Clear spawned")) {
        unload_test_entities(world, assets, scene_state);
    }

    ImGui::Text("Spawned runtime meshes: %llu",
                static_cast<unsigned long long>(scene_state.test_entities.size()));

    if (ImGui::CollapsingHeader("Spawned primitives", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (USize i = 0; i < scene_state.test_entities.size(); ++i) {
            ImGui::PushID(static_cast<int>(scene_state.test_entities[i].serial));
            draw_primitive_entity_debug(alloc, registry, world, assets, runtime,
                                        scene_state.test_entities[i]);
            ImGui::PopID();
        }
    }

    ImGui::End();
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
    ImGui::Checkbox("Runtime Primitives window", &runtime.show_primitives_window);
    ImGui::Checkbox("Lights window", &runtime.show_lights_window);

    if (ImGui::CollapsingHeader("Assets", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_asset_debug(assets, runtime, scene_state);
    }

    if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_renderer_debug(runtime);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_camera_debug(world, scene_state);
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

                            TestbedSceneState scene_state(alloc);
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
                                draw_runtime_mesh_window(alloc, registry, world, assets, runtime,
                                                         scene_state);
                                draw_lights_window(world, runtime, scene_state);

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
                                    extract_desc.forward_transparent_pipeline =
                                        renderer.forward_transparent_pipeline();
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

                            unload_test_entities(world, assets, scene_state);

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
