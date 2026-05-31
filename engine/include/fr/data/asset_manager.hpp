/**
 *
 * @file asset_manager.hpp
 * @author Tfoedy
 * @brief General asset manager for loading and managing game assets.
 *
 */
#pragma once

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/strong_handle.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/renderer/mesh.hpp"
#include "fr/renderer/mesh_loader.hpp"
#include "fr/renderer/render_device.hpp"
#include "fr/renderer/texture_loader.hpp"

namespace fr {
class RenderDevice;

/// @brief Tag identifying high-level mesh assets
struct MeshAssetTag {};
/// @brief Strong handle given to ECS components representing a 3D model
using MeshAssetHandle = StrongHandle<MeshAssetTag>;

/// @brief Tag identifying high-level texture assets
struct TextureAssetTag {};
/// @brief Strong handle given to ECS components representing an image
using TextureAssetHandle = StrongHandle<TextureAssetTag>;

/// @brief Internal registry record for a loaded geometry asset
struct MeshAsset {
    MeshData data{};
    U32 ref_count{0};
    U64 path_hash{0};
};

/// @brief Internal registry record for a loaded texture asset
struct TextureAsset {
    TextureHandle handle{};
    U32 ref_count{0};
    U64 path_hash{0};
};

class AssetManager {
public:
    /**
     * @brief Initializes the asset manager and allocates caching structures.
     * @param device Pointer to the hardware render device.
     * @param alloc Standard memory allocator.
     */
    explicit AssetManager(RenderDevice *device, Alloc *alloc) noexcept
        : m_device(device),
          m_mesh_cache(alloc),
          m_texture_cache(alloc) {
        m_mesh_cache.reserve(1024);
        m_texture_cache.reserve(2048);
    }

    /**
     * @brief Destructor ensuring all remaining assets are destroyed from VRAM.
     */
    ~AssetManager() noexcept {
        for (USize i = 0; i < m_mesh_cache.size(); ++i) {
            MeshAsset *record = m_meshes.get_data_unsafe(m_mesh_cache[i].handle.key);
            if (record && record->data.vbo.is_valid()) {
                m_device->destroy_buffer(record->data.vbo);
                m_device->destroy_buffer(record->data.ibo);
            }
        }

        for (USize i = 0; i < m_texture_cache.size(); ++i) {
            TextureAsset *record = m_textures.get_data_unsafe(m_texture_cache[i].handle.key);
            if (record && record->handle.is_valid())
                m_device->destroy_texture(record->handle);
        }
    }

    AssetManager(const AssetManager &) = delete;
    AssetManager &operator=(const AssetManager &) = delete;

    /**
     * @brief Loads a 3D GLTF model or retrieves it from cache if already loaded.
     * @param file_path Path to the geometry file.
     * @return Safe strong handle to the mesh.
     */
    MeshAssetHandle load_mesh(StringView file_path) noexcept {
        fr::Hash path_hash = file_path.hash();

        for (USize i = 0; i < m_mesh_cache.size(); ++i) {
            if (m_mesh_cache[i].hash.value == path_hash.value) {
                MeshAsset *record = m_meshes.get_data_unsafe(m_mesh_cache[i].handle.key);
                if (record) {
                    record->ref_count++;
                    return m_mesh_cache[i].handle;
                }
            }
        }

        MeshData new_data = load_mesh_gltf(m_device, file_path);
        if (!new_data.vbo.is_valid()) {
            return MeshAssetHandle{};
        }

        MeshAsset new_asset{};
        new_asset.data = new_data;
        new_asset.ref_count = 1;
        new_asset.path_hash = path_hash.value;

        MeshAssetHandle handle{};
        handle.key = m_meshes.add(new_asset);

        AssetCache<MeshAssetHandle> cache_entry{};
        cache_entry.hash = path_hash;
        cache_entry.handle = handle;

        m_mesh_cache.push_back(cache_entry);
        return handle;
    }

    /**
     * @brief Decrements reference counter for a mesh, deleting it if reaches zero.
     * @param handle The mesh to unload.
     */
    void unload_mesh(MeshAssetHandle handle) noexcept {
        MeshAsset *record = m_meshes.get_data_unsafe(handle.key);
        if (!record) {
            return;
        }

        record->ref_count--;
        if (record->ref_count == 0) {
            m_device->destroy_buffer(record->data.vbo);
            m_device->destroy_buffer(record->data.ibo);

            for (USize i = 0; i < m_mesh_cache.size(); ++i) {
                if (m_mesh_cache[i].handle == handle) {
                    m_mesh_cache[i] = m_mesh_cache[m_mesh_cache.size() - 1];
                    m_mesh_cache.pop_back();
                    break;
                }
            }

            m_meshes.erase(handle.key);
        }
    }

    /**
     * @brief O(1) lookup of actual mesh geometry metadata.
     * @param handle Valid handle.
     * @return Raw pointer to the mesh data.
     */
    [[nodiscard]] const MeshData *get_mesh_data(MeshAssetHandle handle) const noexcept {
        const MeshAsset *record = m_meshes.get_data(handle.key);
        return record ? &record->data : nullptr;
    }

    /**
     * @brief Loads an image file and uploads it to GPU using the correct color space.
     * @param path Path to the PNG/JPG file.
     * @param is_srgb Set to true for Albedo/Color maps to enforce sRGB gamma correction.
     * @return Safe strong handle to the texture.
     */
    TextureAssetHandle load_texture(StringView path, bool is_srgb) noexcept {
        fr::Hash path_hash = path.hash();

        for (USize i = 0; i < m_texture_cache.size(); ++i) {
            if (m_texture_cache[i].hash.value == path_hash.value) {
                TextureAsset *record = m_textures.get_data_unsafe(m_texture_cache[i].handle.key);
                if (record) {
                    record->ref_count++;
                    return m_texture_cache[i].handle;
                }
            }
        }

        TextureHandle gpu_handle = load_texture_2d(m_device, path, is_srgb);
        if (!gpu_handle.is_valid())
            return TextureAssetHandle{};

        TextureAsset new_asset{};
        new_asset.handle = gpu_handle;
        new_asset.ref_count = 1;
        new_asset.path_hash = path_hash.value;

        TextureAssetHandle handle{};
        handle.key = m_textures.add(new_asset);

        AssetCache<TextureAssetHandle> cache_entry{};
        cache_entry.hash = path_hash;
        cache_entry.handle = handle;

        m_texture_cache.push_back(cache_entry);
        return handle;
    }

    /**
     * @brief O(1) retrieval of the raw GPU hardware texture handle.
     */
    [[nodiscard]] TextureHandle get_texture_handle(TextureAssetHandle handle) const noexcept {
        const TextureAsset *record = m_textures.get_data(handle.key);
        return record ? record->handle : TextureHandle{};
    }

private:
    template <typename T>
    struct AssetCache {
        fr::Hash hash;
        T handle;
    };

    RenderDevice *m_device{nullptr};

    SlotMap<MeshAsset> m_meshes;
    SlotMap<TextureAsset> m_textures;
    DynamicArray<AssetCache<MeshAssetHandle>> m_mesh_cache;
    DynamicArray<AssetCache<TextureAssetHandle>> m_texture_cache;
};
} // namespace fr
