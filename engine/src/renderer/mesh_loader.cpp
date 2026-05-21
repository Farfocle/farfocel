#include "fr/renderer/mesh_loader.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include <cgltf.h>

namespace fr {

MeshData load_gltf(RenderDevice *device, StringView file_path) {
    FR_ASSERT(device != nullptr, "mesh_loader requires valid RenderDevice");

    MeshData out_data{};
    String fpath = String::from_view(file_path);

    cgltf_options options = {};
    cgltf_data *data = nullptr;

    if (cgltf_parse_file(&options, fpath.data(), &data) != cgltf_result_success) {
        return out_data;
    }

    if (cgltf_load_buffers(&options, data, fpath.data()) != cgltf_result_success) {
        cgltf_free(data);
        return out_data;
    }

    DynamicArray<Vertex> vertices;
    DynamicArray<U32> indices;

    for (cgltf_size i = 0; i < data->meshes_count; i++) {
        const cgltf_mesh &mesh = data->meshes[i];

        for (cgltf_size j = 0; j < mesh.primitives_count; ++j) {
            const cgltf_primitive &primitive = mesh.primitives[j];

            SubMesh submesh{};
            submesh.vertex_offset = static_cast<U32>(vertices.size());
            submesh.index_offset = static_cast<U32>(indices.size());

            // INDEX
            if (primitive.indices != nullptr) {
                const cgltf_accessor *acc = primitive.indices;
                submesh.index_count = static_cast<U32>(acc->count);
                indices.reserve(indices.size() + submesh.index_count);

                for (cgltf_size k = 0; k < acc->count; ++k) {
                    indices.push_back(static_cast<U32>(cgltf_accessor_read_index(acc, k)));
                }
            }

            // VERTEX
            const cgltf_accessor *pos_acc = nullptr;
            const cgltf_accessor *norm_acc = nullptr;
            const cgltf_accessor *uv_acc = nullptr;

            for (cgltf_size k = 0; k < primitive.attributes_count; ++k) {
                const cgltf_attribute &attr = primitive.attributes[k];
                if (attr.type == cgltf_attribute_type_position)
                    pos_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_normal)
                    norm_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord)
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
            out_data.submeshes.push_back(submesh);
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
