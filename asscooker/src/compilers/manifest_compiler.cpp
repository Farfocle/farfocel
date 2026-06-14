/**
 * @file manifest_compiler.cpp
 * @author Tfoedy
 * @brief Runtime asset manifest compiler.
 */

#include "manifest_compiler.hpp"

#include "fr/asset/asset_manifest_format.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {
namespace {

[[nodiscard]] bool validate_manifest_string(StringView value, const char *label,
                                            USize index) noexcept {
    if (value.is_empty()) {
        FR_LOG_ERR("[Cooker] Manifest {} input {} has empty path.", label, index);
        return false;
    }

    if (value.size() > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Manifest {} input {} path is too large.", label, index);
        return false;
    }

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

bool has_duplicate_loose_asset_id(Slice<const ManifestLooseInput> assets, AssetId id,
                                  USize current_index) noexcept {
    for (USize i = 0; i < current_index; ++i) {
        if (assets[i].id == id) {
            return true;
        }
    }

    return false;
}

bool validate_pack_input(const ManifestPackInput &input, USize index) noexcept {
    if (input.path.is_empty()) {
        FR_LOG_ERR("[Cooker] Manifest pack input {} has empty path.", index);
        return false;
    }

    return true;
}

bool validate_loose_input(const ManifestLooseInput &input, USize index) noexcept {
    if (!input.id.is_valid()) {
        FR_LOG_ERR("[Cooker] Manifest loose input {} has invalid AssetId.", index);
        return false;
    }

    if (input.kind == AssetKind::Unknown) {
        FR_LOG_ERR("[Cooker] Manifest loose input {} has unknown AssetKind.", index);
        return false;
    }

    if (input.path.is_empty()) {
        FR_LOG_ERR("[Cooker] Manifest loose input {} has empty path.", index);
        return false;
    }

    return true;
}

U32 append_manifest_string(DynamicArray<Byte> &string_data, StringView value) noexcept {
    FR_ASSERT(!value.is_empty(), "manifest string must be non-empty");

    const U32 offset = static_cast<U32>(string_data.size());

    append_bytes(string_data, value.data(), value.size());

    return offset;
}

} // namespace

bool compile_manifest(const ManifestBuildDesc &desc, StringView output_path) noexcept {
    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot build manifest with empty output path.");
        return false;
    }

    if (desc.packs.is_empty() && desc.loose_assets.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot build an empty asset manifest.");
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    DynamicArray<CookedAssetManifestPackRecord> pack_records(alloc);
    DynamicArray<CookedAssetManifestLooseRecord> loose_records(alloc);
    DynamicArray<Byte> string_data(alloc);

    pack_records.reserve(desc.packs.size());
    loose_records.reserve(desc.loose_assets.size());

    for (USize i = 0; i < desc.packs.size(); ++i) {
        const ManifestPackInput &input = desc.packs[i];

        if (!validate_manifest_string(input.path, "pack", i)) {
            return false;
        }

        if (string_data.size() > static_cast<USize>(0xFFFFFFFFu) - input.path.size()) {
            FR_LOG_ERR("[Cooker] Manifest string block is too large.");
            return false;
        }

        CookedAssetManifestPackRecord record{};
        record.path_offset = append_manifest_string(string_data, input.path);
        record.path_size = static_cast<U32>(input.path.size());

        pack_records.push_back(record);
    }

    for (USize i = 0; i < desc.loose_assets.size(); ++i) {
        const ManifestLooseInput &input = desc.loose_assets[i];

        if (!validate_manifest_string(input.path, "loose", i)) {
            return false;
        }

        if (string_data.size() > static_cast<USize>(0xFFFFFFFFu) - input.path.size()) {
            FR_LOG_ERR("[Cooker] Manifest string block is too large.");
            return false;
        }

        if (has_duplicate_loose_asset_id(desc.loose_assets, input.id, i)) {
            FR_LOG_ERR("[Cooker] Duplicate loose AssetId in manifest input: {}", input.id.value);
            return false;
        }

        CookedAssetManifestLooseRecord record{};
        record.id = input.id;
        record.kind = input.kind;
        record.content_hash = input.content_hash;
        record.path_offset = append_manifest_string(string_data, input.path);
        record.path_size = static_cast<U32>(input.path.size());

        loose_records.push_back(record);
    }

    if (pack_records.size() > static_cast<USize>(0xFFFFFFFFu) ||
        loose_records.size() > static_cast<USize>(0xFFFFFFFFu) ||
        string_data.size() > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Manifest is too large.");
        return false;
    }

    const USize pack_table_size = pack_records.size() * sizeof(CookedAssetManifestPackRecord);

    const USize loose_table_size = loose_records.size() * sizeof(CookedAssetManifestLooseRecord);

    if (pack_table_size > static_cast<USize>(0xFFFFFFFFu) ||
        loose_table_size > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Manifest tables are too large.");
        return false;
    }

    const USize loose_table_offset = sizeof(CookedAssetManifestHeader) + pack_table_size;
    const USize string_data_offset = loose_table_offset + loose_table_size;

    if (loose_table_offset > static_cast<USize>(0xFFFFFFFFu) ||
        string_data_offset > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Manifest offsets are too large.");
        return false;
    }

    CookedAssetManifestHeader header{};
    header.verify[0] = 'F';
    header.verify[1] = 'M';
    header.verify[2] = 'A';
    header.verify[3] = 'N';
    header.version = 1;

    header.pack_count = static_cast<U32>(pack_records.size());
    header.pack_table_offset = sizeof(CookedAssetManifestHeader);

    header.pack_table_size = static_cast<U32>(pack_table_size);

    header.loose_count = static_cast<U32>(loose_records.size());
    header.loose_table_offset = static_cast<U32>(loose_table_offset);
    header.loose_table_size = static_cast<U32>(loose_table_size);

    header.string_data_offset = static_cast<U32>(string_data_offset);
    header.string_data_size = static_cast<U32>(string_data.size());

    DynamicArray<Byte> output(alloc);

    append_bytes(output, &header, sizeof(CookedAssetManifestHeader));

    append_bytes(output, pack_records.data(),
                 pack_records.size() * sizeof(CookedAssetManifestPackRecord));

    append_bytes(output, loose_records.data(),
                 loose_records.size() * sizeof(CookedAssetManifestLooseRecord));

    append_bytes(output, string_data.data(), string_data.size());

    String out_path = String::from_view(alloc, output_path);

    if (!file::write_all_bytes(out_path, Slice<const Byte>(output.data(), output.size()))) {
        FR_LOG_ERR("[Cooker] Failed to write asset manifest: {}", output_path);
        return false;
    }

    return true;
}

} // namespace fr::asscooker
