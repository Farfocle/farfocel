/**
 * @file allocator.hpp
 * @author Kiju
 *
 * @brief Polymorphic allocator interface.
 */

#pragma once

#include <algorithm>
#include <cstring>

#include "fr/core/alloc_typedefs.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/time.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Base class for all polymorphic allocators.
 */
class Alloc {
public:
    virtual ~Alloc() = default;

    /**
     * @brief Allocate memory or abort on failure.
     *
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated memory.
     * @pre sz != 0.
     * @pre alignment is a power of two.
     * @note Aborts if allocation fails.
     */
    [[nodiscard("Discarding the pointer may lead to memory leaks")]] void *
    allocate(USize sz, USize alignment) noexcept {
        void *result = try_allocate(sz, alignment);
        FR_ASSERT(result, "allocation failed");

        return result;
    }

    /**
     * @brief Reallocate memory or abort on failure.
     *
     * @param ptr Existing allocation.
     * @param old_sz Old size in bytes.
     * @param new_sz New size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to reallocated memory.
     * @pre ptr is non-null.
     * @pre old_sz != 0 and new_sz != 0.
     * @pre alignment is a power of two.
     * @note Aborts if reallocation fails.
     */
    [[nodiscard("Discarding the pointer may lead to memory leaks")]] void *
    reallocate(void *ptr, USize old_sz, USize new_sz, USize alignment) noexcept {
        void *result = try_reallocate(ptr, old_sz, new_sz, alignment);
        FR_ASSERT(result, "reallocation failed");

        return result;
    }

    /**
     * @brief Allocate memory and return nullptr on failure.
     *
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated memory or nullptr.
     * @pre sz != 0.
     * @pre alignment is a power of two.
     * @note Respects OOM handler and retry policy.
     */
    [[nodiscard("Discarding the pointer may lead to memory leaks")]] void *
    try_allocate(USize sz, USize alignment) noexcept {
        FR_ASSERT(sz != 0, "size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "alignment must be power of two");

        U8 max_retries = globals::get_oom_retries();
        for (U8 attempt = 0;; ++attempt) {
            void *result = do_try_allocate(sz, alignment);

#ifdef FR_IS_DEBUG
            globals::get_allocation_stack()->record(AllocFrame{
                .timestamp = time::get_steady_now_ns(),
                .action = AllocAction::Allocate,
                .prev_pointer = nullptr,
                .next_pointer = result,
                .prev_size = 0,
                .next_size = sz,
                .alignment = alignment,
                .tag = tag(),
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
     *
     * @param ptr Existing allocation.
     * @param old_sz Old size in bytes.
     * @param new_sz New size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to reallocated memory or nullptr.
     * @pre ptr is non-null.
     * @pre old_sz != 0 and new_sz != 0.
     * @pre alignment is a power of two.
     * @note Respects OOM handler and retry policy.
     */
    [[nodiscard("Discarding the pointer may lead to memory leaks")]] void *
    try_reallocate(void *ptr, USize old_sz, USize new_sz, USize alignment) noexcept {
        FR_ASSERT(ptr, "pointer must be non-null");
        FR_ASSERT(old_sz != 0, "old size must be non-zero");
        FR_ASSERT(new_sz != 0, "new size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "alignment must be power of two");

        U8 max_retries = globals::get_oom_retries();

        for (U8 attempt = 0;; ++attempt) {
            void *result = do_try_reallocate(ptr, old_sz, new_sz, alignment);

#ifdef FR_IS_DEBUG
            globals::get_allocation_stack()->record(AllocFrame{
                .timestamp = time::get_steady_now_ns(),
                .action = AllocAction::Reallocate,
                .prev_pointer = ptr,
                .next_pointer = result,
                .prev_size = old_sz,
                .next_size = new_sz,
                .alignment = alignment,
                .tag = tag(),
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
     *
     * @param ptr Allocation to free (may be null).
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @pre If ptr is non-null, sz != 0.
     * @pre alignment is a power of two.
     */
    void deallocate(void *ptr, USize sz, USize alignment) noexcept {
        if (!ptr) {
            return;
        }

        FR_ASSERT(sz != 0, "size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "alignment must be power of two");

#ifdef FR_IS_DEBUG
        globals::get_allocation_stack()->record(AllocFrame{
            .timestamp = time::get_steady_now_ns(),
            .action = AllocAction::Deallocate,
            .prev_pointer = ptr,
            .next_pointer = nullptr,
            .prev_size = sz,
            .next_size = 0,
            .alignment = alignment,
            .tag = tag(),
            .success = true,
            .attempt = 0,
        });
#endif

        do_deallocate(ptr, sz, alignment);
    }

    /**
     * @brief Check whether ptr is owned by this allocator.
     *
     * @param ptr Pointer to check.
     * @return OwnershipResult::Unknown by default.
     */
    virtual OwnershipResult owns(void * /*ptr*/) const noexcept {
        return OwnershipResult::Unknown;
    };

    /**
     * @brief Human-readable allocator name for debugging.
     *
     * @return Allocator name.
     */
    virtual const char *tag() const noexcept {
        return "UnnamedAllocator";
    };

protected:
    /**
     * @brief Implementation-specific allocation logic.
     *
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated memory or nullptr.
     */
    virtual void *do_try_allocate(USize sz, USize alignment) noexcept = 0;

    /**
     * @brief Default implementation for reallocation.
     *
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
        void *new_ptr = do_try_allocate(new_sz, alignment);

        if (!new_ptr) {
            return nullptr;
        }

        Byte *dst = static_cast<Byte *>(new_ptr);
        Byte *src = static_cast<Byte *>(ptr);
        USize n = std::min(old_sz, new_sz);

        std::memcpy(dst, src, n);
        do_deallocate(ptr, old_sz, alignment);

        return dst;
    }

    /**
     * @brief Implementation-specific deallocation logic.
     *
     * @param ptr Memory to free.
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     */
    virtual void do_deallocate(void *ptr, USize sz, USize alignment) noexcept = 0;
};
} // namespace fr
