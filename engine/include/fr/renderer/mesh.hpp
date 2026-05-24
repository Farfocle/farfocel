#pragma once

#include "fr/core/dynamic_array.hpp"
#include "fr/core/string_view.hpp"
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
    glm::mat4 transform{1.0f};
};

struct MeshData {
    BufferHandle vbo{};
    BufferHandle ibo{};
    DynamicArray<SubMesh> submeshes{};
};

class Mesh {
public:
    Mesh() noexcept = default;
    ~Mesh() noexcept = default;

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    Mesh(Mesh &&) noexcept = default;
    Mesh &operator=(Mesh &&) noexcept = default;

    bool load(RenderDevice *device, StringView path);

private:
    RenderDevice *m_device{nullptr};
    BufferHandle m_vbo{};
    BufferHandle m_ibo{};
    DynamicArray<SubMesh> m_submeshes{};
};

} // namespace fr
