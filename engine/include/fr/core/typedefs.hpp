/**
 * @file typedefs.hpp
 * @brief Common fixed-width and size type aliases.
 */
#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief Unsigned 8-bit integer.
 */
using U8 = std::uint8_t;

/**
 * @brief Unsigned 16-bit integer.
 */
using U16 = std::uint16_t;

/**
 * @brief Unsigned 32-bit integer.
 */
using U32 = std::uint32_t;

/**
 * @brief Unsigned 64-bit integer.
 */
using U64 = std::uint64_t;

/**
 * @brief Signed 8-bit integer.
 */
using S8 = std::int8_t;

/**
 * @brief Signed 16-bit integer.
 */
using S16 = std::int16_t;

/**
 * @brief Signed 32-bit integer.
 */
using S32 = std::int32_t;

/**
 * @brief Signed 64-bit integer.
 */
using S64 = std::int64_t;

/**
 * @brief Unsigned integer type for sizes and counts.
 */
using USize = std::size_t;

/**
 * @brief 32-bit floating-point type.
 */
using F32 = float;

/**
 * @brief 64-bit floating-point type.
 */
using F64 = double;

/**
 * @brief Byte type for raw memory operations.
 */
using Byte = std::byte;
