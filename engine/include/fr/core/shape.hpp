/**
 * @file shape.hpp
 * @author Kiju
 *
 * @brief Shape is a simple reflection/serializatian API inspired by Media Molecule's serialization
 * system.
 *
 * @details The shape system works by passing mutable archives into shape methods/functions
 * implemented by/for arbitrary types. Archives  traverse through an implicit
 * tree of reflected values. Every archive must satify the `IsArchive` concept. This approach is a
 * templated version of the Media Molecule serialization system. Similar systems are also used in
 * Unreal Engine.
 *
 * https://handmade.network/p/29/swedish-cubes-for-unity/blog/p/2723-how_media_molecule_does_serialization
 */

#pragma once

#include <concepts>

#include "fr/core/macros.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
enum class ArchiveKind : U8 { Json };
enum class ArchiveAction : U8 { Write, Read };

template <typename T>
concept IsArchive =
    requires(T &archive, U32 &value) {
        { T::Options } -> std::same_as<typename T::Options>;
        { T::action } -> std::same_as<ArchiveAction>;
        { archive.prop("@name", value) } -> std::same_as<void>;
        {
            archive.list("@items", [](T &) {})
        } -> std::same_as<void>;
        {
            archive.dict("@items", [](T &) {})
        } -> std::same_as<void>;
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
    return name.view(start, end - 1);
#elif defined(_MSC_VER)
    StringView name = __FUNCSIG__;
    USize start_at = name.find("get_typename<");
    USize end = find_last_char(name, '>');
    if (start_at == StringView::npos || end == StringView::npos) {
        return "@unknown";
    }
    USize start = start_at + StringView("get_typename<").size();
    return name.view(start, end - 1);
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
