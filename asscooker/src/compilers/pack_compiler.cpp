/**
 * @file pack_compiler.cpp
 * @author Tfoedy
 * @brief Cooked asset pack compiler.
 */

#include "pack_compiler.hpp"

#include "fr/asset/asset_pack_format.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/file.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"

namespace fr::asscooker {
namespace {

void append_bytes(DynamicArray<Byte> &out, const void *data, USize size) noexcept {
    if (size == 0) {
        return;
    }

    FR_ASSERT(data, "source data must be non-null");

    const USize old_size = out.size();
    out.grow_default(old_size + size);

    fr::mem::copy_raw_range(reinterpret_cast<const Byte *>(data), size, out.data() + old_size);
}

bool has_duplicate_asset_id(Slice<const PackAssetInput> assets, AssetId id,
                            USize current_index) noexcept {
    for (USize i = 0; i < current_index; ++i) {
        if (assets[i].id == id) {
            return true;
        }
    }

    return false;
}

bool validate_input_asset(const PackAssetInput &asset, USize index) noexcept {
    if (!asset.id.is_valid()) {
        FR_LOG_ERR("[Cooker] Pack input asset {} has invalid AssetId.", index);
        return false;
    }

    if (asset.kind == AssetKind::Unknown) {
        FR_LOG_ERR("[Cooker] Pack input asset {} has unknown AssetKind.", index);
        return false;
    }

    if (asset.path.is_empty()) {
        FR_LOG_ERR("[Cooker] Pack input asset {} has empty path.", index);
        return false;
    }

    return true;
}

} // namespace

bool compile_pack(Slice<const PackAssetInput> assets, StringView output_path) noexcept {
    if (assets.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot build an empty asset pack.");
        return false;
    }

    if (output_path.is_empty()) {
        FR_LOG_ERR("[Cooker] Cannot build asset pack with empty output path.");
        return false;
    }

    Alloc *alloc = get_ambient_ctx().alloc;
    FR_ASSERT(alloc, "ambient allocator must be non-null");

    DynamicArray<CookedAssetPackEntry> entries(alloc);
    DynamicArray<Byte> data_blob(alloc);

    entries.reserve(assets.size());

    if (assets.size() > static_cast<USize>(0xFFFFFFFFu)) {
        FR_LOG_ERR("[Cooker] Asset pack has too many entries.");
        return false;
    }

    const U64 entry_table_size =
        static_cast<U64>(assets.size()) * static_cast<U64>(sizeof(CookedAssetPackEntry));

    const U64 data_offset = static_cast<U64>(sizeof(CookedAssetPackHeader)) + entry_table_size;

    if (data_offset < entry_table_size) {
        FR_LOG_ERR("[Cooker] Asset pack header/table size overflow.");
        return false;
    }

    for (USize i = 0; i < assets.size(); ++i) {
        const PackAssetInput &input = assets[i];

        if (!validate_input_asset(input, i)) {
            return false;
        }

        if (has_duplicate_asset_id(assets, input.id, i)) {
            FR_LOG_ERR("[Cooker] Duplicate AssetId in pack input: {}", input.id.value);
            return false;
        }

        String path = String::from_view(alloc, input.path);

        auto file_bytes = file::read_all_bytes(alloc, path);
        if (!file_bytes.is_some()) {
            FR_LOG_ERR("[Cooker] Failed to read cooked asset for pack: {}", input.path);
            return false;
        }

        const DynamicArray<Byte> &payload = file_bytes.unwrap();
        if (payload.is_empty()) {
            FR_LOG_ERR("[Cooker] Cooked asset payload is empty: {}", input.path);
            return false;
        }

        CookedAssetPackEntry entry{};
        entry.id = input.id;
        entry.kind = input.kind;

        if (payload.size() >
            static_cast<USize>(static_cast<U64>(-1) - static_cast<U64>(data_blob.size()))) {
            FR_LOG_ERR("[Cooker] Asset pack data section is too large.");
            return false;
        }

        const U64 payload_offset = data_offset + static_cast<U64>(data_blob.size());
        if (payload_offset < data_offset) {
            FR_LOG_ERR("[Cooker] Asset pack entry offset overflow.");
            return false;
        }

        entry.offset = payload_offset;
        entry.packed_size = static_cast<U64>(payload.size());
        entry.unpacked_size = static_cast<U64>(payload.size());
        entry.content_hash = input.content_hash;
        entry.compression = CookedAssetPackCompression::None;

        append_bytes(data_blob, payload.data(), payload.size());
        entries.push_back(entry);
    }

    CookedAssetPackHeader header{};
    header.verify[0] = 'F';
    header.verify[1] = 'P';
    header.verify[2] = 'C';
    header.verify[3] = 'K';
    header.version = 1;

    header.entry_count = static_cast<U32>(entries.size());
    header.entry_table_offset = sizeof(CookedAssetPackHeader);
    header.entry_table_size = entries.size() * sizeof(CookedAssetPackEntry);

    header.data_offset = data_offset;
    header.data_size = static_cast<U64>(data_blob.size());

    DynamicArray<Byte> output(alloc);

    append_bytes(output, &header, sizeof(CookedAssetPackHeader));
    append_bytes(output, entries.data(), entries.size() * sizeof(CookedAssetPackEntry));
    append_bytes(output, data_blob.data(), data_blob.size());

    String out_path = String::from_view(alloc, output_path);

    if (!file::write_all_bytes(out_path, Slice<const Byte>(output.data(), output.size()))) {
        FR_LOG_ERR("[Cooker] Failed to write asset pack: {}", output_path);
        return false;
    }

    return true;
}

} // namespace fr::asscooker
