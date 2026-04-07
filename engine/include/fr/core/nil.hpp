/**
 * @file nil.hpp
 * @author Kiju
 * @brief Traits and concepts for types that have a "nil" or "invalid" state.
 */

#pragma once

#include <concepts>
#include <type_traits>

namespace fr {

/// @brief Sentinel type for anything that is empty or devoid of data, but not in a unkown, invalid
/// state.
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

/// @brief Concept for types that have a defined nil value.
template <typename T>
concept IsNillable =
    (sizeof(T)) != 0 && (std::is_pointer_v<T> || (impl::HasADLNil<T> && impl::HasADLIsNil<T>) ||
                         (impl::HasMemberNil<T> && impl::HasMemberIsNil<T>));

/// @brief Returns the nil value for a nillable type.
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

/// @brief Checks if a value is nil.
template <IsNillable T>
[[nodiscard]] constexpr bool call_is_nil(const T &v) noexcept {
    if constexpr (impl::HasMemberIsNil<T>) {
        return v.is_nil();
    } else if constexpr (impl::HasADLIsNil<T>) {
        return is_nil(v);
    } else if constexpr (std::is_pointer_v<T>) {
        return v == nullptr;
    }
}
} // namespace fr
