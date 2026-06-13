/**
 * @file renderer.cpp
 * @author Tfoedy
 * @brief Deferred renderer resource and frame management.
 */

#include <glm/common.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/logger/logger.hpp"
#include "fr/renderer/render_gpu_frame.hpp"
#include "fr/renderer/render_passes.hpp"
#include "fr/renderer/renderer.hpp"
#include "fr/renderer/renderer_limits.hpp"

namespace fr {
namespace {

RendererLimits sanitize_limits(RendererLimits limits) noexcept {
    limits.max_instances =
        fr::math::max<USize>(1, fr::math::min(limits.max_instances, MAX_INSTANCES));

    limits.max_materials =
        fr::math::max<USize>(1, fr::math::min(limits.max_materials, MAX_RENDER_MATERIALS));

    limits.max_point_lights =
        fr::math::max<USize>(1, fr::math::min(limits.max_point_lights, MAX_POINT_LIGHTS));

    limits.max_spot_lights =
        fr::math::max<USize>(1, fr::math::min(limits.max_spot_lights, MAX_RENDER_SPOT_LIGHTS));

    limits.max_dir_lights =
        fr::math::max<USize>(1, fr::math::min(limits.max_dir_lights, MAX_DIR_LIGHTS));

    limits.max_point_shadows = fr::math::min(limits.max_point_shadows, MAX_POINT_SHADOWS);
    limits.max_spot_shadows = fr::math::min(limits.max_spot_shadows, MAX_SPOT_SHADOWS);

    return limits;
}

GpuMaterialData build_gpu_material_data(const RenderMaterialPacket &material) noexcept {
    GpuMaterialData gpu{};

    gpu.base_color_factor = material.base_color_factor;

    gpu.params0 = Vec4(material.metallic_factor, material.roughness_factor, material.alpha,
                       material.alpha_cutoff);

    gpu.params1 =
        glm::uvec4(material.shading_model, material.blend_mode, material.texture_flags, 0u);

    return gpu;
}

bool validate_draw_list(Slice<const DrawCall> draws, const RenderFrameSubmission &submission,
                        const RendererLimits &limits, const char *name) noexcept {
    bool ok = true;

    for (USize i = 0; i < draws.size(); ++i) {
        const DrawCall &draw = draws[i];

        if (draw.index_count == 0) {
            FR_LOG_ERR("[Renderer] {} draw {} has zero index_count.", name, i);
            ok = false;
        }

        if (!draw.vbo.is_valid()) {
            FR_LOG_ERR("[Renderer] {} draw {} has invalid vertex buffer.", name, i);
            ok = false;
        }

        if (!draw.ibo.is_valid()) {
            FR_LOG_ERR("[Renderer] {} draw {} has invalid index buffer.", name, i);
            ok = false;
        }

        if (!draw.pipe.is_valid()) {
            FR_LOG_ERR("[Renderer] {} draw {} has invalid pipeline.", name, i);
            ok = false;
        }

        if (draw.transform_index >= submission.transforms.size()) {
            FR_LOG_ERR("[Renderer] {} draw {} references missing transform {}.", name, i,
                       draw.transform_index);
            ok = false;
        }

        if (draw.material_index >= submission.materials.size()) {
            FR_LOG_ERR("[Renderer] {} draw {} references missing material {}.", name, i,
                       draw.material_index);
            ok = false;
        }

        if (draw.transform_index >= limits.max_instances) {
            FR_LOG_ERR("[Renderer] {} draw {} transform index {} exceeds renderer limit {}.", name,
                       i, draw.transform_index, limits.max_instances);
            ok = false;
        }

        if (draw.material_index >= limits.max_materials) {
            FR_LOG_ERR("[Renderer] {} draw {} material index {} exceeds renderer limit {}.", name,
                       i, draw.material_index, limits.max_materials);
            ok = false;
        }
    }

    return ok;
}

bool validate_frame_submission(const RenderFrameSubmission &submission,
                               const RendererLimits &limits) noexcept {
    bool ok = true;

    if (submission.transforms.size() > limits.max_instances) {
        FR_LOG_WARN("[Renderer] Submission has {} transforms, renderer uploads at most {}.",
                    submission.transforms.size(), limits.max_instances);
    }

    if (submission.materials.size() > limits.max_materials) {
        FR_LOG_WARN("[Renderer] Submission has {} materials, renderer uploads at most {}.",
                    submission.materials.size(), limits.max_materials);
    }

    if (submission.point_lights.size() > limits.max_point_lights) {
        FR_LOG_WARN("[Renderer] Submission has {} point lights, renderer uses at most {}.",
                    submission.point_lights.size(), limits.max_point_lights);
    }

    if (submission.spot_lights.size() > limits.max_spot_lights) {
        FR_LOG_WARN("[Renderer] Submission has {} spot lights, renderer uses at most {}.",
                    submission.spot_lights.size(), limits.max_spot_lights);
    }

    if (submission.directional_lights.size() > limits.max_dir_lights) {
        FR_LOG_WARN("[Renderer] Submission has {} directional lights, renderer uses at most {}.",
                    submission.directional_lights.size(), limits.max_dir_lights);
    }

    if (submission.point_shadows.size() > limits.max_point_shadows) {
        FR_LOG_WARN("[Renderer] Submission has {} point shadows, renderer uses at most {}.",
                    submission.point_shadows.size(), limits.max_point_shadows);
    }

    if (submission.spot_shadows.size() > limits.max_spot_shadows) {
        FR_LOG_WARN("[Renderer] Submission has {} spot shadows, renderer uses at most {}.",
                    submission.spot_shadows.size(), limits.max_spot_shadows);
    }

    ok = validate_draw_list(submission.draws.opaque.slice(), submission, limits, "opaque") && ok;
    ok = validate_draw_list(submission.draws.masked.slice(), submission, limits, "masked") && ok;
    ok = validate_draw_list(submission.draws.transparent.slice(), submission, limits,
                            "transparent") &&
         ok;
    // japipapi co tutaj sie z clang formaterem stalo
    ok = validate_draw_list(submission.draws.shadow.slice(), submission, limits, "shadow") && ok;

    return ok;
}

bool validate_global_buffers(const RendererGlobalBuffers &global) noexcept {
    bool ok = true;

    if (!global.transform_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing transform SSBO.");
        ok = false;
    }

    if (!global.shadow_transform_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing shadow transform SSBO.");
        ok = false;
    }

    if (!global.materials_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing material SSBO.");
        ok = false;
    }

    if (!global.camera_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing camera SSBO.");
        ok = false;
    }

    if (!global.point_lights_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing point lights SSBO.");
        ok = false;
    }

    if (!global.spot_lights_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing spot lights SSBO.");
        ok = false;
    }

    if (!global.dir_lights_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing directional lights SSBO.");
        ok = false;
    }

    if (!global.point_shadows_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing point shadows SSBO.");
        ok = false;
    }

    if (!global.spot_shadows_ssbo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing spot shadows SSBO.");
        ok = false;
    }

    return ok;
}

bool validate_fallback_textures(const RendererFallbackTextures &fallback) noexcept {
    bool ok = true;

    if (!fallback.white.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing fallback white texture.");
        ok = false;
    }

    if (!fallback.black.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing fallback black texture.");
        ok = false;
    }

    if (!fallback.normal.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing fallback normal texture.");
        ok = false;
    }

    if (!fallback.material.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing fallback material texture.");
        ok = false;
    }

    return ok;
}

bool validate_startup_resources(const RendererResources &resources) noexcept {
    bool ok = true;

    ok = validate_global_buffers(resources.global) && ok;
    ok = validate_fallback_textures(resources.fallback) && ok;

    return ok;
}

bool validate_gbuffer_targets(const GBufferTargets &gbuffer, U32 width, U32 height) noexcept {
    bool ok = true;

    if (gbuffer.width != width || gbuffer.height != height) {
        FR_LOG_ERR("[Renderer] GBuffer size mismatch. Expected {}x{}, got {}x{}.", width, height,
                   gbuffer.width, gbuffer.height);
        ok = false;
    }

    if (!gbuffer.albedo.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing GBuffer albedo target.");
        ok = false;
    }

    if (!gbuffer.normal.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing GBuffer normal target.");
        ok = false;
    }

    if (!gbuffer.extra.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing GBuffer extra target.");
        ok = false;
    }

    if (!gbuffer.depth.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing GBuffer depth target.");
        ok = false;
    }

    return ok;
}

bool validate_final_target(const FinalColorTarget &final, U32 width, U32 height) noexcept {
    bool ok = true;

    if (final.width != width || final.height != height) {
        FR_LOG_ERR("[Renderer] Final target size mismatch. Expected {}x{}, got {}x{}.", width,
                   height, final.width, final.height);
        ok = false;
    }

    if (!final.color.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing final color target.");
        ok = false;
    }

    return ok;
}

bool validate_ambient_occlusion_resources(const AmbientOcclusionResources &ao, U32 width,
                                          U32 height) noexcept {
    bool ok = true;

    if (ao.width != width || ao.height != height) {
        FR_LOG_ERR("[Renderer] AO target size mismatch. Expected {}x{}, got {}x{}.", width, height,
                   ao.width, ao.height);
        ok = false;
    }

    if (!ao.target.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing AO target.");
        ok = false;
    }

    return ok;
}

bool validate_directional_shadow_resources(const DirectionalShadowResources &shadow) noexcept {
    if (!shadow.map.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing directional shadow map.");
        return false;
    }

    return true;
}

bool validate_point_shadow_resources(const PointShadowResources &point_shadows,
                                     const RendererLimits &limits) noexcept {
    bool ok = true;

    for (USize i = 0; i < limits.max_point_shadows; ++i) {
        if (!point_shadows.cube_maps[i].is_valid()) {
            FR_LOG_ERR("[Renderer] Missing point shadow cubemap {}.", i);
            ok = false;
        }
    }

    return ok;
}

bool validate_spot_shadow_resources(const SpotShadowResources &spot_shadows,
                                    const RendererLimits &limits) noexcept {
    if (limits.max_spot_shadows == 0) {
        return true;
    }

    if (!spot_shadows.atlas.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing spot shadow atlas.");
        return false;
    }

    return true;
}

bool validate_ibl_resources(const IblResources &ibl) noexcept {
    bool ok = true;

    if (!ibl.environment.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing IBL environment cubemap.");
        ok = false;
    }

    if (!ibl.irradiance.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing IBL irradiance cubemap.");
        ok = false;
    }

    if (!ibl.prefiltered.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing IBL prefiltered cubemap.");
        ok = false;
    }

    if (!ibl.brdf_lut.is_valid()) {
        FR_LOG_ERR("[Renderer] Missing IBL BRDF LUT.");
        ok = false;
    }

    return ok;
}

bool validate_frame_resources(const RendererResources &resources, const RendererLimits &limits,
                              U32 width, U32 height) noexcept {
    bool ok = true;

    ok = validate_gbuffer_targets(resources.gbuffer, width, height) && ok;
    ok = validate_final_target(resources.final, width, height) && ok;
    ok = validate_ambient_occlusion_resources(resources.ao, width, height) && ok;

    ok = validate_directional_shadow_resources(resources.shadow) && ok;
    ok = validate_point_shadow_resources(resources.point_shadows, limits) && ok;
    ok = validate_spot_shadow_resources(resources.spot_shadows, limits) && ok;
    ok = validate_ibl_resources(resources.ibl) && ok;

    return ok;
}

} // namespace

Renderer::Renderer(RenderDevice *device, const RendererCreateDesc &desc) noexcept
    : m_device(device),
      m_alloc(desc.alloc ? desc.alloc : get_ambient_ctx().alloc),
      m_limits(sanitize_limits(desc.limits)),
      m_pipelines(desc.pipelines) {
    FR_ASSERT(device != nullptr, "Renderer requires a valid RenderDevice instance");
    FR_ASSERT(m_alloc != nullptr, "Renderer requires a valid allocator");
    FR_ASSERT(m_pipelines.is_valid(), "Renderer requires a valid pipeline set");

    init_fallback_textures();
    init_global_buffers();

    m_ready = validate_startup_resources(m_resources);

    if (!m_ready) {
        FR_LOG_ERR("[Renderer] Renderer startup resource validation failed.");
    }
}

Renderer::~Renderer() noexcept {
    if (!m_device) {
        return;
    }

    m_ready = false;

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
    FR_ASSERT(desc.submission != nullptr, "RenderFrameDesc::submission must be non-null");
    FR_ASSERT(desc.viewport.width > 0, "RenderFrameDesc::viewport.width must be non-zero");
    FR_ASSERT(desc.viewport.height > 0, "RenderFrameDesc::viewport.height must be non-zero");

    if (!m_ready) {
        FR_LOG_ERR("[Renderer] Renderer is not ready. Frame skipped.");
        return;
    }

    if (!validate_frame_submission(*desc.submission, m_limits)) {
        FR_LOG_ERR("[Renderer] Invalid frame submission. Frame skipped.");
        return;
    }

    prepare_render_targets(desc.viewport.width, desc.viewport.height);

    if (!validate_frame_resources(m_resources, m_limits, desc.viewport.width,
                                  desc.viewport.height)) {
        FR_LOG_ERR("[Renderer] Renderer frame resource validation failed. Frame skipped.");
        return;
    }

    CommandBuffer *cmd = m_device->adopt_command_buffer();

    render_pass::IblBrdfLutPassDesc brdf_desc{};
    brdf_desc.resources = &m_resources;
    brdf_desc.pipelines = &m_pipelines;
    render_pass::execute_ibl_brdf_lut(cmd, brdf_desc);

    if (desc.environment_source.is_valid()) {
        render_pass::IblEnvironmentPassDesc environment_desc{};
        environment_desc.resources = &m_resources;
        environment_desc.pipelines = &m_pipelines;
        environment_desc.source = desc.environment_source;
        render_pass::execute_ibl_environment(cmd, environment_desc);

        render_pass::IblIrradiancePassDesc irradiance_desc{};
        irradiance_desc.resources = &m_resources;
        irradiance_desc.pipelines = &m_pipelines;
        render_pass::execute_ibl_irradiance(cmd, irradiance_desc);

        render_pass::IblPrefilterPassDesc prefilter_desc{};
        prefilter_desc.resources = &m_resources;
        prefilter_desc.pipelines = &m_pipelines;
        render_pass::execute_ibl_prefilter(cmd, prefilter_desc);
    }

    update_global_buffers(desc);

    render_pass::DirectionalShadowPassDesc shadow_desc{};
    shadow_desc.resources = &m_resources;
    shadow_desc.pipelines = &m_pipelines;
    shadow_desc.submission = desc.submission;
    render_pass::execute_directional_shadow(cmd, shadow_desc);

    render_pass::PointShadowPassDesc point_shadow_desc{};
    point_shadow_desc.resources = &m_resources;
    point_shadow_desc.pipelines = &m_pipelines;
    point_shadow_desc.submission = desc.submission;
    point_shadow_desc.limits = m_limits;
    render_pass::execute_point_shadow(cmd, point_shadow_desc);

    render_pass::SpotShadowPassDesc spot_shadow_desc{};
    spot_shadow_desc.resources = &m_resources;
    spot_shadow_desc.pipelines = &m_pipelines;
    spot_shadow_desc.submission = desc.submission;
    spot_shadow_desc.limits = m_limits;
    render_pass::execute_spot_shadow(cmd, spot_shadow_desc);

    render_pass::GeometryPassDesc geometry_desc{};
    geometry_desc.resources = &m_resources;
    geometry_desc.pipelines = &m_pipelines;
    geometry_desc.submission = desc.submission;
    geometry_desc.width = desc.viewport.width;
    geometry_desc.height = desc.viewport.height;
    render_pass::execute_geometry(cmd, geometry_desc);

    if (desc.ao.enabled) {
        render_pass::HbaoPassDesc hbao_desc{};
        hbao_desc.resources = &m_resources;
        hbao_desc.pipelines = &m_pipelines;
        hbao_desc.frame = &desc;
        render_pass::execute_hbao(cmd, hbao_desc);
    }

    render_pass::LightingPassDesc lighting_desc{};
    lighting_desc.resources = &m_resources;
    lighting_desc.pipelines = &m_pipelines;
    lighting_desc.frame = &desc;
    lighting_desc.limits = m_limits;
    render_pass::execute_lighting(cmd, lighting_desc);

    render_pass::ForwardTransparentPassDesc forward_desc{};
    forward_desc.resources = &m_resources;
    forward_desc.pipelines = &m_pipelines;
    forward_desc.frame = &desc;
    forward_desc.limits = m_limits;
    render_pass::execute_forward_transparent(cmd, forward_desc);

    render_pass::PresentPassDesc present_desc{};
    present_desc.resources = &m_resources;
    present_desc.pipelines = &m_pipelines;
    render_pass::execute_present(cmd, present_desc);

    m_device->submit_command_buffer(cmd);
}

TextureHandle Renderer::final_image() const noexcept {
    return m_resources.final.color;
}

void Renderer::init_global_buffers() noexcept {
    RendererGlobalBuffers &global = m_resources.global;

    global.transform_ssbo =
        m_device->create_empty_buffer(m_limits.max_instances * sizeof(Mat4), true);

    global.shadow_transform_ssbo =
        m_device->create_empty_buffer(m_limits.max_instances * sizeof(Mat4), true);

    global.materials_ssbo =
        m_device->create_empty_buffer(m_limits.max_materials * sizeof(GpuMaterialData), true);

    global.camera_ssbo = m_device->create_empty_buffer(sizeof(GpuCameraData), true);

    global.point_lights_ssbo =
        m_device->create_empty_buffer(m_limits.max_point_lights * sizeof(PointLightData), true);

    global.spot_lights_ssbo =
        m_device->create_empty_buffer(m_limits.max_spot_lights * sizeof(SpotLightData), true);

    global.dir_lights_ssbo =
        m_device->create_empty_buffer(m_limits.max_dir_lights * sizeof(DirectionalLightData), true);

    global.point_shadows_ssbo = m_device->create_empty_buffer(
        fr::math::max<USize>(1, m_limits.max_point_shadows) * sizeof(PointShadowData), true);

    global.spot_shadows_ssbo = m_device->create_empty_buffer(
        fr::math::max<USize>(1, m_limits.max_spot_shadows) * sizeof(SpotShadowData), true);
}

void Renderer::init_fallback_textures() noexcept {
    RendererFallbackTextures &fallback = m_resources.fallback;

    alignas(4) U8 white[4] = {255, 255, 255, 255};
    fallback.white =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_SRGB,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(white), 4));

    alignas(4) U8 black[4] = {0, 0, 0, 255};
    fallback.black =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_SRGB,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(black), 4));

    alignas(4) U8 normal[4] = {128, 128, 255, 255};
    fallback.normal =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_UNorm,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(normal), 4));

    alignas(4) U8 material[4] = {0, 255, 255, 255};
    fallback.material =
        m_device->create_texture_2d(1, 1, 1, TextureFormat::R8G8B8A8_UNorm,
                                    Slice<const Byte>(reinterpret_cast<const Byte *>(material), 4));
}

void Renderer::prepare_render_targets(U32 width, U32 height) noexcept {
    GBufferTargets &gbuffer = m_resources.gbuffer;
    FinalColorTarget &final = m_resources.final;
    AmbientOcclusionResources &ao = m_resources.ao;

    DirectionalShadowResources &shadow = m_resources.shadow;
    PointShadowResources &point_shadows = m_resources.point_shadows;
    SpotShadowResources &spot_shadows = m_resources.spot_shadows;
    IblResources &ibl = m_resources.ibl;

    if (gbuffer.width != width || gbuffer.height != height) {
        destroy_ao();
        destroy_final_color();
        destroy_gbuffer();

        gbuffer.width = width;
        gbuffer.height = height;

        final.width = width;
        final.height = height;

        ao.width = width;
        ao.height = height;

        gbuffer.albedo =
            m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);

        gbuffer.normal = m_device->create_texture_2d(width, height, 1, TextureFormat::R16G16_Float);

        gbuffer.extra =
            m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);

        gbuffer.depth = m_device->create_texture_2d(width, height, 1, TextureFormat::Depth32_Float);

        final.color = m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);

        ao.target = m_device->create_texture_2d(width, height, 1, TextureFormat::R8G8B8A8_UNorm);
    }

    if (!shadow.map.is_valid()) {
        shadow.map = m_device->create_texture_2d(shadow.size, shadow.size, 1,
                                                 TextureFormat::Depth32_Float_Shadow);
    }

    for (USize i = 0; i < m_limits.max_point_shadows; ++i) {
        if (!point_shadows.cube_maps[i].is_valid()) {
            point_shadows.cube_maps[i] =
                m_device->create_texture_cube(point_shadows.size, 1, TextureFormat::Depth32_Float);
        }
    }

    if (m_limits.max_spot_shadows > 0 && !spot_shadows.atlas.is_valid()) {
        spot_shadows.atlas = m_device->create_texture_2d(spot_shadows.size, spot_shadows.size, 1,
                                                         TextureFormat::Depth32_Float_Shadow);
    }

    if (!ibl.environment.is_valid()) {
        ibl.environment = m_device->create_texture_cube(ibl.environment_size, 1,
                                                        TextureFormat::R16G16B16A16_Float);
    }

    if (!ibl.irradiance.is_valid()) {
        ibl.irradiance = m_device->create_texture_cube(ibl.irradiance_size, 1,
                                                       TextureFormat::R16G16B16A16_Float);
    }

    if (!ibl.prefiltered.is_valid()) {
        ibl.prefiltered = m_device->create_texture_cube(ibl.prefiltered_size, ibl.prefiltered_mips,
                                                        TextureFormat::R16G16B16A16_Float);
    }

    if (!ibl.brdf_lut.is_valid()) {
        ibl.brdf_lut = m_device->create_texture_2d(ibl.brdf_lut_size, ibl.brdf_lut_size, 1,
                                                   TextureFormat::R16G16_Float);
    }
}

void Renderer::update_global_buffers(const RenderFrameDesc &desc) noexcept {
    const RenderFrameSubmission &submission = *desc.submission;
    RendererGlobalBuffers &global = m_resources.global;
    IblResources &ibl = m_resources.ibl;

    if (!submission.transforms.is_empty()) {
        const USize transform_count =
            fr::math::min(submission.transforms.size(), m_limits.max_instances);

        Slice<const Byte> transform_bytes(
            reinterpret_cast<const Byte *>(submission.transforms.data()),
            transform_count * sizeof(Mat4));

        m_device->update_buffer(global.transform_ssbo, transform_bytes);
        m_device->update_buffer(global.shadow_transform_ssbo, transform_bytes);
    }

    if (!submission.materials.is_empty()) {
        const USize material_count =
            fr::math::min(submission.materials.size(), m_limits.max_materials);

        DynamicArray<GpuMaterialData> gpu_materials(m_alloc);
        gpu_materials.reserve(material_count);

        for (USize i = 0; i < material_count; ++i) {
            gpu_materials.push_back(build_gpu_material_data(submission.materials[i]));
        }

        m_device->update_buffer(
            global.materials_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(gpu_materials.data()),
                              gpu_materials.size() * sizeof(GpuMaterialData)));
    }

    auto point_lights = submission.point_lights.slice();
    auto spot_lights = submission.spot_lights.slice();
    auto dir_lights = submission.directional_lights.slice();

    auto point_shadows = submission.point_shadows.slice();
    auto spot_shadows = submission.spot_shadows.slice();

    const U32 active_point_lights =
        static_cast<U32>(fr::math::min(point_lights.size(), m_limits.max_point_lights));

    const U32 active_spot_lights =
        static_cast<U32>(fr::math::min(spot_lights.size(), m_limits.max_spot_lights));

    const U32 active_dir_lights =
        static_cast<U32>(fr::math::min(dir_lights.size(), m_limits.max_dir_lights));

    const U32 active_point_shadows =
        static_cast<U32>(fr::math::min(point_shadows.size(), m_limits.max_point_shadows));

    const U32 active_spot_shadows =
        static_cast<U32>(fr::math::min(spot_shadows.size(), m_limits.max_spot_shadows));

    if (active_point_lights > 0) {
        m_device->update_buffer(
            global.point_lights_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(point_lights.data()),
                              active_point_lights * sizeof(PointLightData)));
    }

    if (active_spot_lights > 0) {
        m_device->update_buffer(
            global.spot_lights_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(spot_lights.data()),
                              active_spot_lights * sizeof(SpotLightData)));
    }

    if (active_dir_lights > 0) {
        m_device->update_buffer(
            global.dir_lights_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(dir_lights.data()),
                              active_dir_lights * sizeof(DirectionalLightData)));
    }

    if (active_point_shadows > 0) {
        m_device->update_buffer(
            global.point_shadows_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(point_shadows.data()),
                              active_point_shadows * sizeof(PointShadowData)));
    }

    if (active_spot_shadows > 0) {
        m_device->update_buffer(
            global.spot_shadows_ssbo,
            Slice<const Byte>(reinterpret_cast<const Byte *>(spot_shadows.data()),
                              active_spot_shadows * sizeof(SpotShadowData)));
    }

    const Vec3 safe_forward = glm::length(desc.camera.forward) > 0.0001f
                                  ? glm::normalize(desc.camera.forward)
                                  : Vec3(0.0f, 0.0f, -1.0f);

    GpuCameraData camera_data{};
    camera_data.view_proj = desc.camera.view_proj;
    camera_data.inv_view_proj = glm::inverse(desc.camera.view_proj);

    camera_data.cam_pos = Vec4(desc.camera.position, 1.0f);
    camera_data.cam_forward = Vec4(safe_forward, 0.0f);

    camera_data.counts_debug = glm::uvec4(active_point_lights, active_spot_lights,
                                          active_dir_lights, static_cast<U32>(desc.debug.mode));

    camera_data.flags_reserved = glm::uvec4(desc.debug.flags, 0, 0, 0);

    camera_data.lighting_params = Vec4(
        desc.lighting.exposure > 0.0f ? desc.lighting.exposure : 1.0f,
        desc.lighting.pbr_ambient_strength >= 0.0f ? desc.lighting.pbr_ambient_strength : 0.01f,
        desc.lighting.standard_ambient_strength >= 0.0f ? desc.lighting.standard_ambient_strength
                                                        : 0.035f,
        desc.lighting.standard_specular_default >= 0.0f ? desc.lighting.standard_specular_default
                                                        : 0.25f);

    camera_data.ao_params = Vec4(desc.ao.radius > 0.0f ? desc.ao.radius : 1.5f,
                                 desc.ao.intensity >= 0.0f ? desc.ao.intensity : 1.2f,
                                 desc.ao.bias >= 0.0f ? desc.ao.bias : 0.05f,
                                 desc.ao.power > 0.0f ? desc.ao.power : 1.5f);

    camera_data.ao_params2 = Vec4(desc.ao.thickness > 0.0f ? desc.ao.thickness : 1.0f,
                                  desc.ao.enabled ? 1.0f : 0.0f, 0.0f, 0.0f);

    camera_data.ibl_params =
        Vec4(ibl.environment_ready ? 1.0f : 0.0f, ibl.irradiance_ready ? 1.0f : 0.0f,
             desc.ibl.diffuse_strength >= 0.0f ? desc.ibl.diffuse_strength : 0.10f,
             desc.ibl.specular_strength >= 0.0f ? desc.ibl.specular_strength : 1.0f);

    camera_data.ibl_params2 = Vec4(
        glm::clamp(desc.ibl.occlusion_strength, 0.0f, 1.0f),
        desc.ibl.occlusion_power > 0.0f ? desc.ibl.occlusion_power : 2.0f,
        glm::clamp(desc.ibl.sky_visibility_strength, 0.0f, 1.0f), desc.ibl.enabled ? 1.0f : 0.0f);

    camera_data.ibl_params3 =
        Vec4(ibl.prefiltered_ready ? 1.0f : 0.0f, ibl.brdf_lut_ready ? 1.0f : 0.0f,
             static_cast<F32>(ibl.prefiltered_mips > 0 ? ibl.prefiltered_mips - 1 : 0), 0.0f);

    m_device->update_buffer(
        global.camera_ssbo,
        Slice<const Byte>(reinterpret_cast<const Byte *>(&camera_data), sizeof(GpuCameraData)));
}

void Renderer::destroy_global_buffers() noexcept {
    RendererGlobalBuffers &global = m_resources.global;

    if (global.transform_ssbo.is_valid()) {
        m_device->destroy_buffer(global.transform_ssbo);
        global.transform_ssbo = {};
    }

    if (global.shadow_transform_ssbo.is_valid()) {
        m_device->destroy_buffer(global.shadow_transform_ssbo);
        global.shadow_transform_ssbo = {};
    }

    if (global.materials_ssbo.is_valid()) {
        m_device->destroy_buffer(global.materials_ssbo);
        global.materials_ssbo = {};
    }

    if (global.camera_ssbo.is_valid()) {
        m_device->destroy_buffer(global.camera_ssbo);
        global.camera_ssbo = {};
    }

    if (global.point_lights_ssbo.is_valid()) {
        m_device->destroy_buffer(global.point_lights_ssbo);
        global.point_lights_ssbo = {};
    }

    if (global.spot_lights_ssbo.is_valid()) {
        m_device->destroy_buffer(global.spot_lights_ssbo);
        global.spot_lights_ssbo = {};
    }

    if (global.dir_lights_ssbo.is_valid()) {
        m_device->destroy_buffer(global.dir_lights_ssbo);
        global.dir_lights_ssbo = {};
    }

    if (global.point_shadows_ssbo.is_valid()) {
        m_device->destroy_buffer(global.point_shadows_ssbo);
        global.point_shadows_ssbo = {};
    }

    if (global.spot_shadows_ssbo.is_valid()) {
        m_device->destroy_buffer(global.spot_shadows_ssbo);
        global.spot_shadows_ssbo = {};
    }
}

void Renderer::destroy_fallback_textures() noexcept {
    RendererFallbackTextures &fallback = m_resources.fallback;

    if (fallback.white.is_valid()) {
        m_device->destroy_texture(fallback.white);
        fallback.white = {};
    }

    if (fallback.black.is_valid()) {
        m_device->destroy_texture(fallback.black);
        fallback.black = {};
    }

    if (fallback.normal.is_valid()) {
        m_device->destroy_texture(fallback.normal);
        fallback.normal = {};
    }

    if (fallback.material.is_valid()) {
        m_device->destroy_texture(fallback.material);
        fallback.material = {};
    }
}

void Renderer::destroy_final_color() noexcept {
    FinalColorTarget &final = m_resources.final;

    if (final.color.is_valid()) {
        m_device->destroy_texture(final.color);
        final.color = {};
    }

    final.width = 0;
    final.height = 0;
}

void Renderer::destroy_gbuffer() noexcept {
    GBufferTargets &gbuffer = m_resources.gbuffer;

    if (gbuffer.albedo.is_valid()) {
        m_device->destroy_texture(gbuffer.albedo);
        gbuffer.albedo = {};
    }

    if (gbuffer.normal.is_valid()) {
        m_device->destroy_texture(gbuffer.normal);
        gbuffer.normal = {};
    }

    if (gbuffer.extra.is_valid()) {
        m_device->destroy_texture(gbuffer.extra);
        gbuffer.extra = {};
    }

    if (gbuffer.depth.is_valid()) {
        m_device->destroy_texture(gbuffer.depth);
        gbuffer.depth = {};
    }

    gbuffer.width = 0;
    gbuffer.height = 0;
}

void Renderer::destroy_ao() noexcept {
    AmbientOcclusionResources &ao = m_resources.ao;

    if (ao.target.is_valid()) {
        m_device->destroy_texture(ao.target);
        ao.target = {};
    }

    ao.width = 0;
    ao.height = 0;
}

void Renderer::destroy_shadow_resources() noexcept {
    DirectionalShadowResources &shadow = m_resources.shadow;

    if (shadow.map.is_valid()) {
        m_device->destroy_texture(shadow.map);
        shadow.map = {};
    }
}

void Renderer::destroy_point_shadow_resources() noexcept {
    PointShadowResources &point_shadows = m_resources.point_shadows;

    for (USize i = 0; i < MAX_POINT_SHADOWS; ++i) {
        if (point_shadows.cube_maps[i].is_valid()) {
            m_device->destroy_texture(point_shadows.cube_maps[i]);
            point_shadows.cube_maps[i] = {};
        }
    }
}

void Renderer::destroy_spot_shadow_resources() noexcept {
    SpotShadowResources &spot_shadows = m_resources.spot_shadows;

    if (spot_shadows.atlas.is_valid()) {
        m_device->destroy_texture(spot_shadows.atlas);
        spot_shadows.atlas = {};
    }
}

void Renderer::destroy_ibl_resources() noexcept {
    IblResources &ibl = m_resources.ibl;

    if (ibl.environment.is_valid()) {
        m_device->destroy_texture(ibl.environment);
        ibl.environment = {};
    }

    if (ibl.irradiance.is_valid()) {
        m_device->destroy_texture(ibl.irradiance);
        ibl.irradiance = {};
    }

    if (ibl.prefiltered.is_valid()) {
        m_device->destroy_texture(ibl.prefiltered);
        ibl.prefiltered = {};
    }

    if (ibl.brdf_lut.is_valid()) {
        m_device->destroy_texture(ibl.brdf_lut);
        ibl.brdf_lut = {};
    }

    ibl.source = {};
    ibl.environment_ready = false;
    ibl.irradiance_ready = false;
    ibl.prefiltered_ready = false;
    ibl.brdf_lut_ready = false;
}
} // namespace fr
