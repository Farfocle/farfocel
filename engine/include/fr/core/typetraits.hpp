/**
 * @file typetraits.hpp
 * @author Kiju
 *
 * @brief Collection of useful type traits used across the core.
 */

#pragma once

#include "fr/core/typedefs.hpp"
#include <concepts>
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
} // namespace fr
