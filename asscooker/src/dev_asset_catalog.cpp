/**
 * @file dev_asset_catalog.cpp
 * @author Tfoedy
 * @brief Development-side cooked asset catalog helpers.
 */

#include "fr/asscooker/dev_asset_catalog.hpp"

#include <utility>

#include "fr/core/macros.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {

DevAssetCatalog::DevAssetCatalog(Alloc *alloc) noexcept
    : m_alloc(alloc),
      m_records(alloc) {
    FR_ASSERT(alloc, "allocator must be non-null");

    m_records.reserve(256);
}

void DevAssetCatalog::clear() noexcept {
    m_records.clear();
}

DevAssetRecord *DevAssetCatalog::find_record(AssetId id) noexcept {
    if (!id.is_valid()) {
        return nullptr;
    }

    for (USize i = 0; i < m_records.size(); ++i) {
        if (m_records[i].id == id) {
            return &m_records[i];
        }
    }

    return nullptr;
}

void DevAssetCatalog::add_or_replace(const CookedAssetOutput &output,
                                     StringView source_path) noexcept {
    if (!output.id.is_valid() || output.kind == AssetKind::Unknown || output.path.size() == 0) {
        FR_LOG_ERR("[Cooker] Cannot add invalid cooked asset output to dev catalog.");
        return;
    }

    DevAssetRecord *existing = find_record(output.id);

    if (existing) {
        existing->kind = output.kind;
        existing->path = String::from_view(m_alloc, output.path.view());
        existing->source_path = String::from_view(m_alloc, source_path);
        existing->content_hash = output.content_hash;
        return;
    }

    DevAssetRecord record{};
    record.id = output.id;
    record.kind = output.kind;
    record.path = String::from_view(m_alloc, output.path.view());
    record.source_path = String::from_view(m_alloc, source_path);
    record.content_hash = output.content_hash;

    m_records.push_back(std::move(record));
}

void DevAssetCatalog::add_or_replace(Slice<const CookedAssetOutput> outputs,
                                     StringView source_path) noexcept {
    for (const CookedAssetOutput &output : outputs) {
        add_or_replace(output, source_path);
    }
}

bool DevAssetCatalog::register_loose_assets(AssetRegistry &registry) const noexcept {
    bool ok = true;

    for (const DevAssetRecord &record : m_records) {
        if (!record.id.is_valid() || record.kind == AssetKind::Unknown || record.path.size() == 0) {
            FR_LOG_ERR("[Cooker] Dev catalog contains invalid asset record.");
            ok = false;
            continue;
        }

        ok = registry.register_loose_asset(record.id, record.kind, record.path.view()) && ok;
    }

    return ok;
}

bool DevAssetCatalog::build_loose_manifest(StringView output_path) const noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot build dev manifest with empty output path.");
        return false;
    }

    if (m_records.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot build dev manifest from an empty catalog.");
        return false;
    }

    DynamicArray<ManifestLooseInput> loose_assets(m_alloc);
    loose_assets.reserve(m_records.size());

    for (const DevAssetRecord &record : m_records) {
        if (!record.id.is_valid() || record.kind == AssetKind::Unknown || record.path.size() == 0) {
            FR_LOG_ERR("[Cooker] Dev catalog contains invalid asset record.");
            return false;
        }

        ManifestLooseInput input{};
        input.id = record.id;
        input.kind = record.kind;
        input.path = record.path.view();
        input.content_hash = record.content_hash;

        loose_assets.push_back(input);
    }

    ManifestBuildDesc desc{};
    desc.loose_assets = loose_assets.slice();

    return build_manifest(desc, output_path);
}

} // namespace fr::asscooker
