#pragma once

#include "fr/core/dynamic_array.hpp"
#include "fr/renderer/render_device.hpp"
#include <glm/glm.hpp>

namespace fr {
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct SubMesh {
    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};
    // U32 materal_id; - PBR
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh() {
        if (m_vbo.is_valid() && m_device)
            m_device->destroy_buffer(m_vbo);
        if (m_ibo.is_valid() && m_device)
            m_device->destroy_buffer(m_ibo);
    }

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    Mesh(Mesh &&) noexcept = default;
    Mesh &operator=(Mesh &&) noexcept = default;

    // we will likely use gltf for model loading
    bool load(RenderDevice *device, StringView file_path);

    BufferHandle get_vbo() const {
        return m_vbo;
    }
    BufferHandle get_ibo() const {
        return m_ibo;
    }
    const DynamicArray<SubMesh> &get_submeshes() const {
        return m_submeshes;
    }

private:
    RenderDevice *m_device{nullptr};
    BufferHandle m_vbo;
    BufferHandle m_ibo;
    DynamicArray<SubMesh> m_submeshes;
};

} // namespace fr
