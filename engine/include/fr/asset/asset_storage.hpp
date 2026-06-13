/**
 * @file asset_storage.hpp
 * @author Tfoedy
 * @brief Physical cooked asset byte access.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_pack.hpp"
#include "fr/asset/asset_registry.hpp"

#include "fr/logger/logger.hpp"

namespace fr {

/**
 * @brief Reads cooked asset bytes from loose files and mounted packs.
 *
 * @details
 * AssetStorage is the physical IO layer. It does not decode asset formats and does not create GPU
 * resources.
 */
class AssetStorage {
public:
    explicit AssetStorage(Alloc *alloc) noexcept
        : m_alloc(alloc),
          m_packs(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");

        m_packs.reserve(16);
    }

    AssetStorage(const AssetStorage &) = delete;
    AssetStorage(AssetStorage &&) = delete;
    AssetStorage &operator=(const AssetStorage &) = delete;
    AssetStorage &operator=(AssetStorage &&) = delete;

    /**
     * @brief Mounts a pack file and returns its runtime pack index.
     */
    [[nodiscard]] bool mount_pack(StringView path, U32 &out_pack_index) noexcept {
        out_pack_index = 0xFFFFFFFFu;

        AssetPack pack(m_alloc);
        if (!pack.mount_from_path(path)) {
            return false;
        }

        out_pack_index = static_cast<U32>(m_packs.size());
        m_packs.push_back(std::move(pack));

        return true;
    }

    /**
     * @brief Registers assets from a mounted pack.
     */
    [[nodiscard]] bool register_pack_assets(AssetRegistry &registry,
                                            U32 pack_index) const noexcept {
        if (pack_index >= m_packs.size()) {
            return false;
        }

        const AssetPack &pack = m_packs[pack_index];
        if (!pack.is_mounted()) {
            return false;
        }

        bool ok = true;

        for (const CookedAssetPackEntry &entry : pack.entries()) {
            ok = registry.register_pack_asset(entry.id, entry.kind, pack_index, entry.offset,
                                              entry.packed_size, entry.unpacked_size,
                                              entry.content_hash) &&
                 ok;
        }

        if (!ok) {
            FR_LOG_ERR("Failed to register one or more assets from pack index {}.", pack_index);
        }

        return ok;
    }

    /**
     * @brief Reads bytes for a resolved asset record.
     */
    [[nodiscard]] bool read_record_bytes(const AssetRecord &record,
                                         DynamicArray<Byte> &out) noexcept {
        out.clear();

        switch (record.location_kind) {
        case AssetLocationKind::LooseFile:
            return read_loose_file(record.loose_path.view(), out);

        case AssetLocationKind::PackFile:
            return read_pack_entry(record, out);

        case AssetLocationKind::None:
        default:
            FR_LOG_ERR("Asset has no readable physical location. Asset id: {}", record.id.value);
            return false;
        }
    }

    [[nodiscard]] USize mounted_pack_count() const noexcept {
        return m_packs.size();
    }

private:
    [[nodiscard]] bool read_loose_file(StringView path, DynamicArray<Byte> &out) noexcept {
        if (path.is_empty()) {
            return false;
        }

        String path_str = String::from_view(m_alloc, path);

        auto file_bytes = file::read_all_bytes(m_alloc, path_str);
        if (!file_bytes.is_some()) {
            FR_LOG_ERR("Failed to read loose cooked asset: {}", path);
            return false;
        }

        out = std::move(file_bytes.unwrap());
        return true;
    }

    [[nodiscard]] bool read_pack_entry(const AssetRecord &record,
                                       DynamicArray<Byte> &out) noexcept {
        if (record.pack_index >= m_packs.size()) {
            FR_LOG_ERR("Invalid asset pack index {} for asset {}.", record.pack_index,
                       record.id.value);
            return false;
        }

        const AssetPack &pack = m_packs[record.pack_index];

        CookedAssetPackEntry entry{};
        entry.id = record.id;
        entry.kind = record.kind;
        entry.offset = record.pack_offset;
        entry.packed_size = record.packed_size;
        entry.unpacked_size = record.unpacked_size;
        entry.content_hash = record.content_hash;
        entry.compression = CookedAssetPackCompression::None;

        return pack.read_entry_bytes(entry, out);
    }

private:
    Alloc *m_alloc{nullptr};

    DynamicArray<AssetPack> m_packs;
};

} // namespace fr
