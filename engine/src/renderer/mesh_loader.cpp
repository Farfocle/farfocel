// THIS WILL BE REMOVED FROM THE ENGINE
// this is some very very bad code that will be rewritten in the asset loading program (cooker)
#include "fr/renderer/mesh_loader.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/data/asset_manager.hpp"
#include "fr/renderer/render_queue.hpp"
#include <cgltf.h>

#include <iostream>

namespace fr {

static String get_base_path(StringView filepath) {
    for (USize i = filepath.size(); i > 0; --i) {
        char c = filepath[i - 1];
        if (c == '/' || c == '\\') {
            return String(filepath.view_to(i - 1));
        }
    }
    return String::from_chars("");
}

MeshData load_mesh_gltf(AssetManager *am, RenderDevice *device, StringView file_path) {
    FR_ASSERT(device != nullptr, "mesh_loader requires valid RenderDevice");
    FR_ASSERT(am != nullptr, "mesh_loader requires valid AssetManager");

    MeshData out_data{};
    String fpath = String::from_view(file_path);
    String base_path = get_base_path(file_path);

    auto cgltf_result_to_string = [](cgltf_result result) {
        switch (result) {
        case cgltf_result_success:
            return "success";
        case cgltf_result_data_too_short:
            return "data_too_short";
        case cgltf_result_unknown_format:
            return "unknown_format";
        case cgltf_result_invalid_json:
            return "invalid_json";
        case cgltf_result_invalid_gltf:
            return "invalid_gltf";
        case cgltf_result_invalid_options:
            return "invalid_options";
        case cgltf_result_file_not_found:
            return "file_not_found";
        case cgltf_result_io_error:
            return "io_error";
        case cgltf_result_out_of_memory:
            return "out_of_memory";
        case cgltf_result_legacy_gltf:
            return "legacy_gltf";
        default:
            return "unknown";
        }
    };

    cgltf_options options = {};
    cgltf_data *data = nullptr;

    cgltf_result parse_result = cgltf_parse_file(&options, fpath.data(), &data);
    if (parse_result != cgltf_result_success) {
        std::cerr << "[mesh_loader] cgltf_parse_file failed: "
                  << cgltf_result_to_string(parse_result) << " (" << fpath.data() << ")\n";
        return out_data;
    }

    cgltf_result buffer_result = cgltf_load_buffers(&options, data, fpath.data());
    if (buffer_result != cgltf_result_success) {
        std::cerr << "[mesh_loader] cgltf_load_buffers failed: "
                  << cgltf_result_to_string(buffer_result) << " (" << fpath.data() << ")\n";
        cgltf_free(data);
        return out_data;
    }

    DynamicArray<Vertex> vertices(get_ambient_ctx().alloc);
    DynamicArray<U32> indices(get_ambient_ctx().alloc);

    struct MeshRange {
        U32 start_submesh;
        U32 count;
    };
    DynamicArray<MeshRange> mesh_ranges(get_ambient_ctx().alloc);
    DynamicArray<SubMesh> base_submeshes(get_ambient_ctx().alloc);

    // bullshit
    auto load_tex = [&](const cgltf_texture_view &tex_view, bool srgb) -> TextureHandle {
        if (tex_view.texture && tex_view.texture->image && tex_view.texture->image->uri) {
            String uri_str = String::from_chars(tex_view.texture->image->uri);
            String full_path = base_path + uri_str;

            TextureAssetHandle asset = am->load_texture(full_path.view(), srgb);
            return am->get_texture_handle(asset);
        }
        return TextureHandle{};
    };
    // end of bullshit

    for (cgltf_size i = 0; i < data->meshes_count; i++) {
        const cgltf_mesh &mesh = data->meshes[i];

        MeshRange range;
        range.start_submesh = static_cast<U32>(base_submeshes.size());
        range.count = static_cast<U32>(mesh.primitives_count);
        mesh_ranges.push_back(range);

        for (cgltf_size j = 0; j < mesh.primitives_count; ++j) {
            const cgltf_primitive &primitive = mesh.primitives[j];

            SubMesh submesh{};
            submesh.vertex_offset = static_cast<U32>(vertices.size());
            submesh.index_offset = static_cast<U32>(indices.size());

            submesh.pass_type = RenderPassType::Opaque;

            if (primitive.material != nullptr) {
                if (primitive.material->alpha_mode == cgltf_alpha_mode_mask) {
                    submesh.pass_type = RenderPassType::Masked;
                } else if (primitive.material->alpha_mode == cgltf_alpha_mode_blend) {
                    submesh.pass_type = RenderPassType::Transparent;
                }

                if (primitive.material->has_pbr_metallic_roughness) {
                    submesh.albedo_map = load_tex(
                        primitive.material->pbr_metallic_roughness.base_color_texture, true);
                    submesh.extra_map = load_tex(
                        primitive.material->pbr_metallic_roughness.metallic_roughness_texture,
                        false);
                }
                submesh.normal_map = load_tex(primitive.material->normal_texture, false);
            }

            if (primitive.indices != nullptr) {
                const cgltf_accessor *acc = primitive.indices;
                submesh.index_count = static_cast<U32>(acc->count);
                indices.reserve(indices.size() + submesh.index_count);

                for (cgltf_size k = 0; k < acc->count; ++k) {
                    indices.push_back(static_cast<U32>(cgltf_accessor_read_index(acc, k)));
                }
            }

            const cgltf_accessor *pos_acc = nullptr;
            const cgltf_accessor *norm_acc = nullptr;
            const cgltf_accessor *uv_acc = nullptr;

            for (cgltf_size k = 0; k < primitive.attributes_count; ++k) {
                const cgltf_attribute &attr = primitive.attributes[k];
                if (attr.type == cgltf_attribute_type_position)
                    pos_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_normal)
                    norm_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0)
                    uv_acc = attr.data;
            }

            if (pos_acc) {
                U32 vertex_count = static_cast<U32>(pos_acc->count);
                vertices.reserve(vertices.size() + vertex_count);

                for (cgltf_size k = 0; k < vertex_count; ++k) {
                    Vertex ver{};
                    cgltf_accessor_read_float(pos_acc, k, &ver.position.x, 3);
                    if (norm_acc)
                        cgltf_accessor_read_float(norm_acc, k, &ver.normal.x, 3);
                    if (uv_acc)
                        cgltf_accessor_read_float(uv_acc, k, &ver.uv.x, 2);
                    vertices.push_back(ver);
                }
            }
            base_submeshes.push_back(submesh);
        }
    }

    for (cgltf_size i = 0; i < data->nodes_count; i++) {
        cgltf_node *node = &data->nodes[i];
        if (!node->mesh)
            continue;

        cgltf_size mesh_index = node->mesh - data->meshes;

        glm::mat4 world_matrix;
        cgltf_node_transform_world(node, &world_matrix[0][0]);

        MeshRange range = mesh_ranges[mesh_index];
        for (U32 j = 0; j < range.count; ++j) {
            SubMesh instanced = base_submeshes[range.start_submesh + j];
            instanced.transform = world_matrix;
            out_data.submeshes.push_back(instanced);
        }
    }

    cgltf_free(data);

    Slice<const Byte> vertex_slice(reinterpret_cast<const Byte *>(vertices.data()),
                                   vertices.size() * sizeof(Vertex));
    Slice<const Byte> index_slice(reinterpret_cast<const Byte *>(indices.data()),
                                  indices.size() * sizeof(U32));

    out_data.vbo = device->create_buffer(vertex_slice, false);
    out_data.ibo = device->create_buffer(index_slice, false);

    return out_data;
}
} // namespace fr
