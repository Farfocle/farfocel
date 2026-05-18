// https://github.khronos.org/glTF-Tutorials/gltfTutorial/gltfTutorial_002_BasicGltfStructure.html
#include "fr/renderer/mesh.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"

#include <cgltf.h>

namespace fr {
bool Mesh::load(RenderDevice *device, StringView path) {
    m_device = device;

    String fpath = String::from_view(path);
    cgltf_options options = {};
    cgltf_data *data = nullptr;

    if (cgltf_parse_file(&options, fpath.data(), &data) != cgltf_result_success) {
        // log
        return false;
    }

    if (cgltf_load_buffers(&options, data, path.data()) != cgltf_result_success) {
        // log
        cgltf_free(data);
        return false;
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

            if (primitive.indices != nullptr) {
                const cgltf_accessor *acc = primitive.indices;
                submesh.index_count = static_cast<U32>(acc->count);

                for (cgltf_size k = 0; k < acc->count; ++k) {
                    U32 index = static_cast<U32>(cgltf_accessor_read_index(acc, k));
                    indices.push_back(index);
                }
            }

            U32 vertex_count = 0;
            const cgltf_accessor *pos_acc = nullptr;
            const cgltf_accessor *normal_acc = nullptr;
            const cgltf_accessor *texture_acc = nullptr;

            for (cgltf_size k = 0; k < primitive.attributes_count; ++k) {
                const cgltf_attribute &attr = primitive.attributes[k];
                if (attr.type == cgltf_attribute_type_position)
                    pos_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_normal)
                    normal_acc = attr.data;
                else if (attr.type == cgltf_attribute_type_texcoord)
                    texture_acc = attr.data;
            }

            if (pos_acc) {
                vertex_count = static_cast<U32>(pos_acc->count);
                for (cgltf_size k = 0; k < vertex_count; ++k) {
                    Vertex ver{};
                    cgltf_accessor_read_float(pos_acc, k, &ver.position.x, 3);
                    if (normal_acc)
                        cgltf_accessor_read_float(normal_acc, k, &ver.normal.x, 3);
                    if (texture_acc)
                        cgltf_accessor_read_float(texture_acc, k, &ver.uv.x, 2);

                    vertices.push_back(ver);
                }
            }
            m_submeshes.push_back(submesh);
        }
    }
    cgltf_free(data);
    Slice<const Byte> vertex_data(reinterpret_cast<const Byte *>(vertices.data()),
                                  vertices.size() * sizeof(Vertex));
    Slice<const Byte> index_data(reinterpret_cast<const Byte *>(indices.data()),
                                 indices.size() * sizeof(Vertex));
    m_vbo = m_device->create_buffer(vertex_data, false);
    m_ibo = m_device->create_buffer(index_data, false);

    return m_vbo.is_valid() && m_ibo.is_valid();
}
} // namespace fr
