/**
 * @file render_scene_extractor.hpp
 * @author Tfoedy
 * @brief Scene to renderer-frame extraction helpers.
 */

#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "fr/core/math.hpp"

#include "fr/asset/asset_manager.hpp"
#include "fr/asset/material_format.hpp"

#include "fr/data/parts.hpp"

#include "fr/data/world.hpp"
#include "fr/renderer/frustum.hpp"
#include "fr/renderer/render_frame.hpp"
#include "fr/renderer/render_mesh.hpp"
#include "fr/scene/render_parts.hpp"

namespace fr {

/**
 * @brief Mesh submission statistics.
 */
struct RenderSubmitStats {
    U32 total_submeshes{0};
    U32 visible_submeshes{0};
    U32 culled_submeshes{0};
    U32 skipped_submeshes{0};
};

struct RenderCameraData {
    Mat4 view_proj{1.0f};
    Vec3 position{0.0f};
    Vec3 forward{0.0f, 0.0f, -1.0f};

    bool found{false};
};

/**
 * @brief Directional shadow cascade setup.
 */
struct RenderDirectionalShadowSettings {
    Vec3 cascade_splits{15.0f, 50.0f, 150.0f};
    Vec3 cascade_half_extents{15.0f, 50.0f, 150.0f};
    Vec3 cascade_depth_ranges{50.0f, 150.0f, 300.0f};

    F32 min_bias{0.0001f};
    F32 slope_bias{0.0010f};
    F32 cascade_bias_scale{1.0f};
    F32 shadow_strength{1.0f};

    F32 filter_radius_texels{1.35f};
    F32 cascade_filter_scale{0.35f};
};

/**
 * @brief Extracts render data from ECS scene state.
 *
 * @details
 * This class does not render and does not load assets. It only converts resolved scene data into
 * RenderFrameSubmission packets.
 */
class RenderSceneExtractor {
public:
    /**
     * @brief Extracts the main camera data.
     */
    static RenderCameraData extract_camera_data(World &world, F32 aspect_ratio) noexcept {
        for (auto [thing, cam, trans] : world.query<CameraPart, WorldTransformPart>()) {
            (void)thing;

            if (cam.is_main) {
                Mat4 proj = glm::perspective(glm::radians(cam.fov), aspect_ratio, cam.near_plane,
                                             cam.far_plane);

                Vec3 forward = trans.rotation * Vec3(0.0f, 0.0f, -1.0f);
                Vec3 up = trans.rotation * Vec3(0.0f, 1.0f, 0.0f);

                return {
                    proj * glm::lookAt(trans.position, trans.position + forward, up),
                    trans.position,
                    forward,
                    true,
                };
            }
        }

        return {
            Mat4(1.0f),
            Vec3(0.0f),
            Vec3(0.0f, 0.0f, -1.0f),
            false,
        };
    }

    /**
     * @brief Submits visible mesh geometry.
     */
    static RenderSubmitStats submit_meshes(World &world, RenderFrameSubmission &submission,
                                           const AssetManager &assets,
                                           RenderPipelineHandle geometry_pipe,
                                           RenderPipelineHandle forward_transparent_pipe,
                                           const Mat4 &view_proj, const Vec3 &camera_position,
                                           const Vec3 &camera_forward) noexcept {
        RenderSubmitStats stats{};
        Frustum frustum = Frustum::extract_from_matrix(view_proj);

        const Vec3 safe_camera_forward = glm::length(camera_forward) > 0.0001f
                                             ? glm::normalize(camera_forward)
                                             : Vec3(0.0f, 0.0f, -1.0f);

        for (auto [thing, mesh_part, trans_part] :
             world.query<MeshRendererPart, WorldTransformPart>()) {
            if (!mesh_part.visible) {
                continue;
            }

            if (!mesh_part.mesh_handle.is_valid()) {
                continue;
            }

            const RenderMeshData *mesh_data = assets.get_mesh_data(mesh_part.mesh_handle);
            if (!mesh_data) {
                continue;
            }

            const MaterialOverridePart *material_part = world.try_get<MaterialOverridePart>(thing);
            const Mat4 base_model = build_model_matrix(trans_part);

            for (USize i = 0; i < mesh_data->submeshes.size(); ++i) {
                const RenderSubMesh &submesh = mesh_data->submeshes[i];
                ++stats.total_submeshes;

                MaterialAssetHandle material_handle =
                    resolve_material_handle(mesh_part, material_part, i, assets);

                RenderMaterialPacket material = build_material_packet(material_handle, assets);
                const RenderPass pass = resolve_render_pass(submesh, material);

                if (!should_submit_to_main_pass(pass)) {
                    ++stats.skipped_submeshes;
                    continue;
                }

                const Mat4 model = base_model * submesh.transform;

                if (!is_submesh_visible(frustum, submesh, model)) {
                    ++stats.culled_submeshes;
                    continue;
                }

                const U32 transform_index = submission.push_transform(model);
                const U32 material_index = submission.push_material(material);

                const RenderPipelineHandle pipeline =
                    pass == RenderPass::Transparent ? forward_transparent_pipe : geometry_pipe;

                const U32 depth_key =
                    pass == RenderPass::Transparent
                        ? build_transparent_depth_key(submesh, model, camera_position,
                                                      safe_camera_forward)
                        : 0;

                DrawCall call =
                    build_draw_call(*mesh_data, submesh, pipeline, material.albedo, depth_key);

                call.transform_index = transform_index;
                call.material_index = material_index;

                submission.push_draw(pass, call);
                ++stats.visible_submeshes;
            }
        }

        return stats;
    }

    /**
     * @brief Submits mesh geometry without frustum culling.
     */
    static RenderSubmitStats submit_meshes_no_cull(World &world, RenderFrameSubmission &submission,
                                                   const AssetManager &assets,
                                                   RenderPipelineHandle geometry_pipe,
                                                   RenderPipelineHandle forward_transparent_pipe,
                                                   const Vec3 &camera_position,
                                                   const Vec3 &camera_forward) noexcept {
        RenderSubmitStats stats{};

        const Vec3 safe_camera_forward = glm::length(camera_forward) > 0.0001f
                                             ? glm::normalize(camera_forward)
                                             : Vec3(0.0f, 0.0f, -1.0f);

        for (auto [thing, mesh_part, trans_part] :
             world.query<MeshRendererPart, WorldTransformPart>()) {
            if (!mesh_part.visible) {
                continue;
            }

            if (!mesh_part.mesh_handle.is_valid()) {
                continue;
            }

            const RenderMeshData *mesh_data = assets.get_mesh_data(mesh_part.mesh_handle);
            if (!mesh_data) {
                continue;
            }

            const MaterialOverridePart *material_part = world.try_get<MaterialOverridePart>(thing);
            const Mat4 base_model = build_model_matrix(trans_part);

            for (USize i = 0; i < mesh_data->submeshes.size(); ++i) {
                const RenderSubMesh &submesh = mesh_data->submeshes[i];
                ++stats.total_submeshes;

                MaterialAssetHandle material_handle =
                    resolve_material_handle(mesh_part, material_part, i, assets);

                RenderMaterialPacket material = build_material_packet(material_handle, assets);
                const RenderPass pass = resolve_render_pass(submesh, material);

                if (!should_submit_to_main_pass(pass)) {
                    ++stats.skipped_submeshes;
                    continue;
                }

                const Mat4 model = base_model * submesh.transform;

                const U32 transform_index = submission.push_transform(model);
                const U32 material_index = submission.push_material(material);

                const RenderPipelineHandle pipeline =
                    pass == RenderPass::Transparent ? forward_transparent_pipe : geometry_pipe;

                const U32 depth_key =
                    pass == RenderPass::Transparent
                        ? build_transparent_depth_key(submesh, model, camera_position,
                                                      safe_camera_forward)
                        : 0;

                DrawCall call =
                    build_draw_call(*mesh_data, submesh, pipeline, material.albedo, depth_key);

                call.transform_index = transform_index;
                call.material_index = material_index;

                submission.push_draw(pass, call);
                ++stats.visible_submeshes;
            }
        }

        return stats;
    }

    /**
     * @brief Submits shadow-casting mesh geometry.
     */
    static RenderSubmitStats submit_shadow_casters(World &world, RenderFrameSubmission &submission,
                                                   const AssetManager &assets,
                                                   RenderPipelineHandle shadow_pipe) noexcept {
        RenderSubmitStats stats{};

        for (auto [thing, mesh_part, trans_part] :
             world.query<MeshRendererPart, WorldTransformPart>()) {
            if (!mesh_part.visible || !mesh_part.casts_shadow) {
                continue;
            }

            if (!mesh_part.mesh_handle.is_valid()) {
                continue;
            }

            const RenderMeshData *mesh_data = assets.get_mesh_data(mesh_part.mesh_handle);
            if (!mesh_data) {
                continue;
            }

            const MaterialOverridePart *material_part = world.try_get<MaterialOverridePart>(thing);
            const Mat4 base_model = build_model_matrix(trans_part);

            for (USize i = 0; i < mesh_data->submeshes.size(); ++i) {
                const RenderSubMesh &submesh = mesh_data->submeshes[i];
                ++stats.total_submeshes;

                MaterialAssetHandle material_handle =
                    resolve_material_handle(mesh_part, material_part, i, assets);

                RenderMaterialPacket material = build_material_packet(material_handle, assets);
                const RenderPass pass = resolve_render_pass(submesh, material);

                if (!should_submit_to_shadow_pass(pass)) {
                    ++stats.skipped_submeshes;
                    continue;
                }

                const Mat4 model = base_model * submesh.transform;

                const U32 transform_index = submission.push_transform(model);
                const U32 material_index = submission.push_material(material);

                DrawCall call =
                    build_draw_call(*mesh_data, submesh, shadow_pipe, material.albedo, 0);

                call.transform_index = transform_index;
                call.material_index = material_index;

                submission.push_shadow_draw(call);
                ++stats.visible_submeshes;
            }
        }

        return stats;
    }

    /**
     * @brief Extracts point and spot lights.
     */
    static void submit_lights(World &world, RenderFrameSubmission &submission) noexcept {
        U32 point_shadow_count = 0;
        U32 spot_shadow_count = 0;

        for (auto [thing, light, trans] : world.query<PointLightPart, WorldTransformPart>()) {
            (void)thing;

            PointLightData data{};
            data.position = trans.position;
            data.radius = light.radius;
            data.color = light.color;
            data.intensity = light.intensity;
            data.shadow_index = -1;
            data.shadow_strength = glm::clamp(light.shadow_strength, 0.0f, 1.0f);
            data.shadow_bias = light.shadow_bias > 0.0f ? light.shadow_bias : 0.005f;
            data.padding = 0.0f;

            if (light.casts_shadow && light.radius > 0.001f &&
                point_shadow_count < static_cast<U32>(MAX_SHADOWED_POINT_LIGHTS)) {
                data.shadow_index = static_cast<S32>(point_shadow_count);

                PointShadowData shadow_data = build_point_shadow_data(trans.position, light.radius);

                submission.point_shadows.push_back(shadow_data);
                ++point_shadow_count;
            }

            submission.point_lights.push_back(data);
        }

        for (auto [thing, light, trans] : world.query<SpotLightPart, WorldTransformPart>()) {
            (void)thing;

            const F32 inner_angle = glm::clamp(light.inner_angle_deg, 0.1f, 89.0f);
            const F32 outer_angle = glm::clamp(light.outer_angle_deg, inner_angle + 0.1f, 89.5f);

            Vec3 direction = trans.rotation * Vec3(0.0f, 0.0f, -1.0f);
            direction = glm::length(direction) > 0.0001f ? glm::normalize(direction)
                                                         : Vec3(0.0f, 0.0f, -1.0f);

            SpotLightData data{};
            data.position_radius = Vec4(trans.position, light.radius);
            data.direction_intensity = Vec4(direction, light.intensity);
            data.color_inner_cos = Vec4(light.color, glm::cos(glm::radians(inner_angle)));

            F32 shadow_index = -1.0f;

            if (light.casts_shadow && light.radius > 0.001f &&
                spot_shadow_count < static_cast<U32>(MAX_SHADOWED_SPOT_LIGHTS)) {
                shadow_index = static_cast<F32>(spot_shadow_count);

                const F32 shadow_bias = light.shadow_bias > 0.0f ? light.shadow_bias : 0.002f;

                SpotShadowData shadow_data = build_spot_shadow_data(
                    trans.position, direction, light.radius, outer_angle, shadow_bias);

                submission.spot_shadows.push_back(shadow_data);
                ++spot_shadow_count;
            }

            data.shadow_params = Vec4(glm::cos(glm::radians(outer_angle)), shadow_index,
                                      glm::clamp(light.shadow_strength, 0.0f, 1.0f),
                                      light.shadow_bias > 0.0f ? light.shadow_bias : 0.002f);

            submission.spot_lights.push_back(data);
        }
    }

    /**
     * @brief Extracts directional lights with default shadow settings.
     */
    static void submit_directional_lights(World &world, RenderFrameSubmission &submission,
                                          const Vec3 &cam_pos, const Vec3 &cam_dir) noexcept {
        RenderDirectionalShadowSettings default_settings{};
        submit_directional_lights(world, submission, cam_pos, cam_dir, default_settings);
    }

    /**
     * @brief Extracts directional lights and builds cascade matrices.
     */
    static void
    submit_directional_lights(World &world, RenderFrameSubmission &submission, const Vec3 &cam_pos,
                              const Vec3 &cam_dir,
                              const RenderDirectionalShadowSettings &shadow_settings) noexcept {
        auto sanitize_positive = [](F32 value, F32 fallback) noexcept -> F32 {
            return value > 0.001f ? value : fallback;
        };

        Vec3 splits = shadow_settings.cascade_splits;
        Vec3 half_extents = shadow_settings.cascade_half_extents;
        Vec3 depth_ranges = shadow_settings.cascade_depth_ranges;

        splits.x = sanitize_positive(splits.x, 15.0f);
        splits.y = splits.y > splits.x + 0.001f ? splits.y : splits.x + 1.0f;
        splits.z = splits.z > splits.y + 0.001f ? splits.z : splits.y + 1.0f;

        half_extents.x = sanitize_positive(half_extents.x, 15.0f);
        half_extents.y = sanitize_positive(half_extents.y, half_extents.x);
        half_extents.z = sanitize_positive(half_extents.z, half_extents.y);

        depth_ranges.x = sanitize_positive(depth_ranges.x, 50.0f);
        depth_ranges.y = sanitize_positive(depth_ranges.y, depth_ranges.x);
        depth_ranges.z = sanitize_positive(depth_ranges.z, depth_ranges.y);

        Vec3 safe_cam_dir =
            glm::length(cam_dir) > 0.0001f ? glm::normalize(cam_dir) : Vec3(0.0f, 0.0f, -1.0f);

        for (auto [thing, light, trans] : world.query<DirectionalLightPart, WorldTransformPart>()) {
            (void)thing;

            DirectionalLightData data{};
            data.direction = glm::normalize(trans.rotation * Vec3(0.0f, 0.0f, -1.0f));
            data.intensity = light.intensity;
            data.color = light.color;
            data.cascade_splits = Vec4(splits.x, splits.y, splits.z, 0.0f);

            const F32 min_bias =
                shadow_settings.min_bias > 0.0f ? shadow_settings.min_bias : 0.0001f;
            const F32 slope_bias =
                shadow_settings.slope_bias > 0.0f ? shadow_settings.slope_bias : 0.0010f;
            const F32 cascade_bias_scale = shadow_settings.cascade_bias_scale >= 0.0f
                                               ? shadow_settings.cascade_bias_scale
                                               : 1.0f;
            const F32 shadow_strength = glm::clamp(shadow_settings.shadow_strength, 0.0f, 1.0f);

            data.shadow_params = Vec4(min_bias, slope_bias, cascade_bias_scale, shadow_strength);

            const F32 filter_radius = shadow_settings.filter_radius_texels > 0.0f
                                          ? shadow_settings.filter_radius_texels
                                          : 1.35f;

            const F32 cascade_filter_scale = shadow_settings.cascade_filter_scale >= 0.0f
                                                 ? shadow_settings.cascade_filter_scale
                                                 : 0.35f;

            data.shadow_filter_params = Vec4(filter_radius, cascade_filter_scale, 0.0f, 0.0f);

            const F32 split_values[DIRECTIONAL_CASCADE_COUNT] = {splits.x, splits.y, splits.z};
            const F32 extent_values[DIRECTIONAL_CASCADE_COUNT] = {half_extents.x, half_extents.y,
                                                                  half_extents.z};
            const F32 depth_values[DIRECTIONAL_CASCADE_COUNT] = {depth_ranges.x, depth_ranges.y,
                                                                 depth_ranges.z};

            for (USize i = 0; i < DIRECTIONAL_CASCADE_COUNT; ++i) {
                const F32 split_distance = split_values[i];
                const F32 extent = extent_values[i];
                const F32 depth_range = depth_values[i];

                Vec3 target = cam_pos + safe_cam_dir * (split_distance * 0.5f);

                Vec3 light_forward = data.direction;
                if (glm::length(light_forward) <= 0.0001f) {
                    light_forward = Vec3(0.0f, -1.0f, 0.0f);
                } else {
                    light_forward = glm::normalize(light_forward);
                }

                Vec3 world_up = Vec3(0.0f, 1.0f, 0.0f);
                if (glm::abs(glm::dot(world_up, light_forward)) > 0.95f) {
                    world_up = Vec3(1.0f, 0.0f, 0.0f);
                }

                Vec3 light_right = glm::normalize(glm::cross(world_up, light_forward));
                Vec3 light_up = glm::normalize(glm::cross(light_forward, light_right));

                constexpr F32 cascade_resolution = 2048.0f;
                const F32 texel_world_size = (extent * 2.0f) / cascade_resolution;

                Vec2 target_light_xy(glm::dot(target, light_right), glm::dot(target, light_up));

                Vec2 snapped_light_xy =
                    glm::round(target_light_xy / texel_world_size) * texel_world_size;

                target += light_right * (snapped_light_xy.x - target_light_xy.x);
                target += light_up * (snapped_light_xy.y - target_light_xy.y);

                Mat4 light_view =
                    glm::lookAt(target - light_forward * (depth_range * 0.5f), target, light_up);

                Mat4 light_proj =
                    glm::ortho(-extent, extent, -extent, extent, -depth_range, depth_range);

                data.light_view_proj[i] = light_proj * light_view;
            }

            submission.directional_lights.push_back(data);
        }
    }

private:
    static constexpr U32 VERTEX_STRIDE = 48;

    static bool should_submit_to_main_pass(RenderPass pass) noexcept {
        return pass == RenderPass::Opaque || pass == RenderPass::Masked ||
               pass == RenderPass::Transparent;
    }

    static bool should_submit_to_shadow_pass(RenderPass pass) noexcept {
        return pass == RenderPass::Opaque || pass == RenderPass::Masked;
    }

    /**
     * @brief Resolves the effective render pass for a submesh.
     *
     * @details
     * Masked and transparent material modes override the cooked submesh pass. Opaque material data
     * keeps the cooked pass so missing/fallback materials do not accidentally make transparent
     * submeshes visible in the deferred geometry pass.
     */
    static RenderPass resolve_render_pass(const RenderSubMesh &submesh,
                                          const RenderMaterialPacket &material) noexcept {
        const MaterialBlendMode blend_mode = static_cast<MaterialBlendMode>(material.blend_mode);

        if (blend_mode == MaterialBlendMode::Transparent) {
            return RenderPass::Transparent;
        }

        if (blend_mode == MaterialBlendMode::Masked) {
            return RenderPass::Masked;
        }

        return submesh.pass_type;
    }

    static Mat4 build_model_matrix(const WorldTransformPart &transform) noexcept {
        return transform.matrix;
    }

    static MaterialAssetHandle resolve_material_handle(const MeshRendererPart &mesh_part,
                                                       const MaterialOverridePart *material_part,
                                                       USize submesh_index,
                                                       const AssetManager &assets) noexcept {
        if (material_part && material_part->is_override_resolved()) {
            return material_part->material_handle;
        }

        return assets.get_mesh_submesh_material(mesh_part.mesh_handle, submesh_index);
    }

    static RenderMaterialPacket build_material_packet(MaterialAssetHandle material,
                                                      const AssetManager &assets) noexcept {
        RenderMaterialPacket packet{};

        const MaterialAssetData *data = assets.get_material_data(material);
        if (!data) {
            packet.base_color_factor = Vec4(1.0f);
            packet.shading_model = static_cast<U32>(MaterialShadingModel::PBR);
            packet.blend_mode = static_cast<U32>(MaterialBlendMode::Opaque);
            packet.texture_flags = 0;
            packet.alpha = 1.0f;
            packet.alpha_cutoff = 0.5f;
            packet.metallic_factor = 0.0f;
            packet.roughness_factor = 1.0f;
            return packet;
        }

        packet.albedo = assets.get_material_albedo_texture(material);
        packet.normal = assets.get_material_normal_texture(material);
        packet.extra = assets.get_material_extra_texture(material);

        packet.texture_flags = 0;

        if (packet.albedo.is_valid()) {
            packet.texture_flags |= RENDER_MATERIAL_HAS_ALBEDO;
        }

        if (packet.normal.is_valid()) {
            packet.texture_flags |= RENDER_MATERIAL_HAS_NORMAL;
        }

        if (packet.extra.is_valid()) {
            packet.texture_flags |= RENDER_MATERIAL_HAS_EXTRA;
        }

        packet.base_color_factor = data->base_color_factor;
        packet.shading_model = static_cast<U32>(data->shading_model);
        packet.blend_mode = static_cast<U32>(data->blend_mode);

        packet.alpha = data->alpha;
        packet.alpha_cutoff = data->alpha_cutoff;
        packet.metallic_factor = data->metallic_factor;
        packet.roughness_factor = data->roughness_factor;

        return packet;
    }

    static PointShadowData build_point_shadow_data(const Vec3 &position, F32 radius) noexcept {
        constexpr F32 near_plane = 0.1f;

        const F32 far_plane = radius > near_plane ? radius : near_plane + 0.001f;

        const Mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, near_plane, far_plane);

        const Vec3 directions[POINT_SHADOW_FACE_COUNT] = {
            Vec3(1.0f, 0.0f, 0.0f),  Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
            Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f),  Vec3(0.0f, 0.0f, -1.0f),
        };

        const Vec3 ups[POINT_SHADOW_FACE_COUNT] = {
            Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f),
            Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f),
        };

        PointShadowData data{};
        data.position_radius = Vec4(position, far_plane);

        for (USize face = 0; face < POINT_SHADOW_FACE_COUNT; ++face) {
            Mat4 view = glm::lookAt(position, position + directions[face], ups[face]);
            data.view_proj[face] = projection * view;
        }

        return data;
    }

    static SpotShadowData build_spot_shadow_data(const Vec3 &position, const Vec3 &direction,
                                                 F32 radius, F32 outer_angle_deg,
                                                 F32 shadow_bias) noexcept {
        constexpr F32 near_plane = 0.1f;

        const F32 far_plane = radius > near_plane ? radius : near_plane + 0.001f;
        const F32 fov = glm::clamp(outer_angle_deg * 2.0f, 1.0f, 175.0f);

        Vec3 safe_direction =
            glm::length(direction) > 0.0001f ? glm::normalize(direction) : Vec3(0.0f, 0.0f, -1.0f);

        Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(up, safe_direction)) > 0.95f) {
            up = Vec3(1.0f, 0.0f, 0.0f);
        }

        Mat4 view = glm::lookAt(position, position + safe_direction, up);
        Mat4 proj = glm::perspective(glm::radians(fov), 1.0f, near_plane, far_plane);

        SpotShadowData data{};
        data.view_proj = proj * view;
        data.position_radius = Vec4(position, far_plane);
        data.direction_bias = Vec4(safe_direction, shadow_bias);

        return data;
    }

    static bool is_submesh_visible(const Frustum &frustum, const RenderSubMesh &submesh,
                                   const Mat4 &model) noexcept {
        const Vec3 local_min = submesh.aabb_min;
        const Vec3 local_max = submesh.aabb_max;

        if (local_min.x > local_max.x || local_min.y > local_max.y || local_min.z > local_max.z) {
            return false;
        }

        const Vec3 corners[8] = {
            Vec3(local_min.x, local_min.y, local_min.z),
            Vec3(local_max.x, local_min.y, local_min.z),
            Vec3(local_min.x, local_max.y, local_min.z),
            Vec3(local_max.x, local_max.y, local_min.z),
            Vec3(local_min.x, local_min.y, local_max.z),
            Vec3(local_max.x, local_min.y, local_max.z),
            Vec3(local_min.x, local_max.y, local_max.z),
            Vec3(local_max.x, local_max.y, local_max.z),
        };

        Vec3 world_min(1.0e30f);
        Vec3 world_max(-1.0e30f);

        for (U32 i = 0; i < 8; ++i) {
            const Vec3 world_corner = Vec3(model * Vec4(corners[i], 1.0f));

            world_min = glm::min(world_min, world_corner);
            world_max = glm::max(world_max, world_corner);
        }

        return frustum.is_aabb_visible(world_min, world_max);
    }

    static U32 build_transparent_depth_key(const RenderSubMesh &submesh, const Mat4 &model,
                                           const Vec3 &camera_position,
                                           const Vec3 &camera_forward) noexcept {
        constexpr U32 MAX_DEPTH_KEY = 0xFFFFFu;
        constexpr F32 DEPTH_SCALE = 1024.0f;

        const Vec3 local_center = (submesh.aabb_min + submesh.aabb_max) * 0.5f;
        const Vec3 world_center = Vec3(model * Vec4(local_center, 1.0f));

        const F32 view_depth =
            glm::max(glm::dot(world_center - camera_position, camera_forward), 0.0f);

        const U32 quantized = static_cast<U32>(
            glm::clamp(view_depth * DEPTH_SCALE, 0.0f, static_cast<F32>(MAX_DEPTH_KEY)));

        /*
            Transparent draws must be rendered back-to-front. RenderSortKey sorts ascending, so
           invert quantized depth: farther objects receive smaller keys.
        */
        return MAX_DEPTH_KEY - quantized;
    }

    static DrawCall build_draw_call(const RenderMeshData &mesh, const RenderSubMesh &submesh,
                                    RenderPipelineHandle pipeline, TextureHandle sort_texture,
                                    U32 depth_key) noexcept {
        DrawCall call{};
        call.key = RenderSortKey::create(pipeline, mesh.vbo, sort_texture, depth_key);
        call.pipe = pipeline;

        call.vbo = mesh.vbo;
        call.ibo = mesh.ibo;

        call.index_count = submesh.index_count;
        call.index_offset = submesh.index_offset;
        call.vertex_offset = submesh.vertex_offset;

        call.vbo_stride = VERTEX_STRIDE;

        return call;
    }
};

} // namespace fr
