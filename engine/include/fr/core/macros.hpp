/**
 * @file macros.hpp
 * @author Kiju
 *
 * @brief Macros for assertions and visibility for DLLs and static builds.
 */

#pragma once

/// @brief Macro to detect if the build is in debug mode.
#if !defined(NDEBUG)
#define FR_IS_DEBUG 1
#else
#define FR_IS_DEBUG 0
#endif

/// @brief Runtime assertion macro with a custom message.
/// @param cond The condition to evaluate.
/// @param msg A string literal explaining the assertion failure.
#if FR_IS_DEBUG
#include <cassert>
#define FR_ASSERT(cond, msg) assert((cond) && (msg))
#else
#define FR_ASSERT(cond, msg) ((void)0)
#endif

/// @brief Runtime panic implented as macro with false condition.
/// @param msg A stirng literal represetning why panic happened.
#define FR_PANIC(msg) FR_ASSERT(false, msg)

/// @brief Compile-time assertion macro with a custom message.
/// @param cond The condition to evaluate at compile-time.
/// @param msg A string literal explaining the assertion failure.
#define FR_STATIC_ASSERT(cond, msg) static_assert((cond), msg)

/// @brief Symbol visibility macros for DLL boundaries and static builds.
#if defined(FR_STL_STATIC)
// Static build: symbols are part of the object files.
#define FR_API
#else
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef FR_STL_EXPORT
#define FR_API __declspec(dllexport)
#else
#define FR_API __declspec(dllimport)
#endif
#else
#if __GNUC__ >= 4
#define FR_API __attribute__((visibility("default")))
#else
#define FR_API
#endif
#endif
#endif
