/**
 * @file nil.hpp
 * @author Kiju
 * @brief The nil protocol.
 *
 * @details Why?
 * Nil differs from null (or nullptr in C++ specifically) in that it is a value of some object, not
 * the absance of it, and thus can be used in value contexts. Nil represents an empty and
 * functionally useless value that is safe and valid to pass around. The inspiration for this
 * concept came from newer programming languages (like Odin) and data-oriented programming. It is
 * more of a formalized design pattern than anything else. Use cases in the farfocel engine include:
 * - `fr::Optional` niche value optimization
 * - Game object systems in which handles can formally have a nil value and use it to represent
 *   stub handles
 */

#pragma once

#include <concepts>
#include <type_traits>

namespace fr {

struct NilTag {
    explicit constexpr NilTag() = default;
};

namespace impl {

template <typename T>
void is_nil(const T &) = delete;
template <typename T>
void nil(T *) = delete;

template <typename T>
concept HasMemberNil = requires {
    { T::nil() } noexcept -> std::convertible_to<T>;
};

template <typename T>
concept HasMemberIsNil = requires(const T &v) {
    { v.is_nil() } noexcept -> std::convertible_to<bool>;
};

template <typename T>
concept HasADLIsNil = requires(const T &v) {
    { is_nil(v) } -> std::convertible_to<bool>;
};

template <typename T>
concept HasADLNil = requires {
    { nil(static_cast<T *>(nullptr)) } -> std::convertible_to<T>;
};

} // namespace impl

/**
 * @brief Concept for types that have a defined nil value.
 */
template <typename T>
concept IsNillable =
    (sizeof(T)) != 0 && (std::is_pointer_v<T> || (impl::HasADLNil<T> && impl::HasADLIsNil<T>) ||
                         (impl::HasMemberNil<T> && impl::HasMemberIsNil<T>));

/**
 * @brief Returns the nil value for a nillable type.
 *
 * @tparam T Nillable type.
 * @return Nil value.
 */
template <IsNillable T>
[[nodiscard]] constexpr T call_nil() noexcept {
    if constexpr (impl::HasMemberNil<T>) {
        return T::nil();
    } else if constexpr (impl::HasADLNil<T>) {
        return nil(static_cast<T *>(nullptr));
    } else if constexpr (std::is_pointer_v<T>) {
        return nullptr;
    }
}

/**
 * @brief Checks if a value is nil.
 *
 * @tparam T Nillable type.
 * @param value Value to check.
 * @return `true` if nil.
 */
template <IsNillable T>
[[nodiscard]] constexpr bool call_is_nil(const T &value) noexcept {
    if constexpr (impl::HasMemberIsNil<T>) {
        return value.is_nil();
    } else if constexpr (impl::HasADLIsNil<T>) {
        return is_nil(value);
    } else if constexpr (std::is_pointer_v<T>) {
        return value == nullptr;
    }
}
} // namespace fr
