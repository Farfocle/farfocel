/**
 * @file render_syste.hpp
 * @author Tfoedy
 * @brief Sample render system for bridging between ECS and Renderer
 *
 *
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include "fr/data/asset_manager.hpp"
#include "fr/data/world.hpp"
#include "fr/renderer/mesh.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_queue.hpp"
#include "fr/scene/components.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace fr {
class RenderSystem {
public:
    /**
     * @brief Computes the active camera projection matrices.
     * @param world ECS world context.
     * @param aspect_ratio Screen's width/height aspect ratio.
     * @return Generated 4x4 View-Projection matrix.
     */
    static glm::mat4 extract_cam_view_proj_mtx(fr::World &world, F32 aspect_ratio) noexcept {
        for (auto [thing, cam, trans] : world.query<CameraComponent, TransformComponent>()) {
            if (cam.is_main) {
                glm::vec3 forward = trans.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 up = trans.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                return glm::perspective(glm::radians(cam.fov), aspect_ratio, cam.near_plane,
                                        cam.far_plane) *
                       glm::lookAt(trans.position, trans.position + forward, up);
            }
        }
        return glm::mat4(1.0f);
    }
    /**
     * @brief Iterates ECS entities and sends their geometry and materials into the queue.
     * @param world ECS world context.
     * @param queue Optimization sorting queue for the GPU.
     * @param assets Global registry holding VRAM asset pointers.
     * @param gbuffer_pipe Rendering pipeline state intended for Geometry Pass.
     */
    static void submit_meshes(fr::World &world, fr::RenderQueue &queue, const AssetManager &assets,
                              RenderPipelineHandle gbuffer_pipe) noexcept {
        for (auto [thing, mesh_comp, mat_comp, trans] :
             world.query<MeshComponent, MaterialComponent, TransformComponent>()) {
            if (!mesh_comp.handle.is_valid())
                continue;

            const MeshData *data = assets.get_mesh_data(mesh_comp.handle);
            if (!data)
                continue;

            glm::mat4 model_mtx = glm::translate(glm::mat4(1.0f), trans.position);
            model_mtx *= glm::mat4_cast(trans.rotation);
            model_mtx = glm::scale(model_mtx, trans.scale);

            for (USize i = 0; i < data->submeshes.size(); ++i) {
                const SubMesh &sub = data->submeshes[i];
                if (sub.pass_type != RenderPassType::Opaque)
                    continue;

                DrawCall call{};
                call.key = SortKey::create(sub.pass_type, gbuffer_pipe, TextureHandle{}, 10);
                call.pipe = gbuffer_pipe;
                call.vbo = data->vbo;
                call.ibo = data->ibo;

                call.albedo_map = assets.get_texture_handle(mat_comp.albedo_map);
                call.normal_map = assets.get_texture_handle(mat_comp.normal_map);
                call.extra_map = assets.get_texture_handle(mat_comp.extra_map);

                call.shading_model = static_cast<U32>(mat_comp.shading_model);

                call.index_count = sub.index_count;
                call.index_offset = sub.index_offset;
                call.vertex_offset = sub.vertex_offset;
                call.vbo_stride = sizeof(Vertex);

                queue.send_call((call), model_mtx * sub.transform);
            }
        }
    }
};
} // namespace fr
