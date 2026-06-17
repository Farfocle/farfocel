/**
 * @file asset_id.hpp
 * @author Tfoedy
 * @brief Stable logical asset identifiers.
 */

#pragma once

#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Stable logical asset identifier.
 *
 * @details
 * Runtime code should use AssetId instead of physical cooked asset paths. AssetRegistry maps
 * AssetId values to cooked asset storage.
 */
struct AssetId {
    U64 value{0};

    [[nodiscard]] static constexpr AssetId from_hash(U64 hash) noexcept {
        return AssetId{hash};
    }

    [[nodiscard]] static AssetId from_view(StringView view) noexcept {
        return AssetId{hash_bytes(view.data(), view.size())};
    }

    /**
     * @brief Creates an AssetId from a normalized logical asset path.
     *
     * @details
     * Logical paths should be relative to the asset root and use '/' separators.
     */
    [[nodiscard]] static AssetId from_logical_path(StringView path) noexcept {
        return from_view(path);
    }

    template <USize N>
    [[nodiscard]] static constexpr AssetId
    from_logical_path_literal(const char (&str)[N]) noexcept {
        return from_literal(str);
    }

    template <USize N>
    [[nodiscard]] static constexpr AssetId from_literal(const char (&str)[N]) noexcept {
        return AssetId{hash_bytes(str, N - 1)};
    }

    [[nodiscard]] static constexpr AssetId nil() noexcept {
        return AssetId{0};
    }

    [[nodiscard]] constexpr bool is_nil() const noexcept {
        return value == 0;
    }

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != 0;
    }

    [[nodiscard]] constexpr bool operator==(const AssetId &other) const noexcept {
        return value == other.value;
    }

    [[nodiscard]] constexpr bool operator!=(const AssetId &other) const noexcept {
        return value != other.value;
    }

private:
    static constexpr U64 FNV1A64_OFFSET_BASIS = 14695981039346656037ull;
    static constexpr U64 FNV1A64_PRIME = 1099511628211ull;

    [[nodiscard]] static constexpr U64 hash_bytes(const char *data, USize size) noexcept {
        U64 hash = FNV1A64_OFFSET_BASIS;

        for (USize i = 0; i < size; ++i) {
            hash ^= static_cast<U64>(static_cast<unsigned char>(data[i]));
            hash *= FNV1A64_PRIME;
        }

        return hash;
    }
};

/// @brief Creates an AssetId from a string literal.
template <USize N>
[[nodiscard]] constexpr AssetId asset_id_literal(const char (&str)[N]) noexcept {
    return AssetId::from_literal(str);
}

} // namespace fr

#define FR_ASSET_ID(str) ::fr::asset_id_literal(str)
#define FR_ASSET_PATH_ID(str) ::fr::AssetId::from_logical_path_literal(str)
