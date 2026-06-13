/**
 * @file asset_manifest_format.hpp
 * @author Tfoedy
 * @brief Runtime asset manifest format.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_kind.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

#pragma pack(push, 1)

/**
 * @brief Header stored at the beginning of .fmanifest.
 *
 * @details
 * String offsets are relative to the manifest string block. Strings are stored as sized byte
 * ranges, not null-terminated C strings.
 */
struct CookedAssetManifestHeader {
    char verify[4]{'F', 'M', 'A', 'N'};
    U32 version{1};

    U32 pack_count{0};
    U32 pack_table_offset{0};
    U32 pack_table_size{0};

    U32 loose_count{0};
    U32 loose_table_offset{0};
    U32 loose_table_size{0};

    U32 string_data_offset{0};
    U32 string_data_size{0};
};

/**
 * @brief One pack path entry in .fmanifest.
 */
struct CookedAssetManifestPackRecord {
    U32 path_offset{0};
    U32 path_size{0};
};

/**
 * @brief One loose cooked asset entry in .fmanifest.
 */
struct CookedAssetManifestLooseRecord {
    AssetId id{};
    AssetKind kind{AssetKind::Unknown};

    U64 content_hash{0};

    U32 path_offset{0};
    U32 path_size{0};
};

#pragma pack(pop)

static_assert(sizeof(AssetId) == sizeof(U64), "AssetId must remain a 64-bit disk value");

} // namespace fr
