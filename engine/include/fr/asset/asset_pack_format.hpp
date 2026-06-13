/**
 * @file asset_pack_format.hpp
 * @author Tfoedy
 * @brief Cooked asset pack format.
 */

#pragma once

#include "fr/asset/asset_id.hpp"
#include "fr/asset/asset_kind.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Compression mode stored in .fpack entries.
 */
enum class CookedAssetPackCompression : U32 {
    None = 0,
};

#pragma pack(push, 1)

/**
 * @brief Header stored at the beginning of .fpack.
 *
 * @details
 * Entry offsets are absolute file offsets.
 */
struct CookedAssetPackHeader {
    char verify[4]{'F', 'P', 'C', 'K'};
    U32 version{1};

    U32 entry_count{0};
    U64 entry_table_offset{0};
    U64 entry_table_size{0};

    U64 data_offset{0};
    U64 data_size{0};
};

/**
 * @brief One cooked asset entry stored in .fpack.
 */
struct CookedAssetPackEntry {
    AssetId id{};
    AssetKind kind{AssetKind::Unknown};

    U64 offset{0};
    U64 packed_size{0};
    U64 unpacked_size{0};

    U64 content_hash{0};
    CookedAssetPackCompression compression{CookedAssetPackCompression::None};
};

#pragma pack(pop)

static_assert(sizeof(AssetId) == sizeof(U64), "AssetId must remain a 64-bit disk value");

} // namespace fr
