/**
 * @file asset_pack.hpp
 * @author Tfoedy
 * @brief Mounted cooked asset pack.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

#include "fr/asset/asset_pack_format.hpp"
#include "fr/logger/logger.hpp"

namespace fr {

/**
 * @brief Loaded .fpack container.
 *
 * @details
 * The current implementation keeps pack bytes in memory.
 */
class AssetPack {
public:
    explicit AssetPack(Alloc *alloc) noexcept
        : m_alloc(alloc),
          m_bytes(alloc),
          m_entries(alloc),
          m_lookup(HashMap<U64, U32>::with_capacity(alloc, 4096)) {
        FR_ASSERT(alloc, "allocator must be non-null");
    }

    AssetPack(const AssetPack &) = delete;
    AssetPack &operator=(const AssetPack &) = delete;

    AssetPack(AssetPack &&other) noexcept
        : m_alloc(other.m_alloc),
          m_bytes(std::move(other.m_bytes)),
          m_entries(std::move(other.m_entries)),
          m_lookup(std::move(other.m_lookup)) {
        other.m_alloc = nullptr;
    }

    AssetPack &operator=(AssetPack &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        m_alloc = other.m_alloc;
        m_bytes = std::move(other.m_bytes);
        m_entries = std::move(other.m_entries);
        m_lookup = std::move(other.m_lookup);

        other.m_alloc = nullptr;
        return *this;
    }

    /// @brief Loads and validates a .fpack file.
    [[nodiscard]] bool mount_from_path(StringView path) noexcept {
        clear();

        if (path.is_empty()) {
            return false;
        }

        String path_str = String::from_view(m_alloc, path);

        auto file_bytes = file::read_all_bytes(m_alloc, path_str);
        if (!file_bytes.is_some()) {
            FR_LOG_ERR("Failed to read asset pack: {}", path);
            return false;
        }

        m_bytes = std::move(file_bytes.unwrap());

        if (!decode_index()) {
            FR_LOG_ERR("Failed to decode asset pack index: {}", path);
            clear();
            return false;
        }

        return true;
    }

    /// @brief Reads a packed asset entry into memory.
    [[nodiscard]] bool read_entry_bytes(const CookedAssetPackEntry &entry,
                                        DynamicArray<Byte> &out) const noexcept {
        out.clear();

        if (entry.compression != CookedAssetPackCompression::None) {
            FR_LOG_ERR("Compressed asset pack entries are not supported yet. Asset id: {}",
                       entry.id.value);
            return false;
        }

        if (entry.packed_size == 0 || entry.unpacked_size == 0 ||
            entry.packed_size != entry.unpacked_size) {
            return false;
        }

        if (entry.offset > m_bytes.size() || entry.packed_size > m_bytes.size() - entry.offset) {
            return false;
        }

        out.grow_default(static_cast<USize>(entry.unpacked_size));

        fr::mem::copy_raw_range(m_bytes.data() + entry.offset,
                                static_cast<USize>(entry.packed_size), out.data());

        return true;
    }

    [[nodiscard]] Slice<const CookedAssetPackEntry> entries() const noexcept {
        return m_entries.slice();
    }

    [[nodiscard]] bool is_mounted() const noexcept {
        return !m_bytes.is_empty() && !m_entries.is_empty();
    }

    void clear() noexcept {
        m_bytes.clear();
        m_entries.clear();
        m_lookup.clear();
    }

private:
    template <typename T>
    [[nodiscard]] static bool read_object(Slice<const Byte> bytes, USize offset, T &out) noexcept {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) {
            return false;
        }

        fr::mem::copy_raw_range(bytes.data() + offset, sizeof(T), reinterpret_cast<Byte *>(&out));
        return true;
    }

    [[nodiscard]] static bool verify_header(const CookedAssetPackHeader &header) noexcept {
        return header.verify[0] == 'F' && header.verify[1] == 'P' && header.verify[2] == 'C' &&
               header.verify[3] == 'K' && header.version == 1;
    }

    [[nodiscard]] static bool validate_range(USize file_size, U64 offset, U64 size) noexcept {
        if (offset > file_size) {
            return false;
        }

        return size <= file_size - offset;
    }

    [[nodiscard]] bool decode_index() noexcept {
        Slice<const Byte> bytes = m_bytes.slice();

        CookedAssetPackHeader header{};
        if (!read_object(bytes, 0, header)) {
            return false;
        }

        if (!verify_header(header)) {
            return false;
        }

        if (header.entry_count == 0) {
            return false;
        }

        const U64 expected_entry_table_size =
            static_cast<U64>(header.entry_count) * sizeof(CookedAssetPackEntry);

        if (header.entry_table_size != expected_entry_table_size) {
            return false;
        }

        if (!validate_range(bytes.size(), header.entry_table_offset, header.entry_table_size) ||
            !validate_range(bytes.size(), header.data_offset, header.data_size)) {
            return false;
        }

        const U64 entry_table_end = header.entry_table_offset + header.entry_table_size;
        if (entry_table_end < header.entry_table_offset) {
            return false;
        }

        const U64 data_end = header.data_offset + header.data_size;
        if (data_end < header.data_offset || data_end > bytes.size()) {
            return false;
        }

        if (header.data_offset < entry_table_end) {
            return false;
        }

        m_entries.reserve(header.entry_count);

        for (U32 i = 0; i < header.entry_count; ++i) {
            const USize entry_offset = static_cast<USize>(header.entry_table_offset) +
                                       static_cast<USize>(i) * sizeof(CookedAssetPackEntry);

            CookedAssetPackEntry entry{};
            if (!read_object(bytes, entry_offset, entry)) {
                return false;
            }

            if (!entry.id.is_valid() || entry.kind == AssetKind::Unknown ||
                entry.compression != CookedAssetPackCompression::None) {
                return false;
            }

            if (entry.offset < header.data_offset) {
                return false;
            }

            if (entry.offset > data_end) {
                return false;
            }

            if (entry.packed_size > data_end - entry.offset) {
                return false;
            }

            if (entry.unpacked_size == 0 || entry.packed_size != entry.unpacked_size) {
                return false;
            }

            if (m_lookup.find(entry.id.value).is_some()) {
                FR_LOG_ERR("Duplicate asset id in pack: {}", entry.id.value);
                return false;
            }

            const U32 index = static_cast<U32>(m_entries.size());
            m_entries.push_back(entry);
            m_lookup.insert(entry.id.value, index);
        }

        return true;
    }

private:
    Alloc *m_alloc{nullptr};

    DynamicArray<Byte> m_bytes;
    DynamicArray<CookedAssetPackEntry> m_entries;
    HashMap<U64, U32> m_lookup;
};

} // namespace fr
