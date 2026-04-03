/**
 * @file allocator.hpp
 * @author Kiju
 *
 * @brief Polymorphic allocator interface.
 */

#pragma once

#include <algorithm>
#include <cstring>

#include "fr/core/allocator_typedefs.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/time.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Base class for all allocators in fr_stl.
class Allocator {
public:
    virtual ~Allocator() = default;

    /**
     * @brief Allocate memory or abort on failure.
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated memory.
     * @pre @p sz != 0.
     * @pre @p alignment is a power of two.
     * @note Aborts if allocation fails.
     */
    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void *
    allocate(USize sz, USize alignment) noexcept {
        void *result = try_allocate(sz, alignment);
        FR_ASSERT(result, "Failed to allocate memory");

        return result;
    }

    /**
     * @brief Reallocate memory or abort on failure.
     * @param ptr Existing allocation.
     * @param old_sz Old size in bytes.
     * @param new_sz New size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to reallocated memory.
     * @pre @p ptr is non-null.
     * @pre @p old_sz != 0 and @p new_sz != 0.
     * @pre @p alignment is a power of two.
     * @note Aborts if reallocation fails.
     */
    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void *
    reallocate(void *ptr, USize old_sz, USize new_sz, USize alignment) noexcept {
        void *result = try_reallocate(ptr, old_sz, new_sz, alignment);
        FR_ASSERT(result, "Failed to allocate memory");

        return result;
    }

    /**
     * @brief Allocate memory and return nullptr on failure.
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated memory or nullptr.
     * @pre @p sz != 0.
     * @pre @p alignment is a power of two.
     * @note Respects OOM handler and retry policy.
     */
    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void *
    try_allocate(USize sz, USize alignment) noexcept {
        FR_ASSERT(sz != 0, "Allocation size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "Alignment must be a power of two");

        U8 max_retries = globals::get_oom_retries();
        for (U8 attempt = 0;; ++attempt) {
            void *result = do_try_allocate(sz, alignment);

#ifdef FR_IS_DEBUG
            globals::get_allocation_stack()->record(AllocationFrame{
                .timestamp = time::get_steady_now_ns(),
                .action = AllocatorAction::Allocate,
                .prev_pointer = nullptr,
                .next_pointer = result,
                .prev_size = 0,
                .next_size = sz,
                .alignment = alignment,
                .tag = debug_tag(),
                .success = (result != nullptr),
                .attempt = attempt,
            });
#endif

            if (result) {
                return result;
            }

            if (attempt >= max_retries) {
                return nullptr;
            }

            OOMHandler oom_handler = globals::get_oom_handler();

            if (oom_handler == nullptr) {
                return nullptr;
            }

            if (oom_handler(sz, alignment) != OOMHandlerAction::Retry) {
                return nullptr;
            }
        }
    }

    /**
     * @brief Reallocate memory and return nullptr on failure.
     * @param ptr Existing allocation.
     * @param old_sz Old size in bytes.
     * @param new_sz New size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to reallocated memory or nullptr.
     * @pre @p ptr is non-null.
     * @pre @p old_sz != 0 and @p new_sz != 0.
     * @pre @p alignment is a power of two.
     * @note Respects OOM handler and retry policy.
     */
    [[nodiscard("Discarding the pointer leads to memory leaks!")]] void *
    try_reallocate(void *ptr, USize old_sz, USize new_sz, USize alignment) noexcept {
        FR_ASSERT(ptr, "Reallocation source pointer must be non-null");
        FR_ASSERT(old_sz != 0, "Old size must be non-zero");
        FR_ASSERT(new_sz != 0, "New size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "Alignment must be a power of two");

        U8 max_retries = globals::get_oom_retries();
        for (U8 attempt = 0;; ++attempt) {
            void *result = do_try_reallocate(ptr, old_sz, new_sz, alignment);

#ifdef FR_IS_DEBUG
            globals::get_allocation_stack()->record(AllocationFrame{
                .timestamp = time::get_steady_now_ns(),
                .action = AllocatorAction::Reallocate,
                .prev_pointer = ptr,
                .next_pointer = result,
                .prev_size = old_sz,
                .next_size = new_sz,
                .alignment = alignment,
                .tag = debug_tag(),
                .success = (result != nullptr),
                .attempt = attempt,
            });
#endif

            if (result) {
                return result;
            }

            if (attempt >= max_retries) {
                return nullptr;
            }

            OOMHandler oom_handler = globals::get_oom_handler();

            if (oom_handler == nullptr) {
                return nullptr;
            }

            if (oom_handler(new_sz, alignment) != OOMHandlerAction::Retry) {
                return nullptr;
            }
        }
    }

    /**
     * @brief Deallocate memory.
     * @param ptr Allocation to free (may be null).
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @pre If @p ptr is non-null, @p sz != 0.
     * @pre @p alignment is a power of two.
     */
    void deallocate(void *ptr, USize sz, USize alignment) noexcept {
        if (!ptr) {
            return;
        }

        FR_ASSERT(sz != 0, "Deallocation size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "Alignment must be a power of two");

#ifdef FR_IS_DEBUG
        globals::get_allocation_stack()->record(AllocationFrame{
            .timestamp = time::get_steady_now_ns(),
            .action = AllocatorAction::Deallocate,
            .prev_pointer = ptr,
            .next_pointer = nullptr,
            .prev_size = sz,
            .next_size = 0,
            .alignment = alignment,
            .tag = debug_tag(),
            .success = true,
            .attempt = 0,
        });
#endif

        do_deallocate(ptr, sz, alignment);
    }

    /**
     * @brief Check whether @p ptr is owned by this allocator.
     * @param ptr Pointer to check.
     * @return OwnershipResult::Unknown by default.
     */
    virtual OwnershipResult debug_owns(void * /*ptr*/) const noexcept {
        return OwnershipResult::Unknown;
    };

    /// @brief Human-readable allocator name for debugging.
    virtual const char *debug_tag() const noexcept {
        return "UnnamedAllocator";
    };

protected:
    /**
     * @brief Implementation-specific allocation logic.
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated memory or nullptr.
     */
    virtual void *do_try_allocate(USize sz, USize alignment) noexcept = 0;

    /**
     * @brief Default implementation for reallocation.
     * @param ptr Existing allocation.
     * @param old_sz Old size.
     * @param new_sz New size.
     * @param alignment Alignment.
     * @return Pointer to reallocated memory or nullptr.
     * @note Reallocates by allocating new memory, copying, then freeing old.
     * @note Copies min(old_sz, new_sz) bytes.
     */
    virtual void *do_try_reallocate(void *ptr, USize old_sz, USize new_sz,
                                    USize alignment) noexcept {
        void *new_pointer = do_try_allocate(new_sz, alignment);

        if (!new_pointer) {
            return nullptr;
        }

        Byte *dst = static_cast<Byte *>(new_pointer);
        Byte *src = static_cast<Byte *>(ptr);
        USize n = std::min(old_sz, new_sz);

        std::memcpy(dst, src, n);
        do_deallocate(ptr, old_sz, alignment);

        return dst;
    }

    /**
     * @brief Implementation-specific deallocation logic.
     * @param ptr Memory to free.
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     */
    virtual void do_deallocate(void *ptr, USize sz, USize alignment) noexcept = 0;
};
} // namespace fr
