/**
 * @file shape.hpp
 * @author Kiju
 *
 * @brief Shape is a simple reflection/serializatian API inspired by Media Molecule's serialization
 * system.
 */

#pragma once

#include <concepts>

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
enum class ArchiveAction : U8 { Read, Write };

template <typename T>
concept IsArchive =
    requires(T &archive, U32 &value) {
        { T::action } -> std::same_as<ArchiveAction>;
        { archive.property("@name", value) } -> std::same_as<void>;
    } && !std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T> &&
    !std::is_move_constructible_v<T> && !std::is_move_assignable_v<T>;

namespace impl {
template <typename A, typename V>
concept HasMemberShape = requires(A &archive, V &value) { value.shape(archive); };

template <typename A, typename V>
concept HasADLShape = requires(A &archive, V &value) { shape(archive, value); };
} // namespace impl

template <typename A, typename V>
concept HasShape = impl::HasMemberShape<A, V> || impl::HasADLShape<A, V>;

template <typename A, typename V>
void call_shape(A &archive, V &value) noexcept {
    if constexpr (impl::HasMemberShape<A, V>) {
        value.shape(archive);
    } else if constexpr (impl::HasADLShape<A, V>) {
        shape(archive, value);
    } else {
        FR_STATIC_ASSERT(false, "Provided object does not implement the member shape method nor "
                                "the global adl shape function");
    }
}

} // namespace fr
