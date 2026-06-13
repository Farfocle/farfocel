/**
 * @file asset_registry.hpp
 * @author Tfoedy
 * @brief Runtime registry for logical cooked assets.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_kind.hpp"

namespace fr {

/**
 * @brief Physical storage location kind for a cooked asset.
 */
enum class AssetLocationKind : U32 {
    None = 0,
    LooseFile,
    PackFile,
};

/**
 * @brief Registry entry for one logical asset.
 */
struct AssetRecord {
    AssetId id{};
    AssetKind kind{AssetKind::Unknown};
    AssetLocationKind location_kind{AssetLocationKind::None};

    String loose_path{};

    U32 pack_index{0};
    U64 pack_offset{0};
    U64 packed_size{0};
    U64 unpacked_size{0};

    U64 content_hash{0};
};

/**
 * @brief Maps AssetIds to cooked asset storage locations.
 *
 * @details
 * AssetRegistry does not read files and does not create runtime resources.
 */
class AssetRegistry {
public:
    explicit AssetRegistry(Alloc *alloc) noexcept
        : m_alloc(alloc),
          m_records(alloc),
          m_lookup(HashMap<U64, U32>::with_capacity(alloc, 4096)) {
        FR_ASSERT(alloc, "allocator must be non-null");
    }

    AssetRegistry(const AssetRegistry &) = delete;
    AssetRegistry(AssetRegistry &&) = delete;
    AssetRegistry &operator=(const AssetRegistry &) = delete;
    AssetRegistry &operator=(AssetRegistry &&) = delete;

    /**
     * @brief Registers or replaces a loose cooked asset record.
     */
    bool register_loose_asset(AssetId id, AssetKind kind, StringView path,
                              U64 content_hash = 0) noexcept {
        if (!id.is_valid() || path.is_empty() || kind == AssetKind::Unknown) {
            return false;
        }

        if (auto existing = m_lookup.find(id.value); existing.is_some()) {
            U32 index = *existing.unwrap();
            AssetRecord &record = m_records[index];

            record.id = id;
            record.kind = kind;
            record.location_kind = AssetLocationKind::LooseFile;
            record.loose_path = String::from_view(m_alloc, path);

            record.pack_index = 0;
            record.pack_offset = 0;
            record.packed_size = 0;
            record.unpacked_size = 0;

            record.content_hash = content_hash;
            return true;
        }

        AssetRecord record{};
        record.id = id;
        record.kind = kind;
        record.location_kind = AssetLocationKind::LooseFile;
        record.loose_path = String::from_view(m_alloc, path);
        record.content_hash = content_hash;

        const U32 index = static_cast<U32>(m_records.size());
        m_records.push_back(std::move(record));
        m_lookup.insert(id.value, index);

        return true;
    }

    /**
     * @brief Registers or replaces a packed cooked asset record.
     */
    bool register_pack_asset(AssetId id, AssetKind kind, U32 pack_index, U64 offset,
                             U64 packed_size, U64 unpacked_size, U64 content_hash = 0) noexcept {
        if (!id.is_valid() || kind == AssetKind::Unknown || packed_size == 0 ||
            unpacked_size == 0) {
            return false;
        }

        if (auto existing = m_lookup.find(id.value); existing.is_some()) {
            U32 index = *existing.unwrap();
            AssetRecord &record = m_records[index];

            record.id = id;
            record.kind = kind;
            record.location_kind = AssetLocationKind::PackFile;
            record.loose_path.clear();

            record.pack_index = pack_index;
            record.pack_offset = offset;
            record.packed_size = packed_size;
            record.unpacked_size = unpacked_size;

            record.content_hash = content_hash;
            return true;
        }

        AssetRecord record{};
        record.id = id;
        record.kind = kind;
        record.location_kind = AssetLocationKind::PackFile;
        record.pack_index = pack_index;
        record.pack_offset = offset;
        record.packed_size = packed_size;
        record.unpacked_size = unpacked_size;
        record.content_hash = content_hash;

        const U32 index = static_cast<U32>(m_records.size());
        m_records.push_back(std::move(record));
        m_lookup.insert(id.value, index);

        return true;
    }

    /**
     * @brief Finds a registry record for an asset id.
     */
    [[nodiscard]] const AssetRecord *find(AssetId id) const noexcept {
        if (!id.is_valid()) {
            return nullptr;
        }

        auto index_opt = m_lookup.find(id.value);
        if (!index_opt.is_some()) {
            return nullptr;
        }

        const U32 index = *index_opt.unwrap();
        if (index >= m_records.size()) {
            return nullptr;
        }

        return &m_records[index];
    }

    void clear() noexcept {
        m_records.clear();
        m_lookup.clear();
    }

    [[nodiscard]] USize size() const noexcept {
        return m_records.size();
    }

private:
    Alloc *m_alloc{nullptr};

    DynamicArray<AssetRecord> m_records;
    HashMap<U64, U32> m_lookup;
};

} // namespace fr
