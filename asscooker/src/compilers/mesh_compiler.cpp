/**
 * @file mesh_compiler.cpp
 * @author Tfoedy
 * @brief Mesh asset compiler.
 */

#include "mesh_compiler.hpp"

#include "fr/asset/asset_format.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {
namespace {

template <typename T, USize N>
void copy_array(const T (&src)[N], T (&dst)[N]) noexcept {
    for (USize i = 0; i < N; ++i) {
        dst[i] = src[i];
    }
}

[[nodiscard]] bool checked_mul_u_size(USize a, USize b, USize &out) noexcept {
    if (a != 0 && b > static_cast<USize>(-1) / a) {
        return false;
    }

    out = a * b;
    return true;
}

[[nodiscard]] bool checked_add_u_size(USize a, USize b, USize &out) noexcept {
    if (b > static_cast<USize>(-1) - a) {
        return false;
    }

    out = a + b;
    return true;
}

void append_bytes(DynamicArray<Byte> &out, const void *data, USize size) noexcept {
    if (size == 0) {
        return;
    }

    FR_ASSERT(data, "source data must be non-null");

    const USize old_size = out.size();
    out.grow_default(old_size + size);

    fr::mem::copy_raw_range(reinterpret_cast<const Byte *>(data), size, out.data() + old_size);
}

[[nodiscard]] bool validate_mesh(const RawMesh &mesh, StringView output_path) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot compile mesh with empty output path.");
        return false;
    }

    if (mesh.vertices.is_empty()) {
        FR_LOG_ERR("[Cooker] Mesh has no vertices: {}", output_path);
        return false;
    }

    if (mesh.indices.is_empty()) {
        FR_LOG_ERR("[Cooker] Mesh has no indices: {}", output_path);
        return false;
    }

    if (mesh.submeshes.is_empty()) {
        FR_LOG_ERR("[Cooker] Mesh has no submeshes: {}", output_path);
        return false;
    }

    if (mesh.vertices.size() > static_cast<USize>(0xFFFFFFFFu) ||
        mesh.indices.size() > static_cast<USize>(0xFFFFFFFFu) ||
        mesh.submeshes.size() > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Mesh is too large for `.fmesh`: {}", output_path);
        return false;
    }

    for (USize i = 0; i < mesh.submeshes.size(); ++i) {
        const RawSubMesh &submesh = mesh.submeshes[i];

        if (submesh.index_count == 0) {
            FR_LOG_ERR("[Cooker] Mesh submesh {} has zero index count: {}", i, output_path);
            return false;
        }

        if (submesh.pass_type > 2) {
            FR_LOG_ERR("[Cooker] Mesh submesh {} has invalid pass type {}.", i, submesh.pass_type);
            return false;
        }

        const USize index_begin = static_cast<USize>(submesh.index_offset);
        const USize index_count = static_cast<USize>(submesh.index_count);

        if (index_begin >= mesh.indices.size()) {
            FR_LOG_ERR("[Cooker] Mesh submesh {} index offset is out of bounds: {}", i,
                       output_path);
            return false;
        }

        if (index_count > mesh.indices.size() - index_begin) {
            FR_LOG_ERR("[Cooker] Mesh submesh {} index range is out of bounds: {}", i, output_path);
            return false;
        }

        const USize vertex_offset = static_cast<USize>(submesh.vertex_offset);
        if (vertex_offset >= mesh.vertices.size()) {
            FR_LOG_ERR("[Cooker] Mesh submesh {} vertex offset is out of bounds: {}", i,
                       output_path);
            return false;
        }

        const USize local_vertex_count = mesh.vertices.size() - vertex_offset;
        const USize index_end = index_begin + index_count;

        for (USize index = index_begin; index < index_end; ++index) {
            const USize local_index = static_cast<USize>(mesh.indices[index]);

            if (local_index >= local_vertex_count) {
                FR_LOG_ERR("[Cooker] Mesh submesh {} references vertex out of bounds: {}", i,
                           output_path);
                return false;
            }
        }
    }

    return true;
}

[[nodiscard]] CookedSubMesh make_cooked_submesh(const RawSubMesh &raw_submesh) noexcept {
    CookedSubMesh cooked{};
    cooked.index_count = raw_submesh.index_count;
    cooked.index_offset = raw_submesh.index_offset;
    cooked.vertex_offset = raw_submesh.vertex_offset;
    cooked.pass_type = raw_submesh.pass_type;
    cooked.material_id = raw_submesh.material_id;

    copy_array(raw_submesh.transform, cooked.transform);
    copy_array(raw_submesh.aabb_min, cooked.aabb_min);
    copy_array(raw_submesh.aabb_max, cooked.aabb_max);

    return cooked;
}

[[nodiscard]] CookedVertex make_cooked_vertex(const RawVertex &raw_vertex) noexcept {
    CookedVertex cooked{};

    copy_array(raw_vertex.position, cooked.position);
    copy_array(raw_vertex.normal, cooked.normal);
    copy_array(raw_vertex.uv, cooked.uv);
    copy_array(raw_vertex.tangent, cooked.tangent);

    return cooked;
}

void append_cooked_submeshes(DynamicArray<Byte> &out, const RawMesh &raw_mesh) noexcept {
    for (USize i = 0; i < raw_mesh.submeshes.size(); ++i) {
        const CookedSubMesh cooked = make_cooked_submesh(raw_mesh.submeshes[i]);
        append_bytes(out, &cooked, sizeof(CookedSubMesh));
    }
}

void append_cooked_vertices(DynamicArray<Byte> &out, const RawMesh &raw_mesh) noexcept {
    for (USize i = 0; i < raw_mesh.vertices.size(); ++i) {
        const CookedVertex cooked = make_cooked_vertex(raw_mesh.vertices[i]);
        append_bytes(out, &cooked, sizeof(CookedVertex));
    }
}

[[nodiscard]] bool append_cooked_indices(DynamicArray<Byte> &out, const RawMesh &raw_mesh,
                                         StringView output_path) noexcept {
    USize index_data_size = 0;
    if (!checked_mul_u_size(raw_mesh.indices.size(), sizeof(U32), index_data_size)) {
        FR_LOG_ERR("[Cooker] Mesh index data size overflow: {}", output_path);
        return false;
    }

    append_bytes(out, raw_mesh.indices.data(), index_data_size);
    return true;
}

} // namespace

bool compile_mesh(const RawMesh &raw_mesh, StringView output_path) noexcept {
    if (!validate_mesh(raw_mesh, output_path)) {
        return false;
    }

    const USize vertex_count = raw_mesh.vertices.size();
    const USize index_count = raw_mesh.indices.size();
    const USize submesh_count = raw_mesh.submeshes.size();

    USize vertex_data_size = 0;
    USize index_data_size = 0;
    USize submesh_data_size = 0;

    if (!checked_mul_u_size(vertex_count, sizeof(CookedVertex), vertex_data_size) ||
        !checked_mul_u_size(index_count, sizeof(U32), index_data_size) ||
        !checked_mul_u_size(submesh_count, sizeof(CookedSubMesh), submesh_data_size)) {
        FR_LOG_ERR("[Cooker] Mesh binary section size overflow: {}", output_path);
        return false;
    }

    if (vertex_data_size > static_cast<USize>(0xFFFFFFFFu) ||
        index_data_size > static_cast<USize>(0xFFFFFFFFu) ||
        submesh_data_size > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Mesh binary sections are too large for `.fmesh` header: {}",
                   output_path);
        return false;
    }

    USize output_size = 0;
    if (!checked_add_u_size(sizeof(CookedMeshHeader), submesh_data_size, output_size) ||
        !checked_add_u_size(output_size, vertex_data_size, output_size) ||
        !checked_add_u_size(output_size, index_data_size, output_size)) {
        FR_LOG_ERR("[Cooker] Mesh output size overflow: {}", output_path);
        return false;
    }

    CookedMeshHeader header{};
    header.base.verify[0] = 'F';
    header.base.verify[1] = 'M';
    header.base.verify[2] = 'S';
    header.base.verify[3] = 'H';
    header.base.version = 2;

    header.vertex_count = static_cast<U32>(vertex_count);
    header.index_count = static_cast<U32>(index_count);
    header.submesh_count = static_cast<U32>(submesh_count);

    header.vertex_data_size = static_cast<U32>(vertex_data_size);
    header.index_data_size = static_cast<U32>(index_data_size);
    header.submesh_data_size = static_cast<U32>(submesh_data_size);
    header.reserved0 = 0;

    copy_array(raw_mesh.aabb_min, header.aabb_min);
    copy_array(raw_mesh.aabb_max, header.aabb_max);

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    DynamicArray<Byte> output(alloc);
    output.reserve(output_size);

    /*
        Runtime loader reads .fmesh in this exact order:
        header -> submeshes -> vertices -> indices.
    */
    append_bytes(output, &header, sizeof(CookedMeshHeader));
    append_cooked_submeshes(output, raw_mesh);
    append_cooked_vertices(output, raw_mesh);

    if (!append_cooked_indices(output, raw_mesh, output_path)) {
        return false;
    }

    if (output.size() != output_size) {
        FR_LOG_ERR("[Cooker] Mesh output size mismatch. Expected {}, got {}: {}", output_size,
                   output.size(), output_path);
        return false;
    }

    String out_path = String::from_view(alloc, output_path);

    if (!file::ensure_parent_directory(out_path.view())) {
        FR_LOG_ERR("[Cooker] Failed to create cooked mesh output directory: {}", output_path);
        return false;
    }

    if (!file::write_all_bytes(out_path, Slice<const Byte>(output.data(), output.size()))) {
        FR_LOG_ERR("[Cooker] Failed to write cooked mesh: {}", output_path);
        return false;
    }

    FR_LOG("[Cooker] Wrote cooked mesh: {}", output_path);
    return true;
}

} // namespace fr::asscooker
