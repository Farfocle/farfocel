/**
 * @file asset_manager.hpp
 * @author Tfoedy
 * @brief Runtime cooked asset manager.
 */

#pragma once

#include <mutex>
#include <type_traits>
#include <utility>

#include <glm/gtc/type_ptr.hpp>

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/slot_map.hpp"
#include "fr/core/strong_handle.hpp"
#include "fr/core/thread_pool.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_format.hpp"
#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/asset_storage.hpp"
#include "fr/asset/material_asset.hpp"
#include "fr/asset/shader_asset.hpp"

#include "fr/logger/logger.hpp"

#include "fr/renderer/render_device.hpp"
#include "fr/renderer/render_mesh.hpp"

namespace fr {

struct MeshAssetTag {};
using MeshAssetHandle = StrongHandle<MeshAssetTag>;

struct TextureAssetTag {};
using TextureAssetHandle = StrongHandle<TextureAssetTag>;

struct MaterialAssetTag {};
using MaterialAssetHandle = StrongHandle<MaterialAssetTag>;

struct ShaderAssetTag {};
using ShaderAssetHandle = StrongHandle<ShaderAssetTag>;

/// @brief Asynchronous cooked asset loading state.
enum class AssetLoadState : U8 {
    Unloaded,
    LoadingCpu,
    ReadyForGpu,
    Loaded,
    Failed,
};

/// @brief Runtime 2D texture creation descriptor.
struct RuntimeTextureDesc {
    U32 width{0};
    U32 height{0};
    U32 mip_levels{1};

    TextureFormat format{TextureFormat::R8G8B8A8_UNorm};

    Slice<const Byte> pixels{};
};

/**
 * @brief Runtime material creation descriptor.
 *
 * @details
 * Texture handles have priority over texture AssetIds stored in data. If a handle is invalid and
 * the corresponding AssetId is valid, AssetManager loads the texture through the cooked asset path.
 */
struct RuntimeMaterialDesc {
    MaterialAssetData data{};

    TextureAssetHandle albedo_texture{};
    TextureAssetHandle normal_texture{};
    TextureAssetHandle extra_texture{};
};

/// @brief Runtime mesh asset record.
struct MeshAsset {
    RenderMeshData data{};
    DynamicArray<MaterialAssetHandle> material_deps{};

    U32 ref_count{0};
    U64 cache_key{0};

    bool runtime_asset{false};
};

/// @brief Runtime texture asset record.
struct TextureAsset {
    TextureHandle handle{};

    U32 ref_count{0};
    U64 cache_key{0};

    bool runtime_asset{false};
};

/// @brief Runtime material asset record.
struct MaterialAsset {
    MaterialAssetData data{};

    TextureAssetHandle albedo_texture{};
    TextureAssetHandle normal_texture{};
    TextureAssetHandle extra_texture{};

    U32 ref_count{0};
    U64 cache_key{0};

    bool runtime_asset{false};
};

/// @brief Runtime shader asset record.
struct ShaderAsset {
    ShaderHandle handle{};

    U32 ref_count{0};
    U64 cache_key{0};
};

/**
 * @brief Loads cooked assets and owns their runtime GPU resources.
 *
 * @details
 * Synchronous load_* methods are kept for simple code paths. Runtime creation methods create
 * resources directly from memory and use the same handles as cooked assets.
 */
class AssetManager {
public:
    explicit AssetManager(RenderDevice *device, Alloc *alloc, const AssetRegistry *registry,
                          AssetStorage *storage) noexcept
        : m_device(device),
          m_alloc(alloc),
          m_registry(registry),
          m_storage(storage),
          m_mesh_cache(HashMap<U64, MeshAssetHandle>::with_capacity(alloc, 1024)),
          m_texture_cache(HashMap<U64, TextureAssetHandle>::with_capacity(alloc, 2048)),
          m_material_cache(HashMap<U64, MaterialAssetHandle>::with_capacity(alloc, 1024)),
          m_shader_cache(HashMap<U64, ShaderAssetHandle>::with_capacity(alloc, 256)),
          m_mesh_async_state(HashMap<U64, AssetLoadState>::with_capacity(alloc, 1024)),
          m_texture_async_state(HashMap<U64, AssetLoadState>::with_capacity(alloc, 2048)),
          m_material_async_state(HashMap<U64, AssetLoadState>::with_capacity(alloc, 1024)),
          m_shader_async_state(HashMap<U64, AssetLoadState>::with_capacity(alloc, 256)),
          m_pending_mesh_loads(alloc),
          m_pending_texture_loads(alloc),
          m_pending_material_loads(alloc),
          m_pending_shader_loads(alloc) {
        FR_ASSERT(device, "RenderDevice must be non-null");
        FR_ASSERT(alloc, "allocator must be non-null");
        FR_ASSERT(storage, "AssetStorage must be non-null");

        m_pending_mesh_loads.reserve(64);
        m_pending_texture_loads.reserve(128);
        m_pending_material_loads.reserve(64);
        m_pending_shader_loads.reserve(32);
    }

    ~AssetManager() noexcept {
        destroy_all_runtime_resources();
    }

    AssetManager(const AssetManager &) = delete;
    AssetManager(AssetManager &&) = delete;
    AssetManager &operator=(const AssetManager &) = delete;
    AssetManager &operator=(AssetManager &&) = delete;

    void set_registry(const AssetRegistry *registry) noexcept {
        m_registry = registry;
    }

    void set_storage(AssetStorage *storage) noexcept {
        FR_ASSERT(storage, "AssetStorage must be non-null");
        m_storage = storage;
    }

    /**
     * @brief Creates a renderer mesh directly from runtime geometry.
     */
    MeshAssetHandle create_runtime_mesh(const RuntimeMeshDesc &desc) noexcept {
        return create_runtime_mesh(desc, {});
    }

    /**
     * @brief Creates a renderer mesh directly from runtime geometry and material handles.
     *
     * @details
     * material_handles may be empty. If not empty, it must have the same size as desc.submeshes.
     * A valid handle overrides RuntimeSubMeshDesc::material_id for that submesh.
     */
    MeshAssetHandle
    create_runtime_mesh(const RuntimeMeshDesc &desc,
                        Slice<const MaterialAssetHandle> material_handles) noexcept {
        if (!validate_runtime_mesh_desc(desc)) {
            FR_LOG_ERR("Invalid runtime mesh descriptor.");
            return {};
        }

        if (!material_handles.is_empty() && material_handles.size() != desc.submeshes.size()) {
            FR_LOG_ERR("Runtime mesh material handle count does not match submesh count.");
            return {};
        }

        USize vertex_data_size = 0;
        USize index_data_size = 0;

        if (!checked_mul_u_size(desc.vertices.size(), sizeof(RenderVertex), vertex_data_size) ||
            !checked_mul_u_size(desc.indices.size(), sizeof(U32), index_data_size)) {
            FR_LOG_ERR("Runtime mesh buffer size overflow.");
            return {};
        }

        RenderMeshData mesh_data(m_alloc);

        mesh_data.vbo = m_device->create_buffer(
            Slice<const Byte>(reinterpret_cast<const Byte *>(desc.vertices.data()),
                              vertex_data_size),
            desc.dynamic);

        mesh_data.ibo = m_device->create_buffer(
            Slice<const Byte>(reinterpret_cast<const Byte *>(desc.indices.data()), index_data_size),
            desc.dynamic);

        mesh_data.aabb_min = desc.aabb_min;
        mesh_data.aabb_max = desc.aabb_max;

        if (!mesh_data.vbo.is_valid() || !mesh_data.ibo.is_valid()) {
            if (mesh_data.vbo.is_valid()) {
                m_device->destroy_buffer(mesh_data.vbo);
                mesh_data.vbo = {};
            }

            if (mesh_data.ibo.is_valid()) {
                m_device->destroy_buffer(mesh_data.ibo);
                mesh_data.ibo = {};
            }

            FR_LOG_ERR("Failed to create GPU buffers for runtime mesh.");
            return {};
        }

        mesh_data.submeshes.reserve(desc.submeshes.size());

        DynamicArray<MaterialAssetHandle> material_deps(m_alloc);
        material_deps.reserve(desc.submeshes.size());

        for (USize i = 0; i < desc.submeshes.size(); ++i) {
            const RuntimeSubMeshDesc &runtime_submesh = desc.submeshes[i];

            RenderSubMesh submesh{};
            submesh.index_count = runtime_submesh.index_count;
            submesh.index_offset = runtime_submesh.index_offset;
            submesh.vertex_offset = runtime_submesh.vertex_offset;
            submesh.transform = runtime_submesh.transform;
            submesh.pass_type = runtime_submesh.pass_type;
            submesh.material_id = runtime_submesh.material_id;
            submesh.material_index = INVALID_RENDER_SUBMESH_MATERIAL_INDEX;
            submesh.aabb_min = runtime_submesh.aabb_min;
            submesh.aabb_max = runtime_submesh.aabb_max;

            MaterialAssetHandle explicit_material{};
            if (!material_handles.is_empty()) {
                explicit_material = material_handles[i];
            }

            if (explicit_material.is_valid()) {
                U32 material_index =
                    find_material_dependency_handle_index(material_deps, explicit_material);

                if (material_index == INVALID_RENDER_SUBMESH_MATERIAL_INDEX &&
                    retain_material(explicit_material)) {
                    material_index = static_cast<U32>(material_deps.size());
                    material_deps.push_back(explicit_material);
                }

                submesh.material_index = material_index;
            } else if (runtime_submesh.material_id.is_valid()) {
                U32 material_index = find_material_dependency_asset_index(
                    material_deps, runtime_submesh.material_id);

                if (material_index == INVALID_RENDER_SUBMESH_MATERIAL_INDEX) {
                    MaterialAssetHandle material = load_material(runtime_submesh.material_id);

                    if (material.is_valid()) {
                        material_index = static_cast<U32>(material_deps.size());
                        material_deps.push_back(material);
                    }
                }

                submesh.material_index = material_index;
            }

            mesh_data.submeshes.push_back(submesh);
        }

        const U64 cache_key = allocate_runtime_mesh_cache_key();

        MeshAsset new_asset{};
        new_asset.data = std::move(mesh_data);
        new_asset.material_deps = std::move(material_deps);
        new_asset.ref_count = 1;
        new_asset.cache_key = cache_key;
        new_asset.runtime_asset = true;

        MeshAssetHandle handle{m_meshes.add(std::move(new_asset))};
        m_mesh_cache.insert(cache_key, handle);

        return handle;
    }

    /// @brief Creates a runtime 2D texture from memory.
    TextureAssetHandle create_runtime_texture_2d(const RuntimeTextureDesc &desc) noexcept {
        if (!validate_runtime_texture_desc(desc)) {
            FR_LOG_ERR("Invalid runtime texture descriptor.");
            return {};
        }

        TextureHandle gpu_handle = m_device->create_texture_2d(
            desc.width, desc.height, desc.mip_levels, desc.format, desc.pixels);

        if (!gpu_handle.is_valid()) {
            FR_LOG_ERR("Failed to create GPU texture for runtime texture.");
            return {};
        }

        const U64 cache_key = allocate_runtime_texture_cache_key();

        TextureAsset new_asset{};
        new_asset.handle = gpu_handle;
        new_asset.ref_count = 1;
        new_asset.cache_key = cache_key;
        new_asset.runtime_asset = true;

        TextureAssetHandle handle{m_textures.add(new_asset)};
        m_texture_cache.insert(cache_key, handle);

        return handle;
    }

    /**
     * @brief Creates a runtime material.
     *
     * @details
     * Runtime material can reference runtime textures through handles or cooked textures through
     * AssetIds stored in RuntimeMaterialDesc::data.
     */
    MaterialAssetHandle create_runtime_material(const RuntimeMaterialDesc &desc) noexcept {
        if (!validate_runtime_material_desc(desc)) {
            FR_LOG_ERR("Invalid runtime material descriptor.");
            return {};
        }

        MaterialAsset new_asset{};
        new_asset.data = desc.data;
        new_asset.ref_count = 1;
        new_asset.cache_key = allocate_runtime_material_cache_key();
        new_asset.runtime_asset = true;

        if (desc.albedo_texture.is_valid()) {
            if (retain_texture(desc.albedo_texture)) {
                new_asset.albedo_texture = desc.albedo_texture;
            }
        } else if (desc.data.albedo_texture.is_valid()) {
            new_asset.albedo_texture = load_texture(desc.data.albedo_texture);
        }

        if (desc.normal_texture.is_valid()) {
            if (retain_texture(desc.normal_texture)) {
                new_asset.normal_texture = desc.normal_texture;
            }
        } else if (desc.data.normal_texture.is_valid()) {
            new_asset.normal_texture = load_texture(desc.data.normal_texture);
        }

        if (desc.extra_texture.is_valid()) {
            if (retain_texture(desc.extra_texture)) {
                new_asset.extra_texture = desc.extra_texture;
            }
        } else if (desc.data.extra_texture.is_valid()) {
            new_asset.extra_texture = load_texture(desc.data.extra_texture);
        }

        const U64 cache_key = new_asset.cache_key;

        MaterialAssetHandle handle{m_materials.add(std::move(new_asset))};
        m_material_cache.insert(cache_key, handle);

        return handle;
    }

    MeshAssetHandle load_mesh(AssetId id) noexcept {
        if (!id.is_valid()) {
            return {};
        }

        if (MeshAssetHandle cached = get_cached_mesh(id.value); cached.is_valid()) {
            set_mesh_state(id.value, AssetLoadState::Loaded);
            return cached;
        }

        const AssetRecord *record = resolve_record(id, AssetKind::Mesh);
        if (!record) {
            FR_LOG_ERR("Failed to resolve mesh asset id: {}", id.value);
            set_mesh_state(id.value, AssetLoadState::Failed);
            return {};
        }

        DynamicArray<Byte> bytes(m_alloc);
        if (!m_storage->read_record_bytes(*record, bytes)) {
            FR_LOG_ERR("Failed to read mesh asset bytes: {}", id.value);
            set_mesh_state(id.value, AssetLoadState::Failed);
            return {};
        }

        LoadedMeshFile loaded(m_alloc);
        if (!load_mesh_file(bytes.slice(), loaded)) {
            FR_LOG_ERR("Failed to decode mesh asset: {}", id.value);
            set_mesh_state(id.value, AssetLoadState::Failed);
            return {};
        }

        MeshAssetHandle handle = upload_loaded_mesh(std::move(loaded), id.value);
        set_mesh_state(id.value,
                       handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
        return handle;
    }

    TextureAssetHandle load_texture(AssetId id) noexcept {
        if (!id.is_valid()) {
            return {};
        }

        if (TextureAssetHandle cached = get_cached_texture(id.value); cached.is_valid()) {
            set_texture_state(id.value, AssetLoadState::Loaded);
            return cached;
        }

        const AssetRecord *record = resolve_record(id, AssetKind::Texture);
        if (!record) {
            FR_LOG_ERR("Failed to resolve texture asset id: {}", id.value);
            set_texture_state(id.value, AssetLoadState::Failed);
            return {};
        }

        DynamicArray<Byte> bytes(m_alloc);
        if (!m_storage->read_record_bytes(*record, bytes)) {
            FR_LOG_ERR("Failed to read texture asset bytes: {}", id.value);
            set_texture_state(id.value, AssetLoadState::Failed);
            return {};
        }

        LoadedTextureFile loaded(m_alloc);
        if (!load_texture_file(bytes.slice(), loaded)) {
            FR_LOG_ERR("Failed to decode texture asset: {}", id.value);
            set_texture_state(id.value, AssetLoadState::Failed);
            return {};
        }

        TextureAssetHandle handle = upload_loaded_texture(std::move(loaded), id.value);
        set_texture_state(id.value,
                          handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
        return handle;
    }

    MaterialAssetHandle load_material(AssetId id) noexcept {
        if (!id.is_valid()) {
            return {};
        }

        if (MaterialAssetHandle cached = get_cached_material(id.value); cached.is_valid()) {
            set_material_state(id.value, AssetLoadState::Loaded);
            return cached;
        }

        const AssetRecord *record = resolve_record(id, AssetKind::Material);
        if (!record) {
            FR_LOG_ERR("Failed to resolve material asset id: {}", id.value);
            set_material_state(id.value, AssetLoadState::Failed);
            return {};
        }

        DynamicArray<Byte> bytes(m_alloc);
        if (!m_storage->read_record_bytes(*record, bytes)) {
            FR_LOG_ERR("Failed to read material asset bytes: {}", id.value);
            set_material_state(id.value, AssetLoadState::Failed);
            return {};
        }

        MaterialAssetData data{};
        if (!load_cooked_material(bytes.slice(), data)) {
            FR_LOG_ERR("Failed to decode material asset: {}", id.value);
            set_material_state(id.value, AssetLoadState::Failed);
            return {};
        }

        MaterialAssetHandle handle = upload_loaded_material(data, id.value);
        set_material_state(id.value,
                           handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
        return handle;
    }

    ShaderAssetHandle load_shader(AssetId id) noexcept {
        if (!id.is_valid()) {
            return {};
        }

        if (ShaderAssetHandle cached = get_cached_shader(id.value); cached.is_valid()) {
            set_shader_state(id.value, AssetLoadState::Loaded);
            return cached;
        }

        const AssetRecord *record = resolve_record(id, AssetKind::Shader);
        if (!record) {
            FR_LOG_ERR("Failed to resolve shader asset id: {}", id.value);
            set_shader_state(id.value, AssetLoadState::Failed);
            return {};
        }

        DynamicArray<Byte> bytes(m_alloc);
        if (!m_storage->read_record_bytes(*record, bytes)) {
            FR_LOG_ERR("Failed to read shader asset bytes: {}", id.value);
            set_shader_state(id.value, AssetLoadState::Failed);
            return {};
        }

        String debug_name(m_alloc);
        if (record->location_kind == AssetLocationKind::LooseFile) {
            debug_name = String::from_view(m_alloc, record->loose_path.view());
        } else {
            debug_name = String::from_view(m_alloc, "shader asset");
        }

        ShaderSourceBundle sources(m_alloc);
        if (!load_cooked_shader_sources(m_alloc, bytes.slice(), sources)) {
            FR_LOG_ERR("Failed to decode shader asset: {}", id.value);
            set_shader_state(id.value, AssetLoadState::Failed);
            return {};
        }

        ShaderAssetHandle handle = upload_loaded_shader(std::move(sources), id.value, debug_name);
        set_shader_state(id.value,
                         handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
        return handle;
    }

    /**
     * @brief Queues CPU decode for a cooked mesh asset.
     * @note GPU upload happens later in process_async_uploads().
     */
    bool request_mesh(ThreadPool &pool, AssetId id, bool retry_failed = false) noexcept {
        if (!id.is_valid()) {
            return false;
        }

        if (m_mesh_cache.find(id.value).is_some()) {
            set_mesh_state(id.value, AssetLoadState::Loaded);
            return true;
        }

        if (!can_start_async_load(m_mesh_async_state, id.value, retry_failed)) {
            return true;
        }

        set_mesh_state(id.value, AssetLoadState::LoadingCpu);

        pool.submit([this, id] {
            PendingMeshLoad pending(m_alloc);
            pending.cache_key = id.value;

            const AssetRecord *record = resolve_record(id, AssetKind::Mesh);
            if (!record) {
                pending.ok = false;
                pending.error = PendingAssetError::ResolveFailed;
                push_pending_mesh(std::move(pending));
                return;
            }

            DynamicArray<Byte> bytes(m_alloc);
            if (!m_storage->read_record_bytes(*record, bytes)) {
                pending.ok = false;
                pending.error = PendingAssetError::ReadFailed;
                push_pending_mesh(std::move(pending));
                return;
            }

            pending.ok = load_mesh_file(bytes.slice(), pending.loaded);
            pending.error = pending.ok ? PendingAssetError::None : PendingAssetError::DecodeFailed;

            push_pending_mesh(std::move(pending));
        });

        return true;
    }

    bool request_texture(ThreadPool &pool, AssetId id, bool retry_failed = false) noexcept {
        if (!id.is_valid()) {
            return false;
        }

        if (m_texture_cache.find(id.value).is_some()) {
            set_texture_state(id.value, AssetLoadState::Loaded);
            return true;
        }

        if (!can_start_async_load(m_texture_async_state, id.value, retry_failed)) {
            return true;
        }

        set_texture_state(id.value, AssetLoadState::LoadingCpu);

        pool.submit([this, id] {
            PendingTextureLoad pending(m_alloc);
            pending.cache_key = id.value;

            const AssetRecord *record = resolve_record(id, AssetKind::Texture);
            if (!record) {
                pending.ok = false;
                pending.error = PendingAssetError::ResolveFailed;
                push_pending_texture(std::move(pending));
                return;
            }

            DynamicArray<Byte> bytes(m_alloc);
            if (!m_storage->read_record_bytes(*record, bytes)) {
                pending.ok = false;
                pending.error = PendingAssetError::ReadFailed;
                push_pending_texture(std::move(pending));
                return;
            }

            pending.ok = load_texture_file(bytes.slice(), pending.loaded);
            pending.error = pending.ok ? PendingAssetError::None : PendingAssetError::DecodeFailed;

            push_pending_texture(std::move(pending));
        });

        return true;
    }

    bool request_material(ThreadPool &pool, AssetId id, bool retry_failed = false) noexcept {
        if (!id.is_valid()) {
            return false;
        }

        if (m_material_cache.find(id.value).is_some()) {
            set_material_state(id.value, AssetLoadState::Loaded);
            return true;
        }

        if (!can_start_async_load(m_material_async_state, id.value, retry_failed)) {
            return true;
        }

        set_material_state(id.value, AssetLoadState::LoadingCpu);

        pool.submit([this, id] {
            PendingMaterialLoad pending{};
            pending.cache_key = id.value;

            const AssetRecord *record = resolve_record(id, AssetKind::Material);
            if (!record) {
                pending.ok = false;
                pending.error = PendingAssetError::ResolveFailed;
                push_pending_material(std::move(pending));
                return;
            }

            DynamicArray<Byte> bytes(m_alloc);
            if (!m_storage->read_record_bytes(*record, bytes)) {
                pending.ok = false;
                pending.error = PendingAssetError::ReadFailed;
                push_pending_material(std::move(pending));
                return;
            }

            pending.ok = load_cooked_material(bytes.slice(), pending.data);
            pending.error = pending.ok ? PendingAssetError::None : PendingAssetError::DecodeFailed;

            push_pending_material(std::move(pending));
        });

        return true;
    }

    bool request_shader(ThreadPool &pool, AssetId id, bool retry_failed = false) noexcept {
        if (!id.is_valid()) {
            return false;
        }

        if (m_shader_cache.find(id.value).is_some()) {
            set_shader_state(id.value, AssetLoadState::Loaded);
            return true;
        }

        if (!can_start_async_load(m_shader_async_state, id.value, retry_failed)) {
            return true;
        }

        set_shader_state(id.value, AssetLoadState::LoadingCpu);

        pool.submit([this, id] {
            PendingShaderLoad pending(m_alloc);
            pending.cache_key = id.value;

            const AssetRecord *record = resolve_record(id, AssetKind::Shader);
            if (!record) {
                pending.ok = false;
                pending.error = PendingAssetError::ResolveFailed;
                push_pending_shader(std::move(pending));
                return;
            }

            if (record->location_kind == AssetLocationKind::LooseFile) {
                pending.debug_name = String::from_view(m_alloc, record->loose_path.view());
            } else {
                pending.debug_name = String::from_view(m_alloc, "shader asset");
            }

            DynamicArray<Byte> bytes(m_alloc);
            if (!m_storage->read_record_bytes(*record, bytes)) {
                pending.ok = false;
                pending.error = PendingAssetError::ReadFailed;
                push_pending_shader(std::move(pending));
                return;
            }

            pending.ok = load_cooked_shader_sources(m_alloc, bytes.slice(), pending.sources);
            pending.error = pending.ok ? PendingAssetError::None : PendingAssetError::DecodeFailed;

            push_pending_shader(std::move(pending));
        });

        return true;
    }

    /**
     * @brief Uploads finished async CPU decode results on the calling thread.
     *
     * @details Call this on the render/main thread because RenderDevice usually owns GL/Vulkan/DX
     * thread-affine resources.
     */
    void process_async_uploads(USize max_uploads = static_cast<USize>(-1)) noexcept {
        USize uploaded = 0;

        uploaded += process_pending_textures(max_uploads - uploaded);
        if (uploaded >= max_uploads) {
            return;
        }

        uploaded += process_pending_materials(max_uploads - uploaded);
        if (uploaded >= max_uploads) {
            return;
        }

        uploaded += process_pending_meshes(max_uploads - uploaded);
        if (uploaded >= max_uploads) {
            return;
        }

        uploaded += process_pending_shaders(max_uploads - uploaded);
        (void)uploaded;
    }

    [[nodiscard]] AssetLoadState mesh_state(AssetId id) const noexcept {
        return get_load_state(m_mesh_async_state, id.value);
    }

    [[nodiscard]] AssetLoadState texture_state(AssetId id) const noexcept {
        return get_load_state(m_texture_async_state, id.value);
    }

    [[nodiscard]] AssetLoadState material_state(AssetId id) const noexcept {
        return get_load_state(m_material_async_state, id.value);
    }

    [[nodiscard]] AssetLoadState shader_state(AssetId id) const noexcept {
        return get_load_state(m_shader_async_state, id.value);
    }

    /// @brief Returns a loaded mesh handle without changing `ref_count`.
    [[nodiscard]] MeshAssetHandle try_get_mesh(AssetId id) const noexcept {
        auto cached = m_mesh_cache.find(id.value);
        return cached.is_some() ? *cached.unwrap() : MeshAssetHandle{};
    }

    /// @brief Returns a loaded texture handle without changing `ref_count`.
    [[nodiscard]] TextureAssetHandle try_get_texture(AssetId id) const noexcept {
        auto cached = m_texture_cache.find(id.value);
        return cached.is_some() ? *cached.unwrap() : TextureAssetHandle{};
    }

    /// @brief Returns a loaded material handle without changing `ref_count`.
    [[nodiscard]] MaterialAssetHandle try_get_material(AssetId id) const noexcept {
        auto cached = m_material_cache.find(id.value);
        return cached.is_some() ? *cached.unwrap() : MaterialAssetHandle{};
    }

    /// @brief Returns a loaded shader handle without changing `ref_count`.
    [[nodiscard]] ShaderAssetHandle try_get_shader(AssetId id) const noexcept {
        auto cached = m_shader_cache.find(id.value);
        return cached.is_some() ? *cached.unwrap() : ShaderAssetHandle{};
    }

    void unload_mesh(MeshAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return;
        }

        MeshAsset *record = m_meshes.get_data(handle.key);
        if (!record || record->ref_count == 0) {
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

        for (USize i = 0; i < record->material_deps.size(); ++i) {
            unload_material(record->material_deps[i]);
        }

        if (!record->runtime_asset) {
            m_mesh_async_state.remove(record->cache_key);
        }

        m_mesh_cache.remove(record->cache_key);
        m_meshes.erase(handle.key);
    }

    void unload_texture(TextureAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return;
        }

        TextureAsset *record = m_textures.get_data(handle.key);
        if (!record || record->ref_count == 0) {
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

        if (!record->runtime_asset) {
            m_texture_async_state.remove(record->cache_key);
        }

        m_texture_cache.remove(record->cache_key);
        m_textures.erase(handle.key);
    }

    void unload_material(MaterialAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return;
        }

        MaterialAsset *record = m_materials.get_data(handle.key);
        if (!record || record->ref_count == 0) {
            return;
        }

        --record->ref_count;

        if (record->ref_count != 0) {
            return;
        }

        unload_texture(record->albedo_texture);
        unload_texture(record->normal_texture);
        unload_texture(record->extra_texture);

        if (!record->runtime_asset) {
            m_material_async_state.remove(record->cache_key);
        }

        m_material_cache.remove(record->cache_key);
        m_materials.erase(handle.key);
    }

    void unload_shader(ShaderAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return;
        }

        ShaderAsset *record = m_shaders.get_data(handle.key);
        if (!record || record->ref_count == 0) {
            return;
        }

        --record->ref_count;

        if (record->ref_count != 0) {
            return;
        }

        if (record->handle.is_valid()) {
            m_device->destroy_shader(record->handle);
            record->handle = {};
        }

        m_shader_async_state.remove(record->cache_key);
        m_shader_cache.remove(record->cache_key);
        m_shaders.erase(handle.key);
    }

    [[nodiscard]] const RenderMeshData *get_mesh_data(MeshAssetHandle handle) const noexcept {
        const MeshAsset *record = m_meshes.get_data(handle.key);
        return record ? &record->data : nullptr;
    }

    [[nodiscard]] MaterialAssetHandle
    get_mesh_submesh_material(MeshAssetHandle mesh_handle, USize submesh_index) const noexcept {
        const MeshAsset *mesh = m_meshes.get_data(mesh_handle.key);
        if (!mesh) {
            return {};
        }

        if (submesh_index >= mesh->data.submeshes.size()) {
            return {};
        }

        const RenderSubMesh &submesh = mesh->data.submeshes[submesh_index];
        if (submesh.material_index == INVALID_RENDER_SUBMESH_MATERIAL_INDEX) {
            return {};
        }

        if (submesh.material_index >= mesh->material_deps.size()) {
            return {};
        }

        return mesh->material_deps[submesh.material_index];
    }

    [[nodiscard]] TextureHandle get_texture_handle(TextureAssetHandle handle) const noexcept {
        const TextureAsset *record = m_textures.get_data(handle.key);
        return record ? record->handle : TextureHandle{};
    }

    [[nodiscard]] const MaterialAsset *get_material(MaterialAssetHandle handle) const noexcept {
        return m_materials.get_data(handle.key);
    }

    [[nodiscard]] const MaterialAssetData *
    get_material_data(MaterialAssetHandle handle) const noexcept {
        const MaterialAsset *record = m_materials.get_data(handle.key);
        return record ? &record->data : nullptr;
    }

    [[nodiscard]] TextureHandle
    get_material_albedo_texture(MaterialAssetHandle handle) const noexcept {
        const MaterialAsset *record = m_materials.get_data(handle.key);
        return record ? get_texture_handle(record->albedo_texture) : TextureHandle{};
    }

    [[nodiscard]] TextureHandle
    get_material_normal_texture(MaterialAssetHandle handle) const noexcept {
        const MaterialAsset *record = m_materials.get_data(handle.key);
        return record ? get_texture_handle(record->normal_texture) : TextureHandle{};
    }

    [[nodiscard]] TextureHandle
    get_material_extra_texture(MaterialAssetHandle handle) const noexcept {
        const MaterialAsset *record = m_materials.get_data(handle.key);
        return record ? get_texture_handle(record->extra_texture) : TextureHandle{};
    }

    [[nodiscard]] ShaderHandle get_shader_handle(ShaderAssetHandle handle) const noexcept {
        const ShaderAsset *record = m_shaders.get_data(handle.key);
        return record ? record->handle : ShaderHandle{};
    }

private:
    [[nodiscard]] static bool checked_mul_u_size(USize a, USize b, USize &out) noexcept {
        if (a != 0 && b > static_cast<USize>(-1) / a) {
            return false;
        }

        out = a * b;
        return true;
    }

    [[nodiscard]] static bool is_supported_runtime_pass(RenderPass pass) noexcept {
        return pass == RenderPass::Opaque || pass == RenderPass::Masked ||
               pass == RenderPass::Transparent;
    }

    [[nodiscard]] bool validate_runtime_mesh_desc(const RuntimeMeshDesc &desc) const noexcept {
        if (desc.vertices.is_empty()) {
            FR_LOG_ERR("Runtime mesh has no vertices.");
            return false;
        }

        if (desc.indices.is_empty()) {
            FR_LOG_ERR("Runtime mesh has no indices.");
            return false;
        }

        if (desc.submeshes.is_empty()) {
            FR_LOG_ERR("Runtime mesh has no submeshes.");
            return false;
        }

        for (USize i = 0; i < desc.submeshes.size(); ++i) {
            const RuntimeSubMeshDesc &submesh = desc.submeshes[i];

            if (submesh.index_count == 0) {
                FR_LOG_ERR("Runtime mesh submesh {} has zero index count.", i);
                return false;
            }

            if (!is_supported_runtime_pass(submesh.pass_type)) {
                FR_LOG_ERR("Runtime mesh submesh {} has unsupported render pass.", i);
                return false;
            }

            const USize index_begin = static_cast<USize>(submesh.index_offset);
            const USize index_count = static_cast<USize>(submesh.index_count);

            if (index_begin >= desc.indices.size()) {
                FR_LOG_ERR("Runtime mesh submesh {} index offset is out of bounds.", i);
                return false;
            }

            if (index_count > desc.indices.size() - index_begin) {
                FR_LOG_ERR("Runtime mesh submesh {} index range is out of bounds.", i);
                return false;
            }

            const USize vertex_offset = static_cast<USize>(submesh.vertex_offset);
            if (vertex_offset >= desc.vertices.size()) {
                FR_LOG_ERR("Runtime mesh submesh {} vertex offset is out of bounds.", i);
                return false;
            }

            const USize local_vertex_count = desc.vertices.size() - vertex_offset;
            const USize index_end = index_begin + index_count;

            for (USize index = index_begin; index < index_end; ++index) {
                const USize local_index = static_cast<USize>(desc.indices[index]);

                if (local_index >= local_vertex_count) {
                    FR_LOG_ERR("Runtime mesh submesh {} references vertex out of bounds.", i);
                    return false;
                }
            }
        }

        return true;
    }

    [[nodiscard]] static USize
    runtime_texture_format_bytes_per_pixel(TextureFormat format) noexcept {
        switch (format) {
        case TextureFormat::R8G8B8A8_UNorm:
        case TextureFormat::R8G8B8A8_SRGB:
            return 4;

        case TextureFormat::R16G16_Float:
            return sizeof(U16) * 2;

        case TextureFormat::R16G16B16A16_Float:
            return sizeof(U16) * 4;

        case TextureFormat::R32G32B32A32_Float:
            return sizeof(F32) * 4;

        default:
            return 0;
        }
    }

    [[nodiscard]] static bool
    validate_runtime_texture_desc(const RuntimeTextureDesc &desc) noexcept {
        if (desc.width == 0 || desc.height == 0 || desc.mip_levels == 0) {
            FR_LOG_ERR("Runtime texture has invalid dimensions.");
            return false;
        }

        const USize bytes_per_pixel = runtime_texture_format_bytes_per_pixel(desc.format);
        if (bytes_per_pixel == 0) {
            FR_LOG_ERR("Runtime texture has unsupported format.");
            return false;
        }

        if (!desc.pixels.is_empty()) {
            USize pixel_count = 0;
            USize expected_size = 0;

            if (!checked_mul_u_size(static_cast<USize>(desc.width), static_cast<USize>(desc.height),
                                    pixel_count) ||
                !checked_mul_u_size(pixel_count, bytes_per_pixel, expected_size)) {
                FR_LOG_ERR("Runtime texture data size overflow.");
                return false;
            }

            if (desc.pixels.size() < expected_size) {
                FR_LOG_ERR("Runtime texture initial data is smaller than base mip size.");
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] static bool
    validate_runtime_material_desc(const RuntimeMaterialDesc &desc) noexcept {
        if (desc.data.metallic_factor < 0.0f || desc.data.metallic_factor > 1.0f) {
            FR_LOG_ERR("Runtime material metallic factor is out of range.");
            return false;
        }

        if (desc.data.roughness_factor < 0.0f || desc.data.roughness_factor > 1.0f) {
            FR_LOG_ERR("Runtime material roughness factor is out of range.");
            return false;
        }

        if (desc.data.alpha < 0.0f || desc.data.alpha > 1.0f) {
            FR_LOG_ERR("Runtime material alpha is out of range.");
            return false;
        }

        if (desc.data.alpha_cutoff < 0.0f || desc.data.alpha_cutoff > 1.0f) {
            FR_LOG_ERR("Runtime material alpha cutoff is out of range.");
            return false;
        }

        const MaterialShadingModel shading = desc.data.shading_model;
        if (shading != MaterialShadingModel::Unlit && shading != MaterialShadingModel::Standard &&
            shading != MaterialShadingModel::PBR) {
            FR_LOG_ERR("Runtime material has invalid shading model.");
            return false;
        }

        const MaterialBlendMode blend = desc.data.blend_mode;
        if (blend != MaterialBlendMode::Opaque && blend != MaterialBlendMode::Masked &&
            blend != MaterialBlendMode::Transparent) {
            FR_LOG_ERR("Runtime material has invalid blend mode.");
            return false;
        }

        return true;
    }

    [[nodiscard]] U64 allocate_runtime_mesh_cache_key() noexcept {
        U64 key = m_next_runtime_mesh_cache_key;

        do {
            key = m_next_runtime_mesh_cache_key++;
            if (m_next_runtime_mesh_cache_key == 0) {
                m_next_runtime_mesh_cache_key = 0x8000000000000000ull;
            }
        } while (m_mesh_cache.find(key).is_some());

        return key;
    }

    [[nodiscard]] U64 allocate_runtime_texture_cache_key() noexcept {
        U64 key = m_next_runtime_texture_cache_key;

        do {
            key = m_next_runtime_texture_cache_key++;
            if (m_next_runtime_texture_cache_key == 0) {
                m_next_runtime_texture_cache_key = 0x9000000000000000ull;
            }
        } while (m_texture_cache.find(key).is_some());

        return key;
    }

    [[nodiscard]] U64 allocate_runtime_material_cache_key() noexcept {
        U64 key = m_next_runtime_material_cache_key;

        do {
            key = m_next_runtime_material_cache_key++;
            if (m_next_runtime_material_cache_key == 0) {
                m_next_runtime_material_cache_key = 0xA000000000000000ull;
            }
        } while (m_material_cache.find(key).is_some());

        return key;
    }

    [[nodiscard]] static U32
    find_material_dependency_handle_index(const DynamicArray<MaterialAssetHandle> &materials,
                                          MaterialAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return INVALID_RENDER_SUBMESH_MATERIAL_INDEX;
        }

        for (USize i = 0; i < materials.size(); ++i) {
            if (materials[i].key == handle.key) {
                return static_cast<U32>(i);
            }
        }

        return INVALID_RENDER_SUBMESH_MATERIAL_INDEX;
    }

    [[nodiscard]] U32
    find_material_dependency_asset_index(const DynamicArray<MaterialAssetHandle> &materials,
                                         AssetId id) const noexcept {
        if (!id.is_valid()) {
            return INVALID_RENDER_SUBMESH_MATERIAL_INDEX;
        }

        for (USize i = 0; i < materials.size(); ++i) {
            const MaterialAsset *record = m_materials.get_data(materials[i].key);
            if (!record) {
                continue;
            }

            if (!record->runtime_asset && record->cache_key == id.value) {
                return static_cast<U32>(i);
            }
        }

        return INVALID_RENDER_SUBMESH_MATERIAL_INDEX;
    }

    [[nodiscard]] bool retain_texture(TextureAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return false;
        }

        TextureAsset *record = m_textures.get_data(handle.key);
        if (!record) {
            return false;
        }

        ++record->ref_count;
        return true;
    }

    [[nodiscard]] bool retain_material(MaterialAssetHandle handle) noexcept {
        if (!handle.is_valid()) {
            return false;
        }

        MaterialAsset *record = m_materials.get_data(handle.key);
        if (!record) {
            return false;
        }

        ++record->ref_count;
        return true;
    }

    struct ByteReader {
        Slice<const Byte> bytes{};
        USize cursor{0};

        explicit ByteReader(Slice<const Byte> source) noexcept
            : bytes(source) {
        }

        bool read_exact(void *dst, USize size) noexcept {
            if (size == 0) {
                return true;
            }

            if (!dst) {
                return false;
            }

            if (cursor > bytes.size() || size > bytes.size() - cursor) {
                return false;
            }

            fr::mem::copy_raw_range(bytes.data() + cursor, size, reinterpret_cast<Byte *>(dst));
            cursor += size;

            return true;
        }

        template <typename T>
        bool read_object(T &out) noexcept {
            FR_STATIC_ASSERT(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return read_exact(&out, sizeof(T));
        }

        template <typename T>
        bool read_array(T *dst, USize count) noexcept {
            FR_STATIC_ASSERT(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

            if (count == 0) {
                return true;
            }

            if (!dst) {
                return false;
            }

            return read_exact(dst, count * sizeof(T));
        }
    };

    struct LoadedTextureFile {
        U32 width{0};
        U32 height{0};
        U32 mip_levels{1};
        TextureFormat gpu_format{TextureFormat::R8G8B8A8_UNorm};
        DynamicArray<Byte> pixels;

        explicit LoadedTextureFile(Alloc *alloc) noexcept
            : pixels(alloc) {
            FR_ASSERT(alloc, "allocator must be non-null");
        }
    };

    struct LoadedMeshFile {
        DynamicArray<CookedSubMesh> submeshes;
        DynamicArray<CookedVertex> vertices;
        DynamicArray<U32> indices;

        Vec3 aabb_min{0.0f};
        Vec3 aabb_max{0.0f};

        explicit LoadedMeshFile(Alloc *alloc) noexcept
            : submeshes(alloc),
              vertices(alloc),
              indices(alloc) {
            FR_ASSERT(alloc, "allocator must be non-null");
        }
    };

    enum class PendingAssetError : U8 {
        None,
        ResolveFailed,
        ReadFailed,
        DecodeFailed,
        UploadFailed,
    };

    struct PendingMeshLoad {
        U64 cache_key{0};
        LoadedMeshFile loaded;
        bool ok{false};
        PendingAssetError error{PendingAssetError::None};

        explicit PendingMeshLoad(Alloc *alloc) noexcept
            : loaded(alloc) {
            FR_ASSERT(alloc, "allocator must be non-null");
        }
    };

    struct PendingTextureLoad {
        U64 cache_key{0};
        LoadedTextureFile loaded;
        bool ok{false};
        PendingAssetError error{PendingAssetError::None};

        explicit PendingTextureLoad(Alloc *alloc) noexcept
            : loaded(alloc) {
            FR_ASSERT(alloc, "allocator must be non-null");
        }
    };

    struct PendingMaterialLoad {
        U64 cache_key{0};
        MaterialAssetData data{};
        bool ok{false};
        PendingAssetError error{PendingAssetError::None};
    };

    struct PendingShaderLoad {
        U64 cache_key{0};
        ShaderSourceBundle sources;
        String debug_name;
        bool ok{false};
        PendingAssetError error{PendingAssetError::None};

        explicit PendingShaderLoad(Alloc *alloc) noexcept
            : sources(alloc),
              debug_name(alloc) {
            FR_ASSERT(alloc, "allocator must be non-null");
        }
    };

    [[nodiscard]] static U32
    find_material_dependency_index(const DynamicArray<AssetId> &material_ids, AssetId id) noexcept {
        if (!id.is_valid()) {
            return INVALID_RENDER_SUBMESH_MATERIAL_INDEX;
        }

        for (USize i = 0; i < material_ids.size(); ++i) {
            if (material_ids[i] == id) {
                return static_cast<U32>(i);
            }
        }

        return INVALID_RENDER_SUBMESH_MATERIAL_INDEX;
    }

    [[nodiscard]] static AssetLoadState get_load_state(const HashMap<U64, AssetLoadState> &states,
                                                       U64 cache_key) noexcept {
        auto found = states.find(cache_key);
        return found.is_some() ? *found.unwrap() : AssetLoadState::Unloaded;
    }

    [[nodiscard]] static bool can_start_async_load(HashMap<U64, AssetLoadState> &states,
                                                   U64 cache_key, bool retry_failed) noexcept {
        auto found = states.find(cache_key);
        if (!found.is_some()) {
            return true;
        }

        AssetLoadState state = *found.unwrap();

        if (state == AssetLoadState::Failed && retry_failed) {
            return true;
        }

        return state == AssetLoadState::Unloaded;
    }

    void set_mesh_state(U64 cache_key, AssetLoadState state) noexcept {
        m_mesh_async_state[cache_key] = state;
    }

    void set_texture_state(U64 cache_key, AssetLoadState state) noexcept {
        m_texture_async_state[cache_key] = state;
    }

    void set_material_state(U64 cache_key, AssetLoadState state) noexcept {
        m_material_async_state[cache_key] = state;
    }

    void set_shader_state(U64 cache_key, AssetLoadState state) noexcept {
        m_shader_async_state[cache_key] = state;
    }

    [[nodiscard]] MeshAssetHandle get_cached_mesh(U64 cache_key) noexcept {
        auto cached_opt = m_mesh_cache.find(cache_key);
        if (!cached_opt.is_some()) {
            return {};
        }

        MeshAssetHandle handle = *cached_opt.unwrap();

        if (MeshAsset *record = m_meshes.get_data(handle.key)) {
            ++record->ref_count;
            return handle;
        }

        m_mesh_cache.remove(cache_key);
        return {};
    }

    [[nodiscard]] TextureAssetHandle get_cached_texture(U64 cache_key) noexcept {
        auto cached_opt = m_texture_cache.find(cache_key);
        if (!cached_opt.is_some()) {
            return {};
        }

        TextureAssetHandle handle = *cached_opt.unwrap();

        if (TextureAsset *record = m_textures.get_data(handle.key)) {
            ++record->ref_count;
            return handle;
        }

        m_texture_cache.remove(cache_key);
        return {};
    }

    [[nodiscard]] MaterialAssetHandle get_cached_material(U64 cache_key) noexcept {
        auto cached_opt = m_material_cache.find(cache_key);
        if (!cached_opt.is_some()) {
            return {};
        }

        MaterialAssetHandle handle = *cached_opt.unwrap();

        if (MaterialAsset *record = m_materials.get_data(handle.key)) {
            ++record->ref_count;
            return handle;
        }

        m_material_cache.remove(cache_key);
        return {};
    }

    [[nodiscard]] ShaderAssetHandle get_cached_shader(U64 cache_key) noexcept {
        auto cached_opt = m_shader_cache.find(cache_key);
        if (!cached_opt.is_some()) {
            return {};
        }

        ShaderAssetHandle handle = *cached_opt.unwrap();

        if (ShaderAsset *record = m_shaders.get_data(handle.key)) {
            ++record->ref_count;
            return handle;
        }

        m_shader_cache.remove(cache_key);
        return {};
    }

    [[nodiscard]] const AssetRecord *resolve_record(AssetId id,
                                                    AssetKind expected_kind) const noexcept {
        if (!m_registry) {
            FR_LOG_ERR("AssetManager has no AssetRegistry.");
            return nullptr;
        }

        const AssetRecord *record = m_registry->find(id);
        if (!record) {
            return nullptr;
        }

        if (record->kind != expected_kind) {
            FR_LOG_ERR("Asset kind mismatch for asset id: {}", id.value);
            return nullptr;
        }

        if (record->location_kind == AssetLocationKind::None) {
            return nullptr;
        }

        return record;
    }

    MeshAssetHandle upload_loaded_mesh(LoadedMeshFile &&loaded, U64 cache_key) noexcept {
        if (MeshAssetHandle cached = try_get_mesh(AssetId::from_hash(cache_key));
            cached.is_valid()) {
            if (MeshAsset *record = m_meshes.get_data(cached.key)) {
                ++record->ref_count;
            }
            return cached;
        }

        RenderMeshData mesh_data(m_alloc);

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
                mesh_data.vbo = {};
            }

            if (mesh_data.ibo.is_valid()) {
                m_device->destroy_buffer(mesh_data.ibo);
                mesh_data.ibo = {};
            }

            FR_LOG_ERR("Failed to create GPU buffers for mesh asset.");
            return {};
        }

        mesh_data.submeshes.reserve(loaded.submeshes.size());

        DynamicArray<MaterialAssetHandle> material_deps(m_alloc);
        DynamicArray<AssetId> material_dep_ids(m_alloc);

        material_deps.reserve(loaded.submeshes.size());
        material_dep_ids.reserve(loaded.submeshes.size());

        for (USize i = 0; i < loaded.submeshes.size(); ++i) {
            const CookedSubMesh &cooked = loaded.submeshes[i];

            RenderSubMesh submesh{};
            submesh.index_count = cooked.index_count;
            submesh.index_offset = cooked.index_offset;
            submesh.vertex_offset = cooked.vertex_offset;

            submesh.transform = glm::make_mat4(cooked.transform);

            submesh.aabb_min = Vec3(cooked.aabb_min[0], cooked.aabb_min[1], cooked.aabb_min[2]);
            submesh.aabb_max = Vec3(cooked.aabb_max[0], cooked.aabb_max[1], cooked.aabb_max[2]);

            submesh.material_id = cooked.material_id;
            submesh.material_index = INVALID_RENDER_SUBMESH_MATERIAL_INDEX;

            if (cooked.material_id.is_valid()) {
                U32 material_index =
                    find_material_dependency_index(material_dep_ids, cooked.material_id);

                if (material_index == INVALID_RENDER_SUBMESH_MATERIAL_INDEX) {
                    MaterialAssetHandle material = load_material(cooked.material_id);

                    if (material.is_valid()) {
                        material_index = static_cast<U32>(material_deps.size());

                        material_deps.push_back(material);
                        material_dep_ids.push_back(cooked.material_id);
                    }
                }

                submesh.material_index = material_index;
            }

            if (cooked.pass_type == 0) {
                submesh.pass_type = RenderPass::Opaque;
            } else if (cooked.pass_type == 1) {
                submesh.pass_type = RenderPass::Masked;
            } else {
                submesh.pass_type = RenderPass::Transparent;
            }

            mesh_data.submeshes.push_back(submesh);
        }

        MeshAsset new_asset{};
        new_asset.data = std::move(mesh_data);
        new_asset.material_deps = std::move(material_deps);
        new_asset.ref_count = 1;
        new_asset.cache_key = cache_key;
        new_asset.runtime_asset = false;

        MeshAssetHandle handle{m_meshes.add(std::move(new_asset))};
        m_mesh_cache.insert(cache_key, handle);

        return handle;
    }

    TextureAssetHandle upload_loaded_texture(LoadedTextureFile &&loaded, U64 cache_key) noexcept {
        if (TextureAssetHandle cached = try_get_texture(AssetId::from_hash(cache_key));
            cached.is_valid()) {
            if (TextureAsset *record = m_textures.get_data(cached.key)) {
                ++record->ref_count;
            }
            return cached;
        }

        TextureHandle gpu_handle = m_device->create_texture_2d(
            loaded.width, loaded.height, loaded.mip_levels, loaded.gpu_format,
            Slice<const Byte>(loaded.pixels.data(), loaded.pixels.size()));

        if (!gpu_handle.is_valid()) {
            FR_LOG_ERR("Failed to create GPU texture for asset.");
            return {};
        }

        TextureAsset new_asset{};
        new_asset.handle = gpu_handle;
        new_asset.ref_count = 1;
        new_asset.cache_key = cache_key;
        new_asset.runtime_asset = false;

        TextureAssetHandle handle{m_textures.add(new_asset)};
        m_texture_cache.insert(cache_key, handle);

        return handle;
    }

    MaterialAssetHandle upload_loaded_material(const MaterialAssetData &data,
                                               U64 cache_key) noexcept {
        if (MaterialAssetHandle cached = try_get_material(AssetId::from_hash(cache_key));
            cached.is_valid()) {
            if (MaterialAsset *record = m_materials.get_data(cached.key)) {
                ++record->ref_count;
            }
            return cached;
        }

        MaterialAsset new_asset{};
        new_asset.data = data;
        new_asset.ref_count = 1;
        new_asset.cache_key = cache_key;
        new_asset.runtime_asset = false;

        if (data.albedo_texture.is_valid()) {
            new_asset.albedo_texture = load_texture(data.albedo_texture);
        }

        if (data.normal_texture.is_valid()) {
            new_asset.normal_texture = load_texture(data.normal_texture);
        }

        if (data.extra_texture.is_valid()) {
            new_asset.extra_texture = load_texture(data.extra_texture);
        }

        MaterialAssetHandle handle{m_materials.add(std::move(new_asset))};
        m_material_cache.insert(cache_key, handle);

        return handle;
    }

    ShaderAssetHandle upload_loaded_shader(ShaderSourceBundle &&sources, U64 cache_key,
                                           StringView debug_name = {}) noexcept {
        if (ShaderAssetHandle cached = try_get_shader(AssetId::from_hash(cache_key));
            cached.is_valid()) {
            if (ShaderAsset *record = m_shaders.get_data(cached.key)) {
                ++record->ref_count;
            }
            return cached;
        }

        ShaderHandle gpu_shader =
            m_device->create_shader(sources.vertex.view(), sources.fragment.view(), debug_name);

        if (!gpu_shader.is_valid()) {
            if (!debug_name.is_empty()) {
                FR_LOG_ERR("Failed to create GPU shader from asset: {}", debug_name);
            } else {
                FR_LOG_ERR("Failed to create GPU shader from asset id: {}", cache_key);
            }

            return {};
        }

        ShaderAsset new_asset{};
        new_asset.handle = gpu_shader;
        new_asset.ref_count = 1;
        new_asset.cache_key = cache_key;

        ShaderAssetHandle handle{m_shaders.add(std::move(new_asset))};
        m_shader_cache.insert(cache_key, handle);

        return handle;
    }

    void push_pending_mesh(PendingMeshLoad &&pending) noexcept {
        {
            std::lock_guard lock(m_pending_mutex);
            m_pending_mesh_loads.push_back(std::move(pending));
        }

        set_mesh_state_thread_unsafe_hint(pending.cache_key, AssetLoadState::ReadyForGpu);
    }

    void push_pending_texture(PendingTextureLoad &&pending) noexcept {
        {
            std::lock_guard lock(m_pending_mutex);
            m_pending_texture_loads.push_back(std::move(pending));
        }

        set_texture_state_thread_unsafe_hint(pending.cache_key, AssetLoadState::ReadyForGpu);
    }

    void push_pending_material(PendingMaterialLoad &&pending) noexcept {
        {
            std::lock_guard lock(m_pending_mutex);
            m_pending_material_loads.push_back(std::move(pending));
        }

        set_material_state_thread_unsafe_hint(pending.cache_key, AssetLoadState::ReadyForGpu);
    }

    void push_pending_shader(PendingShaderLoad &&pending) noexcept {
        {
            std::lock_guard lock(m_pending_mutex);
            m_pending_shader_loads.push_back(std::move(pending));
        }

        set_shader_state_thread_unsafe_hint(pending.cache_key, AssetLoadState::ReadyForGpu);
    }

    void set_mesh_state_thread_unsafe_hint(U64, AssetLoadState) noexcept {
    }

    void set_texture_state_thread_unsafe_hint(U64, AssetLoadState) noexcept {
    }

    void set_material_state_thread_unsafe_hint(U64, AssetLoadState) noexcept {
    }

    void set_shader_state_thread_unsafe_hint(U64, AssetLoadState) noexcept {
    }

    USize process_pending_textures(USize max_uploads) noexcept {
        if (max_uploads == 0) {
            return 0;
        }

        DynamicArray<PendingTextureLoad> pending(m_alloc);
        {
            std::lock_guard lock(m_pending_mutex);
            pending = std::move(m_pending_texture_loads);
            m_pending_texture_loads = DynamicArray<PendingTextureLoad>(m_alloc);
            m_pending_texture_loads.reserve(128);
        }

        USize processed = 0;
        for (USize i = 0; i < pending.size() && processed < max_uploads; ++i) {
            PendingTextureLoad &job = pending[i];

            if (!job.ok) {
                FR_LOG_ERR("Async texture load failed for asset id: {}", job.cache_key);
                set_texture_state(job.cache_key, AssetLoadState::Failed);
                ++processed;
                continue;
            }

            TextureAssetHandle handle = upload_loaded_texture(std::move(job.loaded), job.cache_key);
            set_texture_state(job.cache_key,
                              handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
            ++processed;
        }

        requeue_unprocessed_textures(pending, processed);
        return processed;
    }

    USize process_pending_materials(USize max_uploads) noexcept {
        if (max_uploads == 0) {
            return 0;
        }

        DynamicArray<PendingMaterialLoad> pending(m_alloc);
        {
            std::lock_guard lock(m_pending_mutex);
            pending = std::move(m_pending_material_loads);
            m_pending_material_loads = DynamicArray<PendingMaterialLoad>(m_alloc);
            m_pending_material_loads.reserve(64);
        }

        USize processed = 0;
        for (USize i = 0; i < pending.size() && processed < max_uploads; ++i) {
            PendingMaterialLoad &job = pending[i];

            if (!job.ok) {
                FR_LOG_ERR("Async material load failed for asset id: {}", job.cache_key);
                set_material_state(job.cache_key, AssetLoadState::Failed);
                ++processed;
                continue;
            }

            MaterialAssetHandle handle = upload_loaded_material(job.data, job.cache_key);
            set_material_state(job.cache_key,
                               handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
            ++processed;
        }

        requeue_unprocessed_materials(pending, processed);
        return processed;
    }

    USize process_pending_meshes(USize max_uploads) noexcept {
        if (max_uploads == 0) {
            return 0;
        }

        DynamicArray<PendingMeshLoad> pending(m_alloc);
        {
            std::lock_guard lock(m_pending_mutex);
            pending = std::move(m_pending_mesh_loads);
            m_pending_mesh_loads = DynamicArray<PendingMeshLoad>(m_alloc);
            m_pending_mesh_loads.reserve(64);
        }

        USize processed = 0;
        for (USize i = 0; i < pending.size() && processed < max_uploads; ++i) {
            PendingMeshLoad &job = pending[i];

            if (!job.ok) {
                FR_LOG_ERR("Async mesh load failed for asset id: {}", job.cache_key);
                set_mesh_state(job.cache_key, AssetLoadState::Failed);
                ++processed;
                continue;
            }

            MeshAssetHandle handle = upload_loaded_mesh(std::move(job.loaded), job.cache_key);
            set_mesh_state(job.cache_key,
                           handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
            ++processed;
        }

        requeue_unprocessed_meshes(pending, processed);
        return processed;
    }

    USize process_pending_shaders(USize max_uploads) noexcept {
        if (max_uploads == 0) {
            return 0;
        }

        DynamicArray<PendingShaderLoad> pending(m_alloc);
        {
            std::lock_guard lock(m_pending_mutex);
            pending = std::move(m_pending_shader_loads);
            m_pending_shader_loads = DynamicArray<PendingShaderLoad>(m_alloc);
            m_pending_shader_loads.reserve(32);
        }

        USize processed = 0;
        for (USize i = 0; i < pending.size() && processed < max_uploads; ++i) {
            PendingShaderLoad &job = pending[i];

            if (!job.ok) {
                FR_LOG_ERR("Async shader load failed for asset id: {}", job.cache_key);
                set_shader_state(job.cache_key, AssetLoadState::Failed);
                ++processed;
                continue;
            }

            ShaderAssetHandle handle =
                upload_loaded_shader(std::move(job.sources), job.cache_key, job.debug_name.view());

            set_shader_state(job.cache_key,
                             handle.is_valid() ? AssetLoadState::Loaded : AssetLoadState::Failed);
            ++processed;
        }

        requeue_unprocessed_shaders(pending, processed);
        return processed;
    }

    void requeue_unprocessed_textures(DynamicArray<PendingTextureLoad> &pending,
                                      USize first_unprocessed) noexcept {
        if (first_unprocessed >= pending.size()) {
            return;
        }

        std::lock_guard lock(m_pending_mutex);
        for (USize i = first_unprocessed; i < pending.size(); ++i) {
            m_pending_texture_loads.push_back(std::move(pending[i]));
        }
    }

    void requeue_unprocessed_materials(DynamicArray<PendingMaterialLoad> &pending,
                                       USize first_unprocessed) noexcept {
        if (first_unprocessed >= pending.size()) {
            return;
        }

        std::lock_guard lock(m_pending_mutex);
        for (USize i = first_unprocessed; i < pending.size(); ++i) {
            m_pending_material_loads.push_back(std::move(pending[i]));
        }
    }

    void requeue_unprocessed_meshes(DynamicArray<PendingMeshLoad> &pending,
                                    USize first_unprocessed) noexcept {
        if (first_unprocessed >= pending.size()) {
            return;
        }

        std::lock_guard lock(m_pending_mutex);
        for (USize i = first_unprocessed; i < pending.size(); ++i) {
            m_pending_mesh_loads.push_back(std::move(pending[i]));
        }
    }

    void requeue_unprocessed_shaders(DynamicArray<PendingShaderLoad> &pending,
                                     USize first_unprocessed) noexcept {
        if (first_unprocessed >= pending.size()) {
            return;
        }

        std::lock_guard lock(m_pending_mutex);
        for (USize i = first_unprocessed; i < pending.size(); ++i) {
            m_pending_shader_loads.push_back(std::move(pending[i]));
        }
    }

    void destroy_all_runtime_resources() noexcept {
        for (auto pair : m_mesh_cache) {
            MeshAssetHandle handle = pair.second();
            MeshAsset *record = m_meshes.get_data(handle.key);

            if (!record) {
                continue;
            }

            if (record->data.vbo.is_valid()) {
                m_device->destroy_buffer(record->data.vbo);
                record->data.vbo = {};
            }

            if (record->data.ibo.is_valid()) {
                m_device->destroy_buffer(record->data.ibo);
                record->data.ibo = {};
            }
        }

        for (auto pair : m_texture_cache) {
            TextureAssetHandle handle = pair.second();
            TextureAsset *record = m_textures.get_data(handle.key);

            if (!record) {
                continue;
            }

            if (record->handle.is_valid()) {
                m_device->destroy_texture(record->handle);
                record->handle = {};
            }
        }

        for (auto pair : m_shader_cache) {
            ShaderAssetHandle handle = pair.second();
            ShaderAsset *record = m_shaders.get_data(handle.key);

            if (!record) {
                continue;
            }

            if (record->handle.is_valid()) {
                m_device->destroy_shader(record->handle);
                record->handle = {};
            }
        }
    }

    bool load_texture_file(Slice<const Byte> bytes, LoadedTextureFile &out) noexcept {
        ByteReader reader(bytes);

        CookedTextureHeader header{};
        if (!reader.read_object(header)) {
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

        if (header.format == CookedTextureFormat::RGBA8_UNORM) {
            gpu_format = TextureFormat::R8G8B8A8_UNorm;
            expected_pixel_size = 4;
        } else if (header.format == CookedTextureFormat::RGBA8_SRGB) {
            gpu_format = TextureFormat::R8G8B8A8_SRGB;
            expected_pixel_size = 4;
        } else if (header.format == CookedTextureFormat::RGBA32F_HDR) {
            gpu_format = TextureFormat::R32G32B32A32_Float;
            expected_pixel_size = sizeof(F32) * 4;
        } else {
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

        if (!reader.read_exact(out.pixels.data(), header.image_data_size)) {
            out.pixels.clear();
            return false;
        }

        return true;
    }

    bool load_mesh_file(Slice<const Byte> bytes, LoadedMeshFile &out) noexcept {
        ByteReader reader(bytes);

        CookedMeshHeader header{};
        if (!reader.read_object(header)) {
            return false;
        }

        if (header.base.verify[0] != 'F' || header.base.verify[1] != 'M' ||
            header.base.verify[2] != 'S' || header.base.verify[3] != 'H') {
            return false;
        }

        if (header.base.version != 2) {
            return false;
        }

        if (header.reserved0 != 0) {
            return false;
        }

        if (header.vertex_count == 0 || header.index_count == 0 || header.submesh_count == 0) {
            return false;
        }

        const USize expected_submesh_data_size =
            static_cast<USize>(header.submesh_count) * sizeof(CookedSubMesh);

        const USize expected_vertex_data_size =
            static_cast<USize>(header.vertex_count) * sizeof(CookedVertex);

        const USize expected_index_data_size = static_cast<USize>(header.index_count) * sizeof(U32);

        if (expected_submesh_data_size > static_cast<USize>(0xFFFFFFFFu) ||
            expected_vertex_data_size > static_cast<USize>(0xFFFFFFFFu) ||
            expected_index_data_size > static_cast<USize>(0xFFFFFFFFu)) {
            return false;
        }

        if (static_cast<USize>(header.submesh_data_size) != expected_submesh_data_size ||
            static_cast<USize>(header.vertex_data_size) != expected_vertex_data_size ||
            static_cast<USize>(header.index_data_size) != expected_index_data_size) {
            return false;
        }

        out.submeshes.clear();
        out.vertices.clear();
        out.indices.clear();

        out.submeshes.grow_default(header.submesh_count);
        if (!reader.read_array(out.submeshes.data(), header.submesh_count)) {
            out.submeshes.clear();
            return false;
        }

        out.vertices.grow_default(header.vertex_count);
        if (!reader.read_array(out.vertices.data(), header.vertex_count)) {
            out.submeshes.clear();
            out.vertices.clear();
            return false;
        }

        out.indices.grow_default(header.index_count);
        if (!reader.read_array(out.indices.data(), header.index_count)) {
            out.submeshes.clear();
            out.vertices.clear();
            out.indices.clear();
            return false;
        }

        out.aabb_min = Vec3(header.aabb_min[0], header.aabb_min[1], header.aabb_min[2]);
        out.aabb_max = Vec3(header.aabb_max[0], header.aabb_max[1], header.aabb_max[2]);

        for (USize submesh_idx = 0; submesh_idx < out.submeshes.size(); ++submesh_idx) {
            const CookedSubMesh &submesh = out.submeshes[submesh_idx];

            if (submesh.index_count == 0) {
                return false;
            }

            if (submesh.pass_type > 2) {
                return false;
            }

            const USize index_begin = static_cast<USize>(submesh.index_offset);
            const USize index_count = static_cast<USize>(submesh.index_count);

            if (index_begin >= out.indices.size()) {
                return false;
            }

            if (index_count > out.indices.size() - index_begin) {
                return false;
            }

            const USize vertex_offset = static_cast<USize>(submesh.vertex_offset);
            if (vertex_offset >= out.vertices.size()) {
                return false;
            }

            const USize max_local_vertex_index = out.vertices.size() - vertex_offset;
            const USize index_end = index_begin + index_count;

            for (USize i = index_begin; i < index_end; ++i) {
                const USize local_index = static_cast<USize>(out.indices[i]);

                if (local_index >= max_local_vertex_index) {
                    return false;
                }
            }
        }

        return true;
    }

private:
    RenderDevice *m_device{nullptr};
    Alloc *m_alloc{nullptr};

    const AssetRegistry *m_registry{nullptr};
    AssetStorage *m_storage{nullptr};

    SlotMap<MeshAsset> m_meshes;
    SlotMap<TextureAsset> m_textures;
    SlotMap<MaterialAsset> m_materials;
    SlotMap<ShaderAsset> m_shaders;

    HashMap<U64, MeshAssetHandle> m_mesh_cache;
    HashMap<U64, TextureAssetHandle> m_texture_cache;
    HashMap<U64, MaterialAssetHandle> m_material_cache;
    HashMap<U64, ShaderAssetHandle> m_shader_cache;

    HashMap<U64, AssetLoadState> m_mesh_async_state;
    HashMap<U64, AssetLoadState> m_texture_async_state;
    HashMap<U64, AssetLoadState> m_material_async_state;
    HashMap<U64, AssetLoadState> m_shader_async_state;

    std::mutex m_pending_mutex{};

    DynamicArray<PendingMeshLoad> m_pending_mesh_loads;
    DynamicArray<PendingTextureLoad> m_pending_texture_loads;
    DynamicArray<PendingMaterialLoad> m_pending_material_loads;
    DynamicArray<PendingShaderLoad> m_pending_shader_loads;

    U64 m_next_runtime_mesh_cache_key{0x8000000000000000ull};
    U64 m_next_runtime_texture_cache_key{0x9000000000000000ull};
    U64 m_next_runtime_material_cache_key{0xA000000000000000ull};
};

} // namespace fr
