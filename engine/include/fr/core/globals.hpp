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

#include "fr/core/allocation_stack.hpp"
#include "fr/core/allocator_typedefs.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

class Allocator;

namespace globals {

extern FR_API std::atomic<OOMHandler> oom_handler;
extern FR_API std::atomic<U8> oom_retries;
extern FR_API std::atomic<Allocator *> default_allocator;
/**
 * @brief Debug allocation call stack storage used by allocator instrumentation.
 */
extern FR_API std::atomic<AllocationStack *> allocation_stack;

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

/**
 * @brief Get the current debug allocation stack storage.
 * @note Lazily creates a default stack when none is set.
 */
FR_API AllocationStack *get_allocation_stack() noexcept;

/**
 * @brief Set the debug allocation stack storage.
 * @param stack Non-null debug stack instance.
 * @return Previously configured stack.
 */
FR_API AllocationStack *set_allocation_stack(AllocationStack *stack) noexcept;
} // namespace globals
} // namespace fr
