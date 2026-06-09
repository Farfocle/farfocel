/**
 * @file asset_manager.hpp
 * @author Tfoedy
 * @brief Asset manager.
 */
#pragma once

#include <fstream>

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/strong_handle.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/data/asset_format.hpp"
#include "fr/renderer/mesh.hpp"
#include "fr/renderer/render_device.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace fr {

struct MeshAssetTag {};
using MeshAssetHandle = StrongHandle<MeshAssetTag>;

struct TextureAssetTag {};
using TextureAssetHandle = StrongHandle<TextureAssetTag>;

struct MeshAsset {
    MeshData data{};
    U32 ref_count{0};
    U64 path_hash{0};
    DynamicArray<TextureAssetHandle> texture_deps{};
};

struct TextureAsset {
    TextureHandle handle{};
    U32 ref_count{0};
    U64 path_hash{0};
};

class AssetManager {
private:
    // WARNING, THIS IS TEMPORARY AI SLOP
    // the reason is that we still lack our own file handling in our stl library
    static bool read_exact(std::ifstream &file, void *dst, USize size) noexcept {
        if (size == 0) {
            return true;
        }

        if (!dst) {
            return false;
        }

        file.read(static_cast<char *>(dst), static_cast<std::streamsize>(size));
        return static_cast<bool>(file);
    }

    template <typename T>
    static bool read_object(std::ifstream &file, T &out) noexcept {
        return read_exact(file, &out, sizeof(T));
    }

    template <typename T>
    static bool read_array(std::ifstream &file, T *dst, USize count) noexcept {
        return read_exact(file, dst, count * sizeof(T));
    }

    // END OF TEMPORARY AI SLOP

public:
    explicit AssetManager(RenderDevice *device, Alloc *alloc) noexcept
        : m_device(device),
          m_alloc(alloc),
          m_mesh_cache(HashMap<U64, MeshAssetHandle>::with_capacity(alloc, 1024)),
          m_texture_cache(HashMap<U64, TextureAssetHandle>::with_capacity(alloc, 2048)) {
    }

    ~AssetManager() noexcept {
        for (auto pair : m_mesh_cache) {
            MeshAssetHandle handle = pair.second();
            MeshAsset *record = m_meshes.get_data_unsafe(handle.key);
            if (record && record->data.vbo.is_valid()) {
                m_device->destroy_buffer(record->data.vbo);
                m_device->destroy_buffer(record->data.ibo);
            }
        }
        for (auto pair : m_texture_cache) {
            TextureAssetHandle handle = pair.second();
            TextureAsset *record = m_textures.get_data_unsafe(handle.key);
            if (record && record->handle.is_valid()) {
                m_device->destroy_texture(record->handle);
            }
        }
    }

    /**
     * @brief Loads a cooked mesh asset and uploads it to the GPU.
     *
     * @details
     * This function handles asset-level responsibilities:
     *
     * - cache lookup,
     * - CPU-side `.fmesh` loading,
     * - GPU buffer creation,
     * - texture dependency loading,
     * - asset registration,
     * - reference counting.
     *
     * File validation and payload reading are delegated to load_mesh_file().
     *
     * @param file_path Path to the cooked `.fmesh` file.
     * @return Valid MeshAssetHandle on success, invalid handle on failure.
     */
    MeshAssetHandle load_mesh(StringView file_path) noexcept {
        const U64 path_hash = file_path.hash().value;

        if (auto cached_opt = m_mesh_cache.find(path_hash); cached_opt.is_some()) {
            MeshAssetHandle handle = *cached_opt.unwrap();

            if (MeshAsset *record = m_meshes.get_data(handle.key)) {
                ++record->ref_count;
                return handle;
            }

            // Defensive cleanup for stale cache entries.
            m_mesh_cache.remove(path_hash);
        }

        LoadedMeshFile loaded(m_alloc);
        if (!load_mesh_file(file_path, loaded)) {
            return MeshAssetHandle{};
        }

        MeshData mesh_data{};

        mesh_data.vbo = m_device->create_buffer(
            Slice<const Byte>(reinterpret_cast<const Byte *>(loaded.vertices.data()),
                              loaded.vertices.size() * sizeof(CookedVertex)),
            false);

        mesh_data.ibo = m_device->create_buffer(
            Slice<const Byte>(reinterpret_cast<const Byte *>(loaded.indices.data()),
                              loaded.indices.size() * sizeof(U32)),
            false);

        mesh_data.aabb_min = loaded.aabb_min;
        mesh_data.aabb_max = loaded.aabb_max;

        if (!mesh_data.vbo.is_valid() || !mesh_data.ibo.is_valid()) {
            if (mesh_data.vbo.is_valid()) {
                m_device->destroy_buffer(mesh_data.vbo);
            }

            if (mesh_data.ibo.is_valid()) {
                m_device->destroy_buffer(mesh_data.ibo);
            }

            return MeshAssetHandle{};
        }

        auto get_str = [&](U32 offset) -> StringView {
            if (offset == 0xFFFFFFFF || offset >= loaded.string_block.size()) {
                return "";
            }

            return StringView(loaded.string_block.data() + offset);
        };

        DynamicArray<TextureAssetHandle> deps(m_alloc);

        mesh_data.submeshes.reserve(loaded.submeshes.size());

        for (USize i = 0; i < loaded.submeshes.size(); ++i) {
            const CookedSubMesh &cooked = loaded.submeshes[i];

            SubMesh submesh{};
            submesh.index_count = cooked.index_count;
            submesh.index_offset = cooked.index_offset;
            submesh.vertex_offset = cooked.vertex_offset;

            submesh.transform = glm::make_mat4(cooked.transform);

            submesh.aabb_min =
                glm::vec3(cooked.aabb_min[0], cooked.aabb_min[1], cooked.aabb_min[2]);
            submesh.aabb_max =
                glm::vec3(cooked.aabb_max[0], cooked.aabb_max[1], cooked.aabb_max[2]);

            if (cooked.pass_type == 0) {
                submesh.pass_type = RenderPassType::Opaque;
            } else if (cooked.pass_type == 1) {
                submesh.pass_type = RenderPassType::Masked;
            } else {
                submesh.pass_type = RenderPassType::Transparent;
            }

            StringView albedo_path = get_str(cooked.albedo_path_offset);
            if (!albedo_path.is_empty()) {
                TextureAssetHandle texture = load_texture(albedo_path);

                if (texture.is_valid()) {
                    deps.push_back(texture);
                }

                submesh.albedo_map = get_texture_handle(texture);
            }

            StringView normal_path = get_str(cooked.normal_path_offset);
            if (!normal_path.is_empty()) {
                TextureAssetHandle texture = load_texture(normal_path);

                if (texture.is_valid()) {
                    deps.push_back(texture);
                }

                submesh.normal_map = get_texture_handle(texture);
            }

            StringView extra_path = get_str(cooked.extra_path_offset);
            if (!extra_path.is_empty()) {
                TextureAssetHandle texture = load_texture(extra_path);

                if (texture.is_valid()) {
                    deps.push_back(texture);
                }

                submesh.extra_map = get_texture_handle(texture);
            }

            mesh_data.submeshes.push_back(submesh);
        }

        MeshAsset new_asset{};
        new_asset.data = std::move(mesh_data);
        new_asset.ref_count = 1;
        new_asset.path_hash = path_hash;
        new_asset.texture_deps = std::move(deps);

        MeshAssetHandle handle{m_meshes.add(std::move(new_asset))};
        m_mesh_cache.insert(path_hash, handle);

        return handle;
    }

    /**
     * @brief Loads a cooked texture asset and uploads it to the GPU.
     *
     * @details
     * This function handles asset-level responsibilities:
     *
     * - cache lookup,
     * - CPU-side `.ftex` loading,
     * - GPU texture creation,
     * - asset registration,
     * - reference counting.
     *
     * File validation and payload reading are delegated to load_texture_file().
     *
     * @param path Path to the cooked `.ftex` file.
     * @return Valid TextureAssetHandle on success, invalid handle on failure.
     */
    TextureAssetHandle load_texture(StringView path) noexcept {
        const U64 path_hash = path.hash().value;

        if (auto cached_opt = m_texture_cache.find(path_hash); cached_opt.is_some()) {
            TextureAssetHandle handle = *cached_opt.unwrap();

            if (TextureAsset *record = m_textures.get_data(handle.key)) {
                ++record->ref_count;
                return handle;
            }

            // Defensive cleanup for stale cache entries.
            m_texture_cache.remove(path_hash);
        }

        LoadedTextureFile loaded(m_alloc);
        if (!load_texture_file(path, loaded)) {
            return TextureAssetHandle{};
        }

        TextureHandle gpu_handle = m_device->create_texture_2d(
            loaded.width, loaded.height, loaded.mip_levels, loaded.gpu_format,
            Slice<const Byte>(loaded.pixels.data(), loaded.pixels.size()));

        if (!gpu_handle.is_valid()) {
            return TextureAssetHandle{};
        }

        TextureAsset new_asset{};
        new_asset.handle = gpu_handle;
        new_asset.ref_count = 1;
        new_asset.path_hash = path_hash;

        TextureAssetHandle handle{m_textures.add(new_asset)};
        m_texture_cache.insert(path_hash, handle);

        return handle;
    }

    /**
     * @brief Releases a mesh asset reference and destroys it when the reference count reaches zero.
     *
     * @details
     * This function uses a safe SlotMap lookup. Asset handles can outlive their records if
     * the asset was already unloaded, so unsafe access must not be used here.
     *
     * @param handle Mesh asset handle to release.
     */
    void unload_mesh(MeshAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return;
        }

        MeshAsset *record = m_meshes.get_data(handle.key);
        if (!record) {
            return;
        }

        if (record->ref_count == 0) {
            return;
        }

        --record->ref_count;

        if (record->ref_count != 0) {
            return;
        }

        if (record->data.vbo.is_valid()) {
            m_device->destroy_buffer(record->data.vbo);
            record->data.vbo = {};
        }

        if (record->data.ibo.is_valid()) {
            m_device->destroy_buffer(record->data.ibo);
            record->data.ibo = {};
        }

        for (USize i = 0; i < record->texture_deps.size(); ++i) {
            unload_texture(record->texture_deps[i]);
        }

        m_mesh_cache.remove(record->path_hash);
        m_meshes.erase(handle.key);
    }

    void unload_texture(TextureAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return;
        }

        TextureAsset *record = m_textures.get_data(handle.key);
        if (!record) {
            return;
        }

        if (record->ref_count == 0) {
            return;
        }

        --record->ref_count;

        if (record->ref_count != 0) {
            return;
        }

        if (record->handle.is_valid()) {
            m_device->destroy_texture(record->handle);
            record->handle = {};
        }

        m_texture_cache.remove(record->path_hash);
        m_textures.erase(handle.key);
    }

    [[nodiscard]] const MeshData *get_mesh_data(MeshAssetHandle handle) const noexcept {
        const MeshAsset *record = m_meshes.get_data(handle.key);
        return record ? &record->data : nullptr;
    }

    [[nodiscard]] TextureHandle get_texture_handle(TextureAssetHandle handle) const noexcept {
        const TextureAsset *record = m_textures.get_data(handle.key);
        return record ? record->handle : TextureHandle{};
    }

private:
    /**
     * @brief CPU-side representation of a loaded cooked texture file.
     *
     * @details
     * This structure stores validated texture data read from a `.ftex` file before it is
     * uploaded to the GPU. It intentionally does not contain any GPU handle.
     */
    struct LoadedTextureFile {
        U32 width{0};
        U32 height{0};
        U32 mip_levels{1};
        TextureFormat gpu_format{TextureFormat::R8G8B8A8_UNorm};
        DynamicArray<Byte> pixels;

        explicit LoadedTextureFile(Alloc *alloc) noexcept
            : pixels(alloc) {
        }
    };

    /**
     * @brief CPU-side representation of a loaded cooked mesh file.
     *
     * @details
     * This structure stores validated mesh data read from a `.fmesh` file before it is
     * uploaded to the GPU. It intentionally does not contain any GPU handles.
     */
    struct LoadedMeshFile {
        DynamicArray<CookedSubMesh> submeshes;
        DynamicArray<CookedVertex> vertices;
        DynamicArray<U32> indices;
        DynamicArray<char> string_block;

        glm::vec3 aabb_min{0.0f};
        glm::vec3 aabb_max{0.0f};

        explicit LoadedMeshFile(Alloc *alloc) noexcept
            : submeshes(alloc),
              vertices(alloc),
              indices(alloc),
              string_block(alloc) {
        }
    };

    /**
     * @brief Loads and validates a cooked `.ftex` file into CPU memory.
     *
     * @details
     * This function performs only file loading and validation. It does not create GPU
     * resources and does not modify the asset cache.
     *
     * Expected file layout:
     *
     * - TextureHeader
     * - base-level image payload
     *
     * Runtime mipmaps are generated by the render backend if more than one mip level is
     * requested.
     *
     * @param path Path to the cooked `.ftex` file.
     * @param out Loaded CPU-side texture file data.
     * @return True on success, false on validation or IO failure.
     */
    bool load_texture_file(StringView path, LoadedTextureFile &out) noexcept {
        String path_str = String::from_view(path);

        std::ifstream file(path_str.data(), std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        TextureHeader header{};
        if (!read_object(file, header)) {
            return false;
        }

        if (header.base.verify[0] != 'F' || header.base.verify[1] != 'T' ||
            header.base.verify[2] != 'E' || header.base.verify[3] != 'X') {
            return false;
        }

        if (header.base.version != 1) {
            return false;
        }

        if (header.width == 0 || header.height == 0 || header.image_data_size == 0 ||
            header.mip_levels == 0) {
            return false;
        }

        TextureFormat gpu_format = TextureFormat::R8G8B8A8_UNorm;
        USize expected_pixel_size = 4;

        if (header.format == AssetTextureFormat::RGBA8_UNORM) {
            gpu_format = TextureFormat::R8G8B8A8_UNorm;
            expected_pixel_size = 4;
        } else if (header.format == AssetTextureFormat::RGBA8_SRGB) {
            gpu_format = TextureFormat::R8G8B8A8_SRGB;
            expected_pixel_size = 4;
        } else if (header.format == AssetTextureFormat::RGBA32F_HDR) {
            gpu_format = TextureFormat::R32G32B32A32_Float;
            expected_pixel_size = sizeof(F32) * 4;
        } else {
            // Compressed texture formats are intentionally not supported yet.
            return false;
        }

        const USize expected_size = static_cast<USize>(header.width) *
                                    static_cast<USize>(header.height) * expected_pixel_size;

        if (static_cast<USize>(header.image_data_size) != expected_size) {
            return false;
        }

        out.width = header.width;
        out.height = header.height;
        out.mip_levels = header.mip_levels;
        out.gpu_format = gpu_format;

        out.pixels.clear();
        out.pixels.grow_default(header.image_data_size);

        if (!read_exact(file, out.pixels.data(), header.image_data_size)) {
            out.pixels.clear();
            return false;
        }

        return true;
    }

    /**
     * @brief Loads and validates a cooked `.fmesh` file into CPU memory.
     *
     * @details
     * This function performs only file loading and validation. It does not create GPU
     * resources, does not load texture dependencies, and does not modify the asset cache.
     *
     * Expected file layout:
     *
     * - MeshHeader
     * - CookedSubMesh array
     * - CookedVertex array
     * - U32 index array
     * - optional null-terminated string block
     *
     * @param path Path to the cooked `.fmesh` file.
     * @param out Loaded CPU-side mesh file data.
     * @return True on success, false on validation or IO failure.
     */
    bool load_mesh_file(StringView path, LoadedMeshFile &out) noexcept {
        String path_str = String::from_view(path);

        std::ifstream file(path_str.data(), std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        MeshHeader header{};
        if (!read_object(file, header)) {
            return false;
        }

        if (header.base.verify[0] != 'F' || header.base.verify[1] != 'M' ||
            header.base.verify[2] != 'S' || header.base.verify[3] != 'H') {
            return false;
        }

        if (header.base.version != 1) {
            return false;
        }

        if (header.vertex_count == 0 || header.index_count == 0 || header.submesh_count == 0) {
            return false;
        }

        const U32 expected_submesh_data_size = header.submesh_count * sizeof(CookedSubMesh);
        const U32 expected_vertex_data_size = header.vertex_count * sizeof(CookedVertex);
        const U32 expected_index_data_size = header.index_count * sizeof(U32);

        if (header.submesh_data_size != expected_submesh_data_size ||
            header.vertex_data_size != expected_vertex_data_size ||
            header.index_data_size != expected_index_data_size) {
            return false;
        }

        out.submeshes.clear();
        out.vertices.clear();
        out.indices.clear();
        out.string_block.clear();

        out.submeshes.grow_default(header.submesh_count);
        if (!read_array(file, out.submeshes.data(), header.submesh_count)) {
            out.submeshes.clear();
            return false;
        }

        out.vertices.grow_default(header.vertex_count);
        if (!read_array(file, out.vertices.data(), header.vertex_count)) {
            out.submeshes.clear();
            out.vertices.clear();
            return false;
        }

        out.indices.grow_default(header.index_count);
        if (!read_array(file, out.indices.data(), header.index_count)) {
            out.submeshes.clear();
            out.vertices.clear();
            out.indices.clear();
            return false;
        }

        if (header.string_block_size > 0) {
            out.string_block.grow_default(header.string_block_size);

            if (!read_exact(file, out.string_block.data(), header.string_block_size)) {
                out.submeshes.clear();
                out.vertices.clear();
                out.indices.clear();
                out.string_block.clear();
                return false;
            }

            // The string block stores null-terminated strings referenced by offsets.
            if (out.string_block.back() != '\0') {
                out.submeshes.clear();
                out.vertices.clear();
                out.indices.clear();
                out.string_block.clear();
                return false;
            }
        }

        out.aabb_min = glm::vec3(header.aabb_min[0], header.aabb_min[1], header.aabb_min[2]);
        out.aabb_max = glm::vec3(header.aabb_max[0], header.aabb_max[1], header.aabb_max[2]);

        /*
            Validate submesh draw ranges.

            Indices stored in the file are local to the submesh vertex range.
            The renderer draws with glDrawElementsBaseVertex(), so the effective vertex index is:

                submesh.vertex_offset + index_value
        */
        for (USize submesh_idx = 0; submesh_idx < out.submeshes.size(); ++submesh_idx) {
            const CookedSubMesh &submesh = out.submeshes[submesh_idx];

            if (submesh.index_count == 0) {
                return false;
            }

            const USize index_begin = static_cast<USize>(submesh.index_offset);
            const USize index_count = static_cast<USize>(submesh.index_count);
            const USize index_end = index_begin + index_count;

            if (index_begin >= out.indices.size() || index_end > out.indices.size()) {
                return false;
            }

            const USize vertex_offset = static_cast<USize>(submesh.vertex_offset);

            for (USize i = index_begin; i < index_end; ++i) {
                const USize effective_vertex_index =
                    vertex_offset + static_cast<USize>(out.indices[i]);

                if (effective_vertex_index >= out.vertices.size()) {
                    return false;
                }
            }
        }

        return true;
    }

    RenderDevice *m_device{nullptr};
    Alloc *m_alloc{nullptr};

    SlotMap<MeshAsset> m_meshes;
    SlotMap<TextureAsset> m_textures;

    HashMap<U64, MeshAssetHandle> m_mesh_cache;
    HashMap<U64, TextureAssetHandle> m_texture_cache;
};

} // namespace fr
