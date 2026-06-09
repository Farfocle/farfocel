/**
 * @file render_system.hpp
 * @author Tfoedy
 * @brief Handles scene-to-render-queue submission, geometric culling, and shadow cascade data.
 */

#pragma once

#include "fr/data/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/renderer/frustum.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/scene/camera_system.hpp"
#include "fr/scene/render_parts.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace fr {

/**
 * @brief Basic render statistics.
 */
struct RenderStats {
    U32 total_submeshes{0};
    U32 visible_submeshes{0};
    U32 culled_submeshes{0};
};

/**
 * @brief Camera data extracted from the ECS.
 */
struct CamData {
    glm::mat4 view_proj;
    glm::vec3 pos;
    glm::vec3 dir;
};

/**
 * @brief settings for cascaded directional shadows.
 *
 * @details
 * These values control how the directional light shadow cascades are generated and sampled.
 *
 * - cascade_splits:
 *   Distances used by the lighting shader to select a shadow cascade.
 *
 * - cascade_half_extents:
 *   Half-size of the orthographic shadow projection for each cascade.
 *
 * - cascade_depth_ranges:
 *   Depth range used by the light-space orthographic projection.
 *
 * - min_bias:
 *   Minimum depth bias used during shadow comparison.
 *
 * - slope_bias:
 *   Bias component that increases when the surface normal is nearly perpendicular to the light.
 *
 * - cascade_bias_scale:
 *   Additional multiplier applied per cascade. Farther cascades usually need slightly more bias.
 *
 * - shadow_strength:
 *   Final shadow intensity multiplier. 1.0 means full shadow strength.
 *
 * @note The current renderer stores three cascades in a 2x2 shadow atlas.
 */
struct DirectionalShadowSettings {
    glm::vec3 cascade_splits{15.0f, 50.0f, 150.0f};
    glm::vec3 cascade_half_extents{15.0f, 50.0f, 150.0f};
    glm::vec3 cascade_depth_ranges{50.0f, 150.0f, 300.0f};

    F32 min_bias{0.0001f};
    F32 slope_bias{0.0010f};
    F32 cascade_bias_scale{1.0f};
    F32 shadow_strength{1.0f};

    F32 filter_radius_texels{1.35f};
    F32 cascade_filter_scale{0.35f};
};

/**
 * @brief Converts ECS render parts into renderer queues.
 */
class RenderSystem {
public:
    /**
     * @brief Extracts the main camera view-projection matrix and camera vectors.
     *
     * @param world ECS world.
     * @param aspect_ratio Current viewport aspect ratio.
     * @return Camera data for the active main camera, or identity fallback if no camera exists.
     */
    static CamData extract_cam_data(World &world, F32 aspect_ratio) noexcept {
        for (auto [entity, cam, trans] : world.query<CameraPart, TransformPart>()) {
            (void)entity;

            if (cam.is_main) {
                glm::mat4 proj = glm::perspective(glm::radians(cam.fov), aspect_ratio,
                                                  cam.near_plane, cam.far_plane);

                glm::vec3 forward = trans.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up = trans.rotation * glm::vec3(0.0f, 1.0f, 0.0f);

                return {
                    proj * glm::lookAt(trans.position, trans.position + forward, up),
                    trans.position,
                    forward,
                };
            }
        }

        return {
            glm::mat4(1.0f),
            glm::vec3(0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f),
        };
    }

    /**
     * @brief Submits visible mesh submeshes to a render queue.
     *
     * @details
     * This path performs camera frustum culling against transformed submesh AABBs.
     * Transparent and UI submeshes are currently skipped because the renderer does not
     * have a forward transparent/UI pass yet.
     *
     * @param world ECS world.
     * @param queue Destination render queue.
     * @param assets Asset manager used to resolve mesh and texture handles.
     * @param default_pipe Pipeline used for submitted geometry.
     * @param view_proj Current camera view-projection matrix.
     * @return Render submission statistics.
     */
    static RenderStats submit_meshes(World &world, RenderQueue &queue, const AssetManager &assets,
                                     RenderPipelineHandle default_pipe,
                                     const glm::mat4 &view_proj) noexcept {
        RenderStats stats{};
        Frustum frustum = Frustum::extract_from_matrix(view_proj);

        for (auto [entity, mesh_part, mat_part, trans_part] :
             world.query<MeshPart, MaterialPart, TransformPart>()) {
            (void)entity;

            if (!mesh_part.handle.is_valid()) {
                continue;
            }

            const MeshData *mesh_data = assets.get_mesh_data(mesh_part.handle);
            if (!mesh_data) {
                continue;
            }

            const glm::mat4 base_model = build_model_matrix(trans_part);

            for (USize i = 0; i < mesh_data->submeshes.size(); ++i) {
                const SubMesh &submesh = mesh_data->submeshes[i];
                ++stats.total_submeshes;

                if (!should_submit_to_deferred_geometry(submesh.pass_type)) {
                    ++stats.culled_submeshes;
                    continue;
                }

                const glm::mat4 model = base_model * submesh.transform;

                if (!is_submesh_visible(frustum, submesh, model)) {
                    ++stats.culled_submeshes;
                    continue;
                }

                DrawCall call =
                    build_draw_call(*mesh_data, submesh, mat_part, assets, default_pipe, 0);

                queue.send_call(call, model);
                ++stats.visible_submeshes;
            }
        }

        return stats;
    }

    /**
     * @brief Submits mesh submeshes to a render queue without frustum culling.
     *
     * @details
     * This is useful for shadow rendering or debug paths where camera frustum culling
     * should not be applied.
     *
     * @param world ECS world.
     * @param queue Destination render queue.
     * @param assets Asset manager used to resolve mesh and texture handles.
     * @param default_pipe Pipeline used for submitted geometry.
     * @return Render submission statistics.
     */
    static RenderStats submit_meshes_nocull(World &world, RenderQueue &queue,
                                            const AssetManager &assets,
                                            RenderPipelineHandle default_pipe) noexcept {
        RenderStats stats{};

        for (auto [entity, mesh_part, mat_part, trans_part] :
             world.query<MeshPart, MaterialPart, TransformPart>()) {
            (void)entity;

            if (!mesh_part.handle.is_valid()) {
                continue;
            }

            const MeshData *mesh_data = assets.get_mesh_data(mesh_part.handle);
            if (!mesh_data) {
                continue;
            }

            const glm::mat4 base_model = build_model_matrix(trans_part);

            for (USize i = 0; i < mesh_data->submeshes.size(); ++i) {
                const SubMesh &submesh = mesh_data->submeshes[i];
                ++stats.total_submeshes;

                if (!should_submit_to_deferred_geometry(submesh.pass_type)) {
                    ++stats.culled_submeshes;
                    continue;
                }

                const glm::mat4 model = base_model * submesh.transform;

                DrawCall call =
                    build_draw_call(*mesh_data, submesh, mat_part, assets, default_pipe, 0);

                queue.send_call(call, model);
                ++stats.visible_submeshes;
            }
        }

        return stats;
    }

    /**
     * @brief Submits shadow-casting mesh submeshes to a render queue.
     *
     * @details
     * This path is dedicated to shadow map rendering. It intentionally does not use the
     * camera frustum for culling because shadow casters may be outside the camera view while
     * still casting visible shadows into it.
     *
     * @param world ECS world.
     * @param queue Destination shadow render queue.
     * @param assets Asset manager used to resolve mesh and texture handles.
     * @param shadow_pipe Pipeline used for shadow rendering.
     * @return Render submission statistics.
     */
    static RenderStats submit_shadow_casters(World &world, RenderQueue &queue,
                                             const AssetManager &assets,
                                             RenderPipelineHandle shadow_pipe) noexcept {
        RenderStats stats{};

        for (auto [entity, mesh_part, mat_part, trans_part] :
             world.query<MeshPart, MaterialPart, TransformPart>()) {
            (void)entity;

            if (!mesh_part.handle.is_valid()) {
                continue;
            }

            const MeshData *mesh_data = assets.get_mesh_data(mesh_part.handle);
            if (!mesh_data) {
                continue;
            }

            const glm::mat4 base_model = build_model_matrix(trans_part);

            for (USize i = 0; i < mesh_data->submeshes.size(); ++i) {
                const SubMesh &submesh = mesh_data->submeshes[i];
                ++stats.total_submeshes;

                if (!should_submit_to_shadow_pass(submesh.pass_type)) {
                    ++stats.culled_submeshes;
                    continue;
                }

                const glm::mat4 model = base_model * submesh.transform;

                DrawCall call =
                    build_draw_call(*mesh_data, submesh, mat_part, assets, shadow_pipe, 0);

                queue.send_call(call, model);
                ++stats.visible_submeshes;
            }
        }

        return stats;
    }

    /**
     * @brief Submits local lights to a render queue.
     *
     * @param world ECS world.
     * @param queue Destination render queue.
     */
    static void submit_lights(World &world, RenderQueue &queue) noexcept {
        U32 point_shadow_count = 0;
        U32 spot_shadow_count = 0;

        for (auto [entity, light, trans] : world.query<PointLightPart, TransformPart>()) {
            (void)entity;

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

                queue.send_point_shadow(shadow_data);
                ++point_shadow_count;
            }

            queue.send_point_light(data);
        }

        for (auto [entity, light, trans] : world.query<SpotLightPart, TransformPart>()) {
            (void)entity;

            const F32 inner_angle = glm::clamp(light.inner_angle_deg, 0.1f, 89.0f);
            const F32 outer_angle = glm::clamp(light.outer_angle_deg, inner_angle + 0.1f, 89.5f);

            glm::vec3 direction = trans.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            direction = glm::length(direction) > 0.0001f ? glm::normalize(direction)
                                                         : glm::vec3(0.0f, 0.0f, -1.0f);

            SpotLightData data{};
            data.position_radius = glm::vec4(trans.position, light.radius);
            data.direction_intensity = glm::vec4(direction, light.intensity);
            data.color_inner_cos = glm::vec4(light.color, glm::cos(glm::radians(inner_angle)));

            F32 shadow_index = -1.0f;

            if (light.casts_shadow && light.radius > 0.001f &&
                spot_shadow_count < static_cast<U32>(MAX_SHADOWED_SPOT_LIGHTS)) {
                shadow_index = static_cast<F32>(spot_shadow_count);

                const F32 shadow_bias = light.shadow_bias > 0.0f ? light.shadow_bias : 0.002f;

                SpotShadowData shadow_data = build_spot_shadow_data(
                    trans.position, direction, light.radius, outer_angle, shadow_bias);

                queue.send_spot_shadow(shadow_data);
                ++spot_shadow_count;
            }

            data.shadow_params = glm::vec4(glm::cos(glm::radians(outer_angle)), shadow_index,
                                           glm::clamp(light.shadow_strength, 0.0f, 1.0f),
                                           light.shadow_bias > 0.0f ? light.shadow_bias : 0.002f);

            queue.send_spot_light(data);
        }
    }

    /**
     * @brief Submits directional lights using default cascaded shadow settings.
     *
     * @param world ECS world.
     * @param queue Destination render queue.
     * @param cam_pos Current camera world position.
     * @param cam_dir Current camera forward direction.
     */
    static void submit_directional_lights(World &world, RenderQueue &queue,
                                          const glm::vec3 &cam_pos,
                                          const glm::vec3 &cam_dir) noexcept {
        DirectionalShadowSettings default_settings{};
        submit_directional_lights(world, queue, cam_pos, cam_dir, default_settings);
    }

    /**
     * @brief Submits directional lights and computes cascaded shadow matrices.
     *
     * @details
     * The current renderer uses three cascades packed into a 2x2 atlas. The shadow pass
     * renders cascade 0, 1, and 2 into three atlas quadrants.
     *
     * @param world ECS world.
     * @param queue Destination render queue.
     * @param cam_pos Current camera world position.
     * @param cam_dir Current camera forward direction.
     * @param shadow_settings Runtime shadow cascade settings.
     */
    static void
    submit_directional_lights(World &world, RenderQueue &queue, const glm::vec3 &cam_pos,
                              const glm::vec3 &cam_dir,
                              const DirectionalShadowSettings &shadow_settings) noexcept {
        auto sanitize_positive = [](F32 value, F32 fallback) noexcept -> F32 {
            return value > 0.001f ? value : fallback;
        };

        glm::vec3 splits = shadow_settings.cascade_splits;
        glm::vec3 half_extents = shadow_settings.cascade_half_extents;
        glm::vec3 depth_ranges = shadow_settings.cascade_depth_ranges;

        // Keep cascade values positive and ordered enough to avoid broken projections.
        splits.x = sanitize_positive(splits.x, 15.0f);
        splits.y = splits.y > splits.x + 0.001f ? splits.y : splits.x + 1.0f;
        splits.z = splits.z > splits.y + 0.001f ? splits.z : splits.y + 1.0f;

        half_extents.x = sanitize_positive(half_extents.x, 15.0f);
        half_extents.y = sanitize_positive(half_extents.y, half_extents.x);
        half_extents.z = sanitize_positive(half_extents.z, half_extents.y);

        depth_ranges.x = sanitize_positive(depth_ranges.x, 50.0f);
        depth_ranges.y = sanitize_positive(depth_ranges.y, depth_ranges.x);
        depth_ranges.z = sanitize_positive(depth_ranges.z, depth_ranges.y);

        glm::vec3 safe_cam_dir =
            glm::length(cam_dir) > 0.0001f ? glm::normalize(cam_dir) : glm::vec3(0.0f, 0.0f, -1.0f);

        for (auto [entity, light, trans] : world.query<DirectionalLightPart, TransformPart>()) {
            (void)entity;

            DirectionalLightData data{};
            data.direction = glm::normalize(trans.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
            data.intensity = light.intensity;
            data.color = light.color;
            data.cascade_splits = glm::vec4(splits.x, splits.y, splits.z, 0.0f);

            const F32 min_bias =
                shadow_settings.min_bias > 0.0f ? shadow_settings.min_bias : 0.0001f;
            const F32 slope_bias =
                shadow_settings.slope_bias > 0.0f ? shadow_settings.slope_bias : 0.0010f;
            const F32 cascade_bias_scale = shadow_settings.cascade_bias_scale >= 0.0f
                                               ? shadow_settings.cascade_bias_scale
                                               : 1.0f;
            const F32 shadow_strength = glm::clamp(shadow_settings.shadow_strength, 0.0f, 1.0f);

            data.shadow_params =
                glm::vec4(min_bias, slope_bias, cascade_bias_scale, shadow_strength);

            const F32 filter_radius = shadow_settings.filter_radius_texels > 0.0f
                                          ? shadow_settings.filter_radius_texels
                                          : 1.35f;

            const F32 cascade_filter_scale = shadow_settings.cascade_filter_scale >= 0.0f
                                                 ? shadow_settings.cascade_filter_scale
                                                 : 0.35f;

            data.shadow_filter_params = glm::vec4(filter_radius, cascade_filter_scale, 0.0f, 0.0f);

            const F32 split_values[3] = {splits.x, splits.y, splits.z};
            const F32 extent_values[3] = {half_extents.x, half_extents.y, half_extents.z};
            const F32 depth_values[3] = {depth_ranges.x, depth_ranges.y, depth_ranges.z};

            for (int i = 0; i < 3; ++i) {
                const F32 split_distance = split_values[i];
                const F32 extent = extent_values[i];
                const F32 depth_range = depth_values[i];

                glm::vec3 target = cam_pos + safe_cam_dir * (split_distance * 0.5f);

                glm::vec3 light_forward = data.direction;
                if (glm::length(light_forward) <= 0.0001f) {
                    light_forward = glm::vec3(0.0f, -1.0f, 0.0f);
                } else {
                    light_forward = glm::normalize(light_forward);
                }

                glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (glm::abs(glm::dot(world_up, light_forward)) > 0.95f) {
                    world_up = glm::vec3(1.0f, 0.0f, 0.0f);
                }

                glm::vec3 light_right = glm::normalize(glm::cross(world_up, light_forward));
                glm::vec3 light_up = glm::normalize(glm::cross(light_forward, light_right));

                /*
                    Snap the cascade center to the shadow texel grid in light space.
                    This reduces shadow swimming during small camera movements.
                */
                constexpr F32 cascade_resolution = 2048.0f;
                const F32 texel_world_size = (extent * 2.0f) / cascade_resolution;

                glm::vec2 target_light_xy(glm::dot(target, light_right),
                                          glm::dot(target, light_up));

                glm::vec2 snapped_light_xy =
                    glm::round(target_light_xy / texel_world_size) * texel_world_size;

                target += light_right * (snapped_light_xy.x - target_light_xy.x);
                target += light_up * (snapped_light_xy.y - target_light_xy.y);

                glm::mat4 light_view =
                    glm::lookAt(target - light_forward * (depth_range * 0.5f), target, light_up);

                glm::mat4 light_proj =
                    glm::ortho(-extent, extent, -extent, extent, -depth_range, depth_range);

                data.light_view_proj[i] = light_proj * light_view;
            }

            queue.send_directional_light(data);
        }
    }

private:
    static constexpr U32 k_vertex_stride = 48;

    /**
     * @brief Checks whether a pass type should be submitted to the deferred geometry pass.
     *
     * @param pass_type Submesh render pass type.
     * @return True for opaque and masked geometry.
     */
    static bool should_submit_to_deferred_geometry(RenderPassType pass_type) noexcept {
        return pass_type == RenderPassType::Opaque || pass_type == RenderPassType::Masked;
    }

    /**
     * @brief Builds a model matrix from a TransformPart.
     *
     * @param transform ECS transform part.
     * @return World-space model matrix.
     */
    static glm::mat4 build_model_matrix(const TransformPart &transform) noexcept {
        return glm::translate(glm::mat4(1.0f), transform.position) *
               glm::mat4_cast(transform.rotation) * glm::scale(glm::mat4(1.0f), transform.scale);
    }

    /**
     * @brief Builds cubemap view-projection matrices for one point light.
     *
     * @param position Point light world position.
     * @param radius Point light influence radius.
     * @return GPU-side point shadow data.
     */
    static PointShadowData build_point_shadow_data(const glm::vec3 &position, F32 radius) noexcept {
        constexpr F32 near_plane = 0.1f;

        const F32 far_plane = radius > near_plane ? radius : near_plane + 0.001f;

        const glm::mat4 projection =
            glm::perspective(glm::radians(90.0f), 1.0f, near_plane, far_plane);

        const glm::vec3 directions[POINT_SHADOW_FACE_COUNT] = {
            glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
        };

        const glm::vec3 ups[POINT_SHADOW_FACE_COUNT] = {
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        };

        PointShadowData data{};
        data.position_radius = glm::vec4(position, far_plane);

        for (USize face = 0; face < POINT_SHADOW_FACE_COUNT; ++face) {
            glm::mat4 view = glm::lookAt(position, position + directions[face], ups[face]);
            data.view_proj[face] = projection * view;
        }

        return data;
    }

    /**
     * @brief Builds a view-projection matrix for one spot light shadow.
     *
     * @param position Spot light world position.
     * @param direction Spot light world direction.
     * @param radius Spot light influence radius.
     * @param outer_angle_deg Outer cone angle in degrees.
     * @param shadow_bias Depth bias used by the lighting pass.
     * @return GPU-side spot shadow data.
     */
    static SpotShadowData build_spot_shadow_data(const glm::vec3 &position,
                                                 const glm::vec3 &direction, F32 radius,
                                                 F32 outer_angle_deg, F32 shadow_bias) noexcept {
        constexpr F32 near_plane = 0.1f;

        const F32 far_plane = radius > near_plane ? radius : near_plane + 0.001f;
        const F32 fov = glm::clamp(outer_angle_deg * 2.0f, 1.0f, 175.0f);

        glm::vec3 safe_direction = glm::length(direction) > 0.0001f ? glm::normalize(direction)
                                                                    : glm::vec3(0.0f, 0.0f, -1.0f);

        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(up, safe_direction)) > 0.95f) {
            up = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        glm::mat4 view = glm::lookAt(position, position + safe_direction, up);
        glm::mat4 proj = glm::perspective(glm::radians(fov), 1.0f, near_plane, far_plane);

        SpotShadowData data{};
        data.view_proj = proj * view;
        data.position_radius = glm::vec4(position, far_plane);
        data.direction_bias = glm::vec4(safe_direction, shadow_bias);

        return data;
    }

    /**
     * @brief Tests a transformed submesh AABB against the camera frustum.
     *
     * @param frustum Camera frustum.
     * @param submesh Submesh with local AABB.
     * @param model Submesh model matrix.
     * @return True if the transformed AABB is visible.
     */
    static bool is_submesh_visible(const Frustum &frustum, const SubMesh &submesh,
                                   const glm::mat4 &model) noexcept {
        glm::vec3 center = (submesh.aabb_min + submesh.aabb_max) * 0.5f;
        glm::vec3 extents = submesh.aabb_max - center;

        glm::vec3 transformed_center = glm::vec3(model * glm::vec4(center, 1.0f));

        glm::vec3 transformed_extents =
            glm::vec3(glm::abs(model[0][0]) * extents.x + glm::abs(model[1][0]) * extents.y +
                          glm::abs(model[2][0]) * extents.z,
                      glm::abs(model[0][1]) * extents.x + glm::abs(model[1][1]) * extents.y +
                          glm::abs(model[2][1]) * extents.z,
                      glm::abs(model[0][2]) * extents.x + glm::abs(model[1][2]) * extents.y +
                          glm::abs(model[2][2]) * extents.z);

        return frustum.is_aabb_visible(transformed_center - transformed_extents,
                                       transformed_center + transformed_extents);
    }

    /**
     * @brief Resolves the texture that should be used for a draw call.
     *
     * @details
     * The cooked submesh texture has priority. If it is missing, the material part texture
     * is used as a fallback. If both are missing, the returned handle is invalid and the
     * renderer-level fallback texture will be used later.
     *
     * @param submesh_texture Texture handle stored directly in the cooked submesh.
     * @param material_texture Texture asset handle stored in the ECS material part.
     * @param assets Asset manager used to resolve material texture assets.
     * @return GPU texture handle or invalid handle.
     */
    static TextureHandle resolve_texture(TextureHandle submesh_texture,
                                         TextureAssetHandle material_texture,
                                         const AssetManager &assets) noexcept {
        if (submesh_texture.is_valid()) {
            return submesh_texture;
        }

        return assets.get_texture_handle(material_texture);
    }

    /**
     * @brief Builds a DrawCall for a single submesh.
     *
     * @details
     * This function centralizes material texture resolution and draw-call setup so both
     * culled and non-culled submission paths produce identical packets.
     *
     * @param mesh Mesh GPU data.
     * @param submesh Submesh metadata.
     * @param material ECS material part.
     * @param assets Asset manager used to resolve material fallback textures.
     * @param pipeline Render pipeline used for the draw.
     * @param depth_key Encoded depth sorting value.
     * @return Fully initialized draw call.
     */
    static DrawCall build_draw_call(const MeshData &mesh, const SubMesh &submesh,
                                    const MaterialPart &material, const AssetManager &assets,
                                    RenderPipelineHandle pipeline, U32 depth_key) noexcept {
        TextureHandle albedo = resolve_texture(submesh.albedo_map, material.albedo_map, assets);
        TextureHandle normal = resolve_texture(submesh.normal_map, material.normal_map, assets);
        TextureHandle extra = resolve_texture(submesh.extra_map, material.extra_map, assets);

        DrawCall call{};
        call.key = SortKey::create(submesh.pass_type, pipeline, mesh.vbo, albedo, depth_key);
        call.pipe = pipeline;

        call.vbo = mesh.vbo;
        call.ibo = mesh.ibo;

        call.albedo_map = albedo;
        call.normal_map = normal;
        call.extra_map = extra;

        call.index_count = submesh.index_count;
        call.index_offset = submesh.index_offset;
        call.vertex_offset = submesh.vertex_offset;

        call.vbo_stride = k_vertex_stride;
        call.shading_model = static_cast<U32>(material.shading_model);

        return call;
    }

    /**
     * @brief Checks whether a pass type should be submitted to the shadow pass.
     *
     * @details
     * Opaque and masked geometry can safely write to the shadow map. Masked geometry still
     * needs its albedo texture because the shadow fragment shader uses alpha discard.
     *
     * Transparent and UI geometry are skipped until the renderer supports the proper pipeline.
     *
     * @param pass_type Submesh render pass type.
     * @return True if the submesh should be rendered into the shadow map.
     */
    static bool should_submit_to_shadow_pass(RenderPassType pass_type) noexcept {
        return pass_type == RenderPassType::Opaque || pass_type == RenderPassType::Masked;
    }
};

} // namespace fr
