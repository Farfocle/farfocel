/**
 * @file mesh_compiler.cpp
 * @author Tfoedy
 * @brief Serializes RawMesh structures into .fmesh binary files.
 */

#include "mesh_compiler.hpp"
#include "fr/core/mem.hpp"
#include "fr/data/asset_format.hpp"
#include <cstdio>
#include <fstream>

namespace fr::asscooker {

/**
 * @brief Serializes an intermediate RawMesh into the cooked `.fmesh` binary format.
 *
 * @details
 * The resulting file layout is:
 *
 * - MeshHeader
 * - CookedSubMesh array
 * - CookedVertex array
 * - U32 index array
 * - optional null-terminated string block
 *
 * All write operations are validated. If any write fails, the function returns false.
 *
 * @param raw_mesh Source mesh in the intermediate cooker format.
 * @param output_path Destination path for the cooked `.fmesh` file.
 * @return True when the mesh was written successfully, false otherwise.
 */
bool compile_mesh(const RawMesh &raw_mesh, StringView output_path) {
    String out_path_str = String::from_view(output_path);

    std::ofstream file(out_path_str.data(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    auto write_items = [&](const void *data, USize item_size, USize item_count) -> bool {
        if (item_count == 0) {
            return true;
        }

        if (!data || item_size == 0) {
            return false;
        }

        file.write(static_cast<const char *>(data),
                   static_cast<std::streamsize>(item_size * item_count));

        return static_cast<bool>(file);
    };

    if (raw_mesh.vertices.is_empty() || raw_mesh.indices.is_empty() ||
        raw_mesh.submeshes.is_empty()) {
        return false;
    }

    USize string_block_capacity = 0;
    for (USize i = 0; i < raw_mesh.submeshes.size(); ++i) {
        const RawSubMesh &sub = raw_mesh.submeshes[i];

        if (sub.albedo_path.size() > 0) {
            string_block_capacity += sub.albedo_path.size() + 1;
        }

        if (sub.normal_path.size() > 0) {
            string_block_capacity += sub.normal_path.size() + 1;
        }

        if (sub.extra_path.size() > 0) {
            string_block_capacity += sub.extra_path.size() + 1;
        }
    }

    DynamicArray<char> string_table;
    string_table.reserve(string_block_capacity);

    auto add_string = [&](const String &str) -> U32 {
        if (str.size() == 0) {
            return 0xFFFFFFFF;
        }

        U32 offset = static_cast<U32>(string_table.size());
        USize current_size = string_table.size();

        string_table.grow_default(current_size + str.size() + 1);
        fr::mem::copy_raw_range(str.data(), str.size(), string_table.data() + current_size);
        string_table[current_size + str.size()] = '\0';

        return offset;
    };

    DynamicArray<CookedSubMesh> disk_submeshes;
    disk_submeshes.reserve(raw_mesh.submeshes.size());

    for (USize i = 0; i < raw_mesh.submeshes.size(); ++i) {
        const RawSubMesh &raw_sub = raw_mesh.submeshes[i];

        if (raw_sub.index_count == 0) {
            return false;
        }

        CookedSubMesh cooked{};
        cooked.index_count = raw_sub.index_count;
        cooked.index_offset = raw_sub.index_offset;
        cooked.vertex_offset = raw_sub.vertex_offset;
        cooked.pass_type = raw_sub.pass_type;

        fr::mem::copy_raw_range(raw_sub.transform, 16, cooked.transform);
        fr::mem::copy_raw_range(raw_sub.aabb_min, 3, cooked.aabb_min);
        fr::mem::copy_raw_range(raw_sub.aabb_max, 3, cooked.aabb_max);

        cooked.albedo_path_offset = add_string(raw_sub.albedo_path);
        cooked.normal_path_offset = add_string(raw_sub.normal_path);
        cooked.extra_path_offset = add_string(raw_sub.extra_path);

        disk_submeshes.push_back(cooked);
    }

    DynamicArray<CookedVertex> safe_vertices;
    safe_vertices.reserve(raw_mesh.vertices.size());

    for (USize i = 0; i < raw_mesh.vertices.size(); ++i) {
        const RawVertex &rv = raw_mesh.vertices[i];

        CookedVertex cv{};
        cv.position[0] = rv.position[0];
        cv.position[1] = rv.position[1];
        cv.position[2] = rv.position[2];

        cv.normal[0] = rv.normal[0];
        cv.normal[1] = rv.normal[1];
        cv.normal[2] = rv.normal[2];

        cv.uv[0] = rv.uv[0];
        cv.uv[1] = rv.uv[1];

        cv.tangent[0] = rv.tangent[0];
        cv.tangent[1] = rv.tangent[1];
        cv.tangent[2] = rv.tangent[2];
        cv.tangent[3] = rv.tangent[3];

        safe_vertices.push_back(cv);
    }

    MeshHeader header{};
    header.base.verify[0] = 'F';
    header.base.verify[1] = 'M';
    header.base.verify[2] = 'S';
    header.base.verify[3] = 'H';
    header.base.version = 1;

    header.vertex_count = static_cast<U32>(safe_vertices.size());
    header.index_count = static_cast<U32>(raw_mesh.indices.size());
    header.submesh_count = static_cast<U32>(disk_submeshes.size());

    header.vertex_data_size = header.vertex_count * sizeof(CookedVertex);
    header.index_data_size = header.index_count * sizeof(U32);
    header.submesh_data_size = header.submesh_count * sizeof(CookedSubMesh);
    header.string_block_size = static_cast<U32>(string_table.size());

    fr::mem::copy_raw_range(raw_mesh.aabb_min, 3, header.aabb_min);
    fr::mem::copy_raw_range(raw_mesh.aabb_max, 3, header.aabb_max);

    if (!write_items(&header, sizeof(MeshHeader), 1)) {
        return false;
    }

    if (!write_items(disk_submeshes.data(), sizeof(CookedSubMesh), header.submesh_count)) {
        return false;
    }

    if (!write_items(safe_vertices.data(), sizeof(CookedVertex), header.vertex_count)) {
        return false;
    }

    if (!write_items(raw_mesh.indices.data(), sizeof(U32), header.index_count)) {
        return false;
    }

    if (header.string_block_size > 0) {
        if (!write_items(string_table.data(), 1, header.string_block_size)) {
            return false;
        }
    }

    file.close();
    return static_cast<bool>(file);
}

} // namespace fr::asscooker
