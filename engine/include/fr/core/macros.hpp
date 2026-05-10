/**
 * @file macros.hpp
 * @author Kiju
 *
 * @brief Macros for assertions and visibility for DLLs and static builds.
 */

#pragma once

/**
 * @brief Symbol visibility macros for DLL boundaries and static builds.
 */
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

/**
 * @brief Macro to detect if the build is in debug mode.
 */
#if !defined(NDEBUG)
#define FR_IS_DEBUG 1
#else
#define FR_IS_DEBUG 0
#endif

/**
 * @brief Runtime assertion macro with a custom message.
 *
 * @param cond The condition to evaluate.
 * @param msg A string literal explaining the assertion failure.
 */
#if FR_IS_DEBUG
#include <cstdio>
#include <cstdlib>

namespace fr::impl {
/**
 * @brief Internal helper to report an assertion failure and abort.
 */
inline void report_assertion_failure(const char *cond, const char *msg, const char *file, int line,
                                     const char *func) noexcept {
    std::fprintf(stderr, "Assertion failed: (%s)\nMessage: %s\nFile: %s\nLine: %d\nFunction: %s\n",
                 cond, msg, file, line, func);
    std::fflush(stderr);
    std::abort();
}
} // namespace fr::impl

#define FR_ASSERT(cond, msg)                                                                       \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            if consteval {                                                                         \
                /* This call will fail constant evaluation if executed. */                         \
                ::fr::impl::report_assertion_failure(#cond, msg, __FILE__, __LINE__,               \
                                                     __FUNCTION__);                                \
            } else {                                                                               \
                ::fr::impl::report_assertion_failure(#cond, msg, __FILE__, __LINE__,               \
                                                     __FUNCTION__);                                \
            }                                                                                      \
        }                                                                                          \
    } while (0)
#else
#define FR_ASSERT(cond, msg) ((void)0)
#endif

/**
 * @brief Runtime panic implented as macro with false condition.
 *
 * @param msg A stirng literal represetning why panic happened.
 */
#define FR_PANIC(msg) FR_ASSERT(false, msg)

/**
 * @brief Compile-time assertion macro with a custom message.
 *
 * @param cond The condition to evaluate at compile-time.
 * @param msg A string literal explaining the assertion failure.
 */
#define FR_STATIC_ASSERT(cond, msg) static_assert((cond), msg)

/**
 * @brief Compile-time panic macro.
 *
 * @param msg A string literal explaining why the panic happened.
 */
#define FR_STATIC_PANIC(msg) static_assert(false, msg)

/**
 * @brief Nothrow requirement macros for compile-time enforcement.
 */
#define FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T)                                          \
    static_assert(::std::is_nothrow_default_constructible_v<T>,                                    \
                  #T " must be nothrow default constructible")

#define FR_STATIC_ASSERT_NOTHROW_MOVE_CONSTRUCTIBLE(T)                                             \
    static_assert(::std::is_nothrow_move_constructible_v<T>,                                       \
                  #T " must be nothrow move constructible")

#define FR_STATIC_ASSERT_NOTHROW_MOVE_ASSIGNABLE(T)                                                \
    static_assert(::std::is_nothrow_move_assignable_v<T>, #T " must be nothrow move assignable")

#define FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T)                                             \
    static_assert(::std::is_nothrow_copy_constructible_v<T>,                                       \
                  #T " must be nothrow copy constructible")

#define FR_STATIC_ASSERT_NOTHROW_COPY_ASSIGNABLE(T)                                                \
    static_assert(::std::is_nothrow_copy_assignable_v<T>, #T " must be nothrow copy assignable")

#define FR_STATIC_ASSERT_NOTHROW_DESTRUCTIBLE(T)                                                   \
    static_assert(::std::is_nothrow_destructible_v<T>, #T " must be nothrow destructible")

#define FR_STATIC_ASSERT_NOTHROW_BASE(T)                                                           \
    FR_STATIC_ASSERT_NOTHROW_DESTRUCTIBLE(T);                                                      \
    FR_STATIC_ASSERT_NOTHROW_MOVE_CONSTRUCTIBLE(T);                                                \
    FR_STATIC_ASSERT_NOTHROW_MOVE_ASSIGNABLE(T)
