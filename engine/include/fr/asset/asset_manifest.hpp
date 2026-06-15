/**
 * @file asset_manifest.hpp
 * @author Tfoedy
 * @brief Runtime asset manifest loading.
 */

#pragma once

#include <type_traits>

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_manifest_format.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/asset/asset_storage.hpp"

#include "fr/logger/logger.hpp"

namespace fr {
namespace impl {

template <typename T>
[[nodiscard]] inline bool read_manifest_object(Slice<const Byte> bytes, USize offset,
                                               T &out) noexcept {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
        return false;
    }

    fr::mem::copy_raw_range(bytes.data() + offset, sizeof(T), reinterpret_cast<Byte *>(&out));
    return true;
}

[[nodiscard]] inline bool validate_manifest_range(USize file_size, U32 offset, U32 size) noexcept {
    const USize range_offset = static_cast<USize>(offset);
    const USize range_size = static_cast<USize>(size);

    if (range_offset > file_size) {
        return false;
    }

    return range_size <= file_size - range_offset;
}

[[nodiscard]] inline bool verify_manifest_header(const CookedAssetManifestHeader &header) noexcept {
    return header.verify[0] == 'F' && header.verify[1] == 'M' && header.verify[2] == 'A' &&
           header.verify[3] == 'N' && header.version == 1;
}

[[nodiscard]] inline bool
validate_manifest_header(Slice<const Byte> bytes,
                         const CookedAssetManifestHeader &header) noexcept {
    if (!verify_manifest_header(header)) {
        return false;
    }

    const USize expected_pack_table_size =
        static_cast<USize>(header.pack_count) * sizeof(CookedAssetManifestPackRecord);

    const USize expected_loose_table_size =
        static_cast<USize>(header.loose_count) * sizeof(CookedAssetManifestLooseRecord);

    if (expected_pack_table_size > static_cast<USize>(0xFFFFFFFFu) ||
        expected_loose_table_size > static_cast<USize>(0xFFFFFFFFu)) {
        return false;
    }

    if (static_cast<USize>(header.pack_table_size) != expected_pack_table_size) {
        return false;
    }

    if (static_cast<USize>(header.loose_table_size) != expected_loose_table_size) {
        return false;
    }

    if (!validate_manifest_range(bytes.size(), header.pack_table_offset, header.pack_table_size)) {
        return false;
    }

    if (!validate_manifest_range(bytes.size(), header.loose_table_offset,
                                 header.loose_table_size)) {
        return false;
    }

    if (!validate_manifest_range(bytes.size(), header.string_data_offset,
                                 header.string_data_size)) {
        return false;
    }

    if (header.pack_count == 0 && header.loose_count == 0) {
        return false;
    }

    return true;
}

[[nodiscard]] inline bool validate_manifest_string_range(const CookedAssetManifestHeader &header,
                                                         U32 offset, U32 size) noexcept {
    if (size == 0) {
        return false;
    }

    if (offset > header.string_data_size) {
        return false;
    }

    return size <= header.string_data_size - offset;
}

[[nodiscard]] inline StringView manifest_string_view(Slice<const Byte> bytes,
                                                     const CookedAssetManifestHeader &header,
                                                     U32 offset, U32 size) noexcept {
    if (!validate_manifest_string_range(header, offset, size)) {
        return {};
    }

    const USize string_offset =
        static_cast<USize>(header.string_data_offset) + static_cast<USize>(offset);

    return StringView(reinterpret_cast<const char *>(bytes.data() + string_offset), size);
}

[[nodiscard]] inline bool is_manifest_path_separator(char c) noexcept {
    return c == '/' || c == '\\';
}

[[nodiscard]] inline bool is_manifest_absolute_path(StringView path) noexcept {
    if (path.is_empty()) {
        return false;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return true;
    }

    if (path.size() >= 3 && path[1] == ':' && is_manifest_path_separator(path[2])) {
        const char drive = path[0];
        return (drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z');
    }

    return false;
}

[[nodiscard]] inline String normalize_manifest_path(Alloc *alloc, StringView path) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    String result = String::from_view(alloc, path);
    file::normalize_unix(result);

    while (result.starts_with("./")) {
        result.erase(0, 2);
    }

    return result;
}

[[nodiscard]] inline String join_manifest_paths(Alloc *alloc, StringView base,
                                                StringView path) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (path.is_empty()) {
        return String(alloc);
    }

    if (is_manifest_absolute_path(path)) {
        return normalize_manifest_path(alloc, path);
    }

    if (base.is_empty()) {
        return normalize_manifest_path(alloc, path);
    }

    String joined = String::with_capacity(alloc, base.size() + path.size() + 1);
    joined.append(base);

    if (joined.size() > 0 && !is_manifest_path_separator(joined.back())) {
        joined.push_back('/');
    }

    joined.append(path);

    return normalize_manifest_path(alloc, joined.view());
}

/**
 * @brief Resolves a manifest file path against the manifest directory.
 *
 * @details
 * Manifest entries are normally relative to the manifest directory. Development manifests may also
 * contain project-root-relative paths such as "assets/models/...", so this helper falls back to the
 * raw normalized path when the joined path does not exist.
 */
[[nodiscard]] inline String resolve_manifest_file_path(Alloc *alloc, StringView base,
                                                       StringView path) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (path.is_empty()) {
        return String(alloc);
    }

    if (is_manifest_absolute_path(path)) {
        return normalize_manifest_path(alloc, path);
    }

    String joined = join_manifest_paths(alloc, base, path);
    if (file::exists(joined)) {
        return joined;
    }

    String raw = normalize_manifest_path(alloc, path);
    if (file::exists(raw)) {
        return raw;
    }

    return joined;
}

[[nodiscard]] inline String manifest_base_dir(Alloc *alloc, StringView manifest_path) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    String normalized = normalize_manifest_path(alloc, manifest_path);
    StringView parent = file::get_parent_path(normalized.view());

    if (parent.is_empty()) {
        return String(alloc);
    }

    return String::from_view(alloc, parent);
}

} // namespace impl

/**
 * @brief Loads an asset manifest into a registry and storage backend.
 *
 * @details
 * This function does not clear the registry or storage. Pack records are mounted and registered
 * first. Loose records are registered afterwards, so loose cooked files can override packed
 * assets. Manifest paths are resolved relative to the manifest directory unless absolute.
 */
[[nodiscard]] inline bool load_asset_manifest(Alloc *alloc, StringView manifest_path,
                                              AssetRegistry &registry,
                                              AssetStorage &storage) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (manifest_path.is_empty()) {
        FR_LOG_ERR("Cannot load asset manifest from an empty path.");
        return false;
    }

    String manifest_path_str = impl::normalize_manifest_path(alloc, manifest_path);
    String manifest_base_dir = impl::manifest_base_dir(alloc, manifest_path_str.view());

    auto file_bytes = file::read_all_bytes(alloc, manifest_path_str);

    if (!file_bytes.is_some()) {
        FR_LOG_ERR("Failed to read asset manifest: {}", manifest_path);
        return false;
    }

    const DynamicArray<Byte> &manifest_storage = file_bytes.unwrap();
    Slice<const Byte> bytes = manifest_storage.slice();

    CookedAssetManifestHeader header{};
    if (!impl::read_manifest_object(bytes, 0, header)) {
        FR_LOG_ERR("Asset manifest is too small: {}", manifest_path);
        return false;
    }

    if (!impl::validate_manifest_header(bytes, header)) {
        FR_LOG_ERR("Invalid asset manifest header: {}", manifest_path);
        return false;
    }

    for (U32 i = 0; i < header.pack_count; ++i) {
        const USize record_offset = static_cast<USize>(header.pack_table_offset) +
                                    static_cast<USize>(i) * sizeof(CookedAssetManifestPackRecord);

        CookedAssetManifestPackRecord record{};
        if (!impl::read_manifest_object(bytes, record_offset, record)) {
            FR_LOG_ERR("Failed to read asset manifest pack record {}.", i);
            return false;
        }

        StringView pack_path =
            impl::manifest_string_view(bytes, header, record.path_offset, record.path_size);

        if (pack_path.is_empty()) {
            FR_LOG_ERR("Asset manifest pack record {} has invalid path.", i);
            return false;
        }

        String resolved_pack_path =
            impl::resolve_manifest_file_path(alloc, manifest_base_dir.view(), pack_path);

        U32 pack_index = 0;
        if (!storage.mount_pack(resolved_pack_path.view(), pack_index)) {
            FR_LOG_ERR("Failed to mount asset pack from manifest: {}", resolved_pack_path.view());
            return false;
        }

        if (!storage.register_pack_assets(registry, pack_index)) {
            FR_LOG_ERR("Failed to register assets from manifest pack: {}",
                       resolved_pack_path.view());
            return false;
        }
    }

    for (U32 i = 0; i < header.loose_count; ++i) {
        const USize record_offset = static_cast<USize>(header.loose_table_offset) +
                                    static_cast<USize>(i) * sizeof(CookedAssetManifestLooseRecord);

        CookedAssetManifestLooseRecord record{};
        if (!impl::read_manifest_object(bytes, record_offset, record)) {
            FR_LOG_ERR("Failed to read asset manifest loose record {}.", i);
            return false;
        }

        if (!record.id.is_valid() || record.kind == AssetKind::Unknown) {
            FR_LOG_ERR("Asset manifest loose record {} has invalid asset metadata.", i);
            return false;
        }

        StringView loose_path =
            impl::manifest_string_view(bytes, header, record.path_offset, record.path_size);

        if (loose_path.is_empty()) {
            FR_LOG_ERR("Asset manifest loose record {} has invalid path.", i);
            return false;
        }

        String resolved_loose_path =
            impl::resolve_manifest_file_path(alloc, manifest_base_dir.view(), loose_path);

        if (!registry.register_loose_asset(record.id, record.kind, resolved_loose_path.view(),
                                           record.content_hash)) {
            FR_LOG_ERR("Failed to register loose asset from manifest: {}", record.id.value);
            return false;
        }
    }

    return true;
}

} // namespace fr
