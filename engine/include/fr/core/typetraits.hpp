/**
 * @file typetraits.hpp
 * @author Kiju
 * @brief A collection of useful type traits used across the core.
 */

#pragma once

#include <concepts>
#include <type_traits>

#include "fr/core/typedefs.hpp"

namespace fr {
template <typename T>
concept IsU =
    std::same_as<T, U8> || std::same_as<T, U16> || std::same_as<T, U32> || std::same_as<T, U64>;

template <typename T>
concept IsS =
    std::same_as<T, S8> || std::same_as<T, S16> || std::same_as<T, S32> || std::same_as<T, S64>;

template <typename T>
concept IsByte = std::same_as<T, Byte>;

template <typename T>
concept IsChar = std::same_as<T, char>;

template <typename T>
concept IsBool = std::same_as<T, bool>;

template <typename T>
concept IsF = std::same_as<T, F32> || std::same_as<T, F64>;

template <typename T>
concept IsPrimitive = IsU<T> || IsS<T> || IsF<T> || IsByte<T> || IsChar<T> || IsBool<T>;

template <typename T>
concept IsNothrowDefaultConstructible = std::is_nothrow_default_constructible_v<T>;

template <typename T>
concept IsNothrowMoveConstructible = std::is_nothrow_move_constructible_v<T>;

template <typename T>
concept IsNothrowMoveAssignable = std::is_nothrow_move_assignable_v<T>;

template <typename T>
concept IsNothrowCopyConstructible = std::is_nothrow_copy_constructible_v<T>;

template <typename T>
concept IsNothrowCopyAssignable = std::is_nothrow_copy_assignable_v<T>;

template <typename T>
concept IsNothrowDestructible = std::is_nothrow_destructible_v<T>;

/**
 * @brief Foundational requirements for most containers in the project.
 * @details Ensures T can be safely moved and destroyed without throwing.
 * Handles cases where T might be const-qualified or a reference (relevant for views like Slice).
 */
template <typename T>
concept IsNothrowBase =
    std::is_nothrow_destructible_v<T> &&
    (std::is_reference_v<T> || std::is_abstract_v<T> ||
     (std::is_nothrow_move_constructible_v<std::remove_cv_t<T>> &&
      (std::is_const_v<T> || std::is_nothrow_move_assignable_v<std::remove_cv_t<T>>)));
} // namespace fr
