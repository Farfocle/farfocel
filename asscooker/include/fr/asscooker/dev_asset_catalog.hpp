/**
 * @file dev_asset_catalog.hpp
 * @author Tfoedy
 * @brief Development-side cooked asset catalog helpers.
 */

#pragma once

#include "fr/asscooker/asscooker.hpp"
#include "fr/asset/asset_registry.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::asscooker {

/**
 * @brief One development catalog record for a cooked runtime asset.
 *
 * @details
 * path is the cooked loose asset path used by AssetRegistry and .fmanifest.
 * source_path is optional development metadata used to track where the asset came from.
 */
struct DevAssetRecord {
    AssetId id{};
    AssetKind kind{AssetKind::Unknown};

    String path{};
    String source_path{};

    U64 content_hash{0};
};

/**
 * @brief Lightweight development asset catalog.
 *
 * @details
 * This is not a final runtime system. It is used by development tools to accumulate cooked asset
 * outputs, register them as loose assets and rebuild a development .fmanifest.
 */
class DevAssetCatalog {
public:
    DevAssetCatalog() noexcept
        : DevAssetCatalog(get_ambient_ctx().alloc) {
    }

    explicit DevAssetCatalog(Alloc *alloc) noexcept;

    DevAssetCatalog(const DevAssetCatalog &) = delete;
    DevAssetCatalog(DevAssetCatalog &&) = delete;
    DevAssetCatalog &operator=(const DevAssetCatalog &) = delete;
    DevAssetCatalog &operator=(DevAssetCatalog &&) = delete;

    /// @brief Removes all catalog records.
    void clear() noexcept;

    /// @brief Adds or replaces a cooked output record.
    void add_or_replace(const CookedAssetOutput &output, StringView source_path = {}) noexcept;

    /// @brief Adds or replaces all cooked output records.
    void add_or_replace(Slice<const CookedAssetOutput> outputs,
                        StringView source_path = {}) noexcept;

    /// @brief Registers catalog records as loose assets.
    [[nodiscard]] bool register_loose_assets(AssetRegistry &registry) const noexcept;

    /// @brief Builds a loose-asset development manifest from catalog records.
    [[nodiscard]] bool build_loose_manifest(StringView output_path) const noexcept;

    /// @brief Returns all catalog records.
    [[nodiscard]] Slice<const DevAssetRecord> records() const noexcept {
        return m_records.slice();
    }

    /// @brief Returns number of catalog records.
    [[nodiscard]] USize size() const noexcept {
        return m_records.size();
    }

    /// @brief Returns true when the catalog has no records.
    [[nodiscard]] bool is_empty() const noexcept {
        return m_records.is_empty();
    }

private:
    [[nodiscard]] DevAssetRecord *find_record(AssetId id) noexcept;

private:
    Alloc *m_alloc{nullptr};
    DynamicArray<DevAssetRecord> m_records;
};

} // namespace fr::asscooker
