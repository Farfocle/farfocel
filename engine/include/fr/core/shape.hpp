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
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
enum class ArchiveKind : U8 { Serializer, Deserializer };

template <typename T>
concept IsArchive =
    requires(T &archive, U32 &value) {
        { T::kind } -> std::same_as<ArchiveKind>;
        { archive.prop("@name", value) } -> std::same_as<void>;
        { archive.list("@items", [](T &) {}) } -> std::same_as<void>;
        { archive.dict("@items", [](T &) {}) } -> std::same_as<void>;
        { archive.current_list_size() } -> std::same_as<USize>;
    } && !std::is_copy_constructible_v<T> && !std::is_copy_assignable_v<T> &&
    !std::is_move_constructible_v<T> && !std::is_move_assignable_v<T>;

namespace impl {
constexpr USize find_last_char(StringView view, char needle) {
    for (USize i = view.size(); i > 0; --i) {
        if (view[i - 1] == needle) {
            return i - 1;
        }
    }
    return StringView::npos;
}

template <typename A, typename V>
concept HasMemberShape = requires(A &archive, V &value) { value.shape(archive); };

template <typename A, typename V>
concept HasADLShape = requires(A &archive, V &value) { shape(archive, value); };

template <typename T>
consteval StringView get_typename() {
#if defined(__clang__) || defined(__GNUC__)
    StringView name = __PRETTY_FUNCTION__;
    USize start_at = name.find("T = ");
    USize end = find_last_char(name, ']');
    if (start_at == StringView::npos || end == StringView::npos) {
        return "@unknown";
    }
    USize start = start_at + 4;
    return name.substr(start, end - start);
#elif defined(_MSC_VER)
    StringView name = __FUNCSIG__;
    USize start_at = name.find("get_typename<");
    USize end = find_last_char(name, '>');
    if (start_at == StringView::npos || end == StringView::npos) {
        return "@unknown";
    }
    USize start = start_at + StringView("get_typename<").size();
    return name.substr(start, end - start);
#else
    return "@unknown";
#endif
}

} // namespace impl

template <typename A, typename V>
concept IsShape = impl::HasMemberShape<A, V> || impl::HasADLShape<A, V>;

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
