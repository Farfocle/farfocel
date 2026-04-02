/**
 * @file globals.hpp
 * @author Kiju
 *
 * @brief Global state storage and accessors.
 *
 * This file contains OOM handlers and other library-wide settings that must persist across DLL
 * boundaries.
 */

#pragma once

#include <atomic>

#include "fr/core/allocator_typedefs.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

class Allocator;
class TrackingAllocator;

namespace globals {

extern FR_API std::atomic<Allocator *> default_allocator;
extern FR_API std::atomic<OOMHandler> oom_handler;
extern FR_API std::atomic<U8> oom_retries;

/**
 * @brief Get the current OOM handler.
 * @return The currently registered handler or nullptr.
 */
FR_API OOMHandler get_oom_handler() noexcept;

/**
 * @brief Set the OOM handler.
 * @param handler New handler or nullptr to disable.
 * @return Previously registered handler.
 */
FR_API OOMHandler set_oom_handler(OOMHandler handler) noexcept;

/**
 * @brief Get the maximum number of OOM retry attempts.
 */
FR_API U8 get_oom_retries() noexcept;

/**
 * @brief Set the maximum number of OOM retry attempts.
 * @param retries Retry count.
 * @return Previous retry count.
 */
FR_API U8 set_oom_retries(U8 retries) noexcept;

/**
 * @brief Get the global new/delete heap allocator.
 */
FR_API Allocator *get_new_delete_allocator() noexcept;

/**
 * @brief Get the global malloc heap allocator (untracked).
 */
FR_API Allocator *get_malloc_allocator_untracked() noexcept;

/**
 * @brief Get the global malloc heap allocator.
 */
FR_API Allocator *get_malloc_allocator() noexcept;

/**
 * @brief Get the current default allocator.
 * @note The default allocator is MallocAllocator.
 */
FR_API Allocator *get_default_allocator() noexcept;

/**
 * @brief Set the default allocator.
 * @param allocator New default allocator.
 * @return Previous default allocator.
 * @pre @p allocator is non-null.
 */
FR_API Allocator *set_default_allocator(Allocator *allocator) noexcept;
} // namespace globals
} // namespace fr
