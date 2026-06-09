/**
 * @file renderer.cpp
 * @author Tfoedy
 * @brief Deferred renderer resource and frame management.
 */

#include <algorithm>
#include <glm/common.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "fr/renderer/renderer.hpp"
#include "fr/core/macros.hpp"
#include "fr/renderer/renderer_constants.hpp"
#include "fr/renderer/renderer_frame_data.hpp"


namespace fr {
namespace {

RendererLimits sanitize_limits(RendererLimits limits) noexcept {
    limits.max_instances = std::max<USize>(1, std::min(limits.max_instances, MAX_INSTANCES));

    limits.max_point_lights =
        std::max<USize>(1, std::min(limits.max_point_lights, MAX_POINT_LIGHTS));

    limits.max_spot_lights =
        std::max<USize>(1, std::min(limits.max_spot_lights, MAX_RENDER_SPOT_LIGHTS));

    limits.max_dir_lights = std::max<USize>(1, std::min(limits.max_dir_lights, MAX_DIR_LIGHTS));

    limits.max_point_shadows = std::min(limits.max_point_shadows, MAX_POINT_SHADOWS);
    limits.max_spot_shadows = std::min(limits.max_spot_shadows, MAX_SPOT_SHADOWS);

    return limits;
}

} // namespace

Renderer::Renderer(RenderDevice *device, const RendererCreateDesc &desc) noexcept
    : m_device(device),
      m_limits(sanitize_limits(desc.limits)) {
    FR_ASSERT(device != nullptr, "Renderer requires a valid RenderDevice instance");

    init_fallback_textures();
    init_global_buffers();
}

Renderer::~Renderer() noexcept {
    if (!m_device) {
        return;
    }

    destroy_global_buffers();
    destroy_fallback_textures();
    destroy_ibl_resources();
    destroy_point_shadow_resources();
    destroy_spot_shadow_resources();
    destroy_shadow_resources();
    destroy_ao();
    destroy_final_color();
    destroy_gbuffer();
}

void Renderer::render(const RenderFrameDesc &desc) noexcept {
    FR_ASSERT(desc.geom_queue != nullptr, "RenderFrameDesc::geom_queue must be non-null");
    FR_ASSERT(desc.shadow_queue != nullptr, "RenderFrameDesc::shadow_queue must be non-null");
    FR_ASSERT(desc.viewport.width > 0, "RenderFrameDesc::viewport.width must be non-zero");
    FR_ASSERT(desc.viewport.height > 0, "RenderFrameDesc::viewport.height must be non-zero");

    prepare_render_targets(desc.viewport.width, desc.viewport.height);

    CommandBuffer *cmd = m_device->adopt_command_buffer();

    execute_ibl_brdf_lut_pass(cmd, desc.pipelines.brdf_lut);

    if (desc.skybox_map.is_valid()) {
        execute_ibl_environment_pass(cmd, desc.skybox_map, desc.pipelines.equirect_to_cube);
        execute_ibl_irradiance_pass(cmd, desc.pipelines.irradiance);
        execute_ibl_prefilter_pass(cmd, desc.pipelines.prefilter_env);
    }

    update_global_buffers(desc);

    execute_shadow_pass(cmd, *desc.shadow_queue, desc.pipelines.shadow);

    execute_point_shadow_pass(cmd, *desc.shadow_queue, *desc.geom_queue,
                              desc.pipelines.point_shadow);

    execute_spot_shadow_pass(cmd, *desc.shadow_queue, *desc.geom_queue, desc.pipelines.spot_shadow);

    execute_geometry_pass(cmd, *desc.geom_queue, desc.viewport.width, desc.viewport.height);

    if (desc.ao.enabled) {
        execute_hbao_pass(cmd, desc);
    }

    execute_lighting_pass(cmd, desc);

    execute_lighting_pass(cmd, desc);
    execute_present_pass(cmd, desc.pipelines.present);

    m_device->submit_command_buffer(cmd);
}

TextureHandle Renderer::get_final_image() const noexcept {
    return m_final.color;
}

void Renderer::init_global_buffers() noexcept {
    m_global.transform_ssbo =
        m_device->create_empty_buffer(m_limits.max_instances * sizeof(glm::mat4), true);

    m_global.shadow_transform_ssbo =
        m_device->create_empty_buffer(m_limits.max_instances * sizeof(glm::mat4), true);

    m_global.camera_ssbo = m_device->create_empty_buffer(sizeof(CameraData), true);

    m_global.point_lights_ssbo =
        m_device->create_empty_buffer(m_limits.max_point_lights * sizeof(PointLightData), true);

    m_global.spot_lights_ssbo =
        m_device->create_empty_buffer(m_limits.max_spot_lights * sizeof(SpotLightData), true);

    m_global.dir_lights_ssbo =
        m_device->create_empty_buffer(m_limits.max_dir_lights * sizeof(DirectionalLightData), true);

    m_global.point_shadows_ssbo = m_device->create_empty_buffer(
        std::max<USize>(1, m_limits.max_point_shadows) * sizeof(PointShadowData), true);

    m_global.spot_shadows_ssbo = m_device->create_empty_buffer(
        std::max<USize>(1, m_limits.max_spot_shadows) * sizeof(SpotShadowData), true);
}

void Renderer::init_fallback_textures() noexcept {
    alignas(4) U8 white[4] = {255, 255, 255, 255};
    m_fallback.white =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_SRGB,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(white), 4));

    alignas(4) U8 black[4] = {0, 0, 0, 255};
    m_fallback.black =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_SRGB,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(black), 4));

    alignas(4) U8 normal[4] = {128, 128, 255, 255};
    m_fallback.normal =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_UNorm,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(normal), 4));

    alignas(4) U8 material[4] = {0, 255, 255, 255};
    m_fallback.material =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_UNorm,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(material), 4));
}

void Renderer::prepare_render_targets(U32 width, U32 height) noexcept {
    if (m_gbuffer.width != width || m_gbuffer.height != height) {
        destroy_ao();
        destroy_final_color();
        destroy_gbuffer();

        m_gbuffer.width = width;
        m_gbuffer.height = height;

        m_final.width = width;
        m_final.height = height;

        m_ao.width = width;
        m_ao.height = height;

        m_gbuffer.albedo =
            m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);

        m_gbuffer.normal =
            m_device->create_texture_2d(width, height, 1, TextureFormat::R16G16_Float);

        m_gbuffer.extra =
            m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);

        m_gbuffer.depth =
            m_device->create_texture_2d(width, height, 1, TextureFormat::Depth32_Float);

        m_final.color =
            m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);

        m_ao.target = m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);
    }

    if (!m_shadow.map.is_valid()) {
        m_shadow.map = m_device->create_texture_2d(m_shadow.size, m_shadow.size, 1,
                                                   TextureFormat::Depth32_Float_Shadow);
    }

    for (USize i = 0; i < m_limits.max_point_shadows; ++i) {
        if (!m_point_shadows.cube_maps[i].is_valid()) {
            m_point_shadows.cube_maps[i] = m_device->create_texture_cube(
                m_point_shadows.size, 1, TextureFormat::Depth32_Float);
        }
    }

    if (m_limits.max_spot_shadows > 0 && !m_spot_shadows.atlas.is_valid()) {
        m_spot_shadows.atlas = m_device->create_texture_2d(m_spot_shadows.size, m_spot_shadows.size,
                                                           1, TextureFormat::Depth32_Float_Shadow);
    }

    if (!m_ibl.environment.is_valid()) {
        m_ibl.environment = m_device->create_texture_cube(m_ibl.environment_size, 1,
                                                          TextureFormat::R16G16B16A16_Float);
    }

    if (!m_ibl.irradiance.is_valid()) {
        m_ibl.irradiance = m_device->create_texture_cube(m_ibl.irradiance_size, 1,
                                                         TextureFormat::R16G16B16A16_Float);
    }

    if (!m_ibl.prefiltered.is_valid()) {
        m_ibl.prefiltered = m_device->create_texture_cube(
            m_ibl.prefiltered_size, m_ibl.prefiltered_mips, TextureFormat::R16G16B16A16_Float);
    }

    if (!m_ibl.brdf_lut.is_valid()) {
        m_ibl.brdf_lut = m_device->create_texture_2d(m_ibl.brdf_lut_size, m_ibl.brdf_lut_size, 1,
                                                     TextureFormat::R16G16_Float);
    }
}

void Renderer::update_global_buffers(const RenderFrameDesc &desc) noexcept {
    const RenderQueue &geom_queue = *desc.geom_queue;
    const RenderQueue &shadow_queue = *desc.shadow_queue;

    auto geom_transforms = geom_queue.get_transforms();
    if (!geom_transforms.is_empty()) {
        const USize transform_count = std::min(geom_transforms.size(), m_limits.max_instances);

        m_device->update_buffer(
            m_global.transform_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(geom_transforms.data()),
                              transform_count * sizeof(glm::mat4)));
    }

    auto shadow_transforms = shadow_queue.get_transforms();
    if (!shadow_transforms.is_empty()) {
        const USize transform_count = std::min(shadow_transforms.size(), m_limits.max_instances);

        m_device->update_buffer(
            m_global.shadow_transform_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(shadow_transforms.data()),
                              transform_count * sizeof(glm::mat4)));
    }

    auto point_lights = geom_queue.get_point_lights();
    auto spot_lights = geom_queue.get_spot_lights();
    auto dir_lights = geom_queue.get_directional_lights();

    auto point_shadows = geom_queue.get_point_shadows();
    auto spot_shadows = geom_queue.get_spot_shadows();

    const U32 active_point_lights =
        static_cast<U32>(std::min(point_lights.size(), m_limits.max_point_lights));

    const U32 active_spot_lights =
        static_cast<U32>(std::min(spot_lights.size(), m_limits.max_spot_lights));

    const U32 active_dir_lights =
        static_cast<U32>(std::min(dir_lights.size(), m_limits.max_dir_lights));

    const U32 active_point_shadows =
        static_cast<U32>(std::min(point_shadows.size(), m_limits.max_point_shadows));

    const U32 active_spot_shadows =
        static_cast<U32>(std::min(spot_shadows.size(), m_limits.max_spot_shadows));

    if (active_point_lights > 0) {
        m_device->update_buffer(
            m_global.point_lights_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(point_lights.data()),
                              active_point_lights * sizeof(PointLightData)));
    }

    if (active_spot_lights > 0) {
        m_device->update_buffer(
            m_global.spot_lights_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(spot_lights.data()),
                              active_spot_lights * sizeof(SpotLightData)));
    }

    if (active_dir_lights > 0) {
        m_device->update_buffer(
            m_global.dir_lights_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(dir_lights.data()),
                              active_dir_lights * sizeof(DirectionalLightData)));
    }

    if (active_point_shadows > 0) {
        m_device->update_buffer(
            m_global.point_shadows_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(point_shadows.data()),
                              active_point_shadows * sizeof(PointShadowData)));
    }

    if (active_spot_shadows > 0) {
        m_device->update_buffer(
            m_global.spot_shadows_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(spot_shadows.data()),
                              active_spot_shadows * sizeof(SpotShadowData)));
    }

    const glm::vec3 safe_forward = glm::length(desc.camera.forward) > 0.0001f
                                       ? glm::normalize(desc.camera.forward)
                                       : glm::vec3(0.0f, 0.0f, -1.0f);

    CameraData camera_data{};
    camera_data.view_proj = desc.camera.view_proj;
    camera_data.inv_view_proj = glm::inverse(desc.camera.view_proj);

    camera_data.cam_pos = glm::vec4(desc.camera.position, 1.0f);
    camera_data.cam_forward = glm::vec4(safe_forward, 0.0f);

    camera_data.counts_debug = glm::uvec4(active_point_lights, active_spot_lights,
                                          active_dir_lights, static_cast<U32>(desc.debug.mode));

    camera_data.flags_reserved = glm::uvec4(desc.debug.flags, 0, 0, 0);

    camera_data.lighting_params = glm::vec4(
        desc.lighting.exposure > 0.0f ? desc.lighting.exposure : 1.0f,
        desc.lighting.pbr_ambient_strength >= 0.0f ? desc.lighting.pbr_ambient_strength : 0.01f,
        desc.lighting.standard_ambient_strength >= 0.0f ? desc.lighting.standard_ambient_strength
                                                        : 0.035f,
        desc.lighting.standard_specular_default >= 0.0f ? desc.lighting.standard_specular_default
                                                        : 0.25f);

    camera_data.ao_params = glm::vec4(desc.ao.radius > 0.0f ? desc.ao.radius : 1.5f,
                                      desc.ao.intensity >= 0.0f ? desc.ao.intensity : 1.2f,
                                      desc.ao.bias >= 0.0f ? desc.ao.bias : 0.05f,
                                      desc.ao.power > 0.0f ? desc.ao.power : 1.5f);

    camera_data.ao_params2 = glm::vec4(desc.ao.thickness > 0.0f ? desc.ao.thickness : 1.0f,
                                       desc.ao.enabled ? 1.0f : 0.0f, 0.0f, 0.0f);

    camera_data.ibl_params =
        glm::vec4(m_ibl.environment_ready ? 1.0f : 0.0f, m_ibl.irradiance_ready ? 1.0f : 0.0f,
                  desc.ibl.diffuse_strength >= 0.0f ? desc.ibl.diffuse_strength : 0.10f,
                  desc.ibl.specular_strength >= 0.0f ? desc.ibl.specular_strength : 1.0f);

    camera_data.ibl_params2 = glm::vec4(
        glm::clamp(desc.ibl.occlusion_strength, 0.0f, 1.0f),
        desc.ibl.occlusion_power > 0.0f ? desc.ibl.occlusion_power : 2.0f,
        glm::clamp(desc.ibl.sky_visibility_strength, 0.0f, 1.0f), desc.ibl.enabled ? 1.0f : 0.0f);

    camera_data.ibl_params3 = glm::vec4(
        m_ibl.prefiltered_ready ? 1.0f : 0.0f, m_ibl.brdf_lut_ready ? 1.0f : 0.0f,
        static_cast<F32>(m_ibl.prefiltered_mips > 0 ? m_ibl.prefiltered_mips - 1 : 0), 0.0f);

    m_device->update_buffer(
        m_global.camera_ssbo,
        Slice<const Byte>(reinterpret_cast<const Byte *>(&camera_data), sizeof(CameraData)));
}

void Renderer::destroy_global_buffers() noexcept {
    if (m_global.transform_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.transform_ssbo);
        m_global.transform_ssbo = {};
    }

    if (m_global.shadow_transform_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.shadow_transform_ssbo);
        m_global.shadow_transform_ssbo = {};
    }

    if (m_global.camera_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.camera_ssbo);
        m_global.camera_ssbo = {};
    }

    if (m_global.point_lights_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.point_lights_ssbo);
        m_global.point_lights_ssbo = {};
    }

    if (m_global.spot_lights_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.spot_lights_ssbo);
        m_global.spot_lights_ssbo = {};
    }

    if (m_global.dir_lights_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.dir_lights_ssbo);
        m_global.dir_lights_ssbo = {};
    }

    if (m_global.point_shadows_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.point_shadows_ssbo);
        m_global.point_shadows_ssbo = {};
    }

    if (m_global.spot_shadows_ssbo.is_valid()) {
        m_device->destroy_buffer(m_global.spot_shadows_ssbo);
        m_global.spot_shadows_ssbo = {};
    }
}

void Renderer::destroy_fallback_textures() noexcept {
    if (m_fallback.white.is_valid()) {
        m_device->destroy_texture(m_fallback.white);
        m_fallback.white = {};
    }

    if (m_fallback.black.is_valid()) {
        m_device->destroy_texture(m_fallback.black);
        m_fallback.black = {};
    }

    if (m_fallback.normal.is_valid()) {
        m_device->destroy_texture(m_fallback.normal);
        m_fallback.normal = {};
    }

    if (m_fallback.material.is_valid()) {
        m_device->destroy_texture(m_fallback.material);
        m_fallback.material = {};
    }
}

void Renderer::destroy_final_color() noexcept {
    if (m_final.color.is_valid()) {
        m_device->destroy_texture(m_final.color);
        m_final.color = {};
    }

    m_final.width = 0;
    m_final.height = 0;
}

void Renderer::destroy_gbuffer() noexcept {
    if (m_gbuffer.albedo.is_valid()) {
        m_device->destroy_texture(m_gbuffer.albedo);
        m_gbuffer.albedo = {};
    }

    if (m_gbuffer.normal.is_valid()) {
        m_device->destroy_texture(m_gbuffer.normal);
        m_gbuffer.normal = {};
    }

    if (m_gbuffer.extra.is_valid()) {
        m_device->destroy_texture(m_gbuffer.extra);
        m_gbuffer.extra = {};
    }

    if (m_gbuffer.depth.is_valid()) {
        m_device->destroy_texture(m_gbuffer.depth);
        m_gbuffer.depth = {};
    }

    m_gbuffer.width = 0;
    m_gbuffer.height = 0;
}

void Renderer::destroy_ao() noexcept {
    if (m_ao.target.is_valid()) {
        m_device->destroy_texture(m_ao.target);
        m_ao.target = {};
    }

    m_ao.width = 0;
    m_ao.height = 0;
}

void Renderer::destroy_shadow_resources() noexcept {
    if (m_shadow.map.is_valid()) {
        m_device->destroy_texture(m_shadow.map);
        m_shadow.map = {};
    }
}

void Renderer::destroy_point_shadow_resources() noexcept {
    for (USize i = 0; i < MAX_POINT_SHADOWS; ++i) {
        if (m_point_shadows.cube_maps[i].is_valid()) {
            m_device->destroy_texture(m_point_shadows.cube_maps[i]);
            m_point_shadows.cube_maps[i] = {};
        }
    }
}

void Renderer::destroy_spot_shadow_resources() noexcept {
    if (m_spot_shadows.atlas.is_valid()) {
        m_device->destroy_texture(m_spot_shadows.atlas);
        m_spot_shadows.atlas = {};
    }
}

void Renderer::destroy_ibl_resources() noexcept {
    if (m_ibl.environment.is_valid()) {
        m_device->destroy_texture(m_ibl.environment);
        m_ibl.environment = {};
    }

    if (m_ibl.irradiance.is_valid()) {
        m_device->destroy_texture(m_ibl.irradiance);
        m_ibl.irradiance = {};
    }

    if (m_ibl.prefiltered.is_valid()) {
        m_device->destroy_texture(m_ibl.prefiltered);
        m_ibl.prefiltered = {};
    }

    if (m_ibl.brdf_lut.is_valid()) {
        m_device->destroy_texture(m_ibl.brdf_lut);
        m_ibl.brdf_lut = {};
    }

    m_ibl.source = {};
    m_ibl.environment_ready = false;
    m_ibl.irradiance_ready = false;
    m_ibl.prefiltered_ready = false;
    m_ibl.brdf_lut_ready = false;
}

} // namespace fr
