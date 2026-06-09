/**
 * @file raw_mesh.hpp
 * @brief Intermediate structure for raw mesh data parsing.
 */
#pragma once
#include "fr/core/dynamic_array.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::asscooker {

struct RawVertex {
    F32 position[3];
    F32 normal[3];
    F32 uv[2];
    F32 tangent[4];
};

struct RawSubMesh {
    U32 index_count{0};
    U32 index_offset{0};
    U32 vertex_offset{0};
    U32 pass_type{0};
    F32 transform[16]{};

    String albedo_path;
    String normal_path;
    String extra_path;

    F32 aabb_min[3]{0.0f, 0.0f, 0.0f};
    F32 aabb_max[3]{0.0f, 0.0f, 0.0f};
};

struct RawMesh {
    DynamicArray<RawVertex> vertices;
    DynamicArray<U32> indices;
    DynamicArray<RawSubMesh> submeshes;

    F32 aabb_min[3]{0.0f, 0.0f, 0.0f};
    F32 aabb_max[3]{0.0f, 0.0f, 0.0f};
};

} // namespace fr::asscooker
