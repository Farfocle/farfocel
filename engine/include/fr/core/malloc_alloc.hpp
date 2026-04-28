/**
 * @file malloc_allocator.hpp
 * @author Kiju
 *
 * @brief Allocator backed by malloc and free.
 */

#pragma once

#include <cstdlib>

#include "fr/core/alloc.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace fr {

/**
 * @brief Allocator backed by standard malloc and free.
 *
 * Uses the standard C library malloc, realloc, and free.
 * Supports aligned allocations using platform-specific functions
 * (_aligned_malloc on Windows, posix_memalign on others).
 */
class MallocAlloc final : public Alloc {
protected:
    void *do_try_allocate(USize sz, USize alignment) noexcept override {
        if (!mem::is_overaligned(alignment)) [[likely]] {
            return std::malloc(sz);
        }

#if defined(_WIN32)
        return _aligned_malloc(sz, alignment);
#else
        void *ptr = nullptr;

        if (S32 result =
                ::posix_memalign(&ptr, static_cast<USize>(alignment), static_cast<USize>(sz));
            result != 0) {
            return nullptr;
        }

        return ptr;
#endif
    }

    void *do_try_reallocate(void *ptr, USize old_sz, USize new_sz,
                            USize alignment) noexcept override {
        if (!mem::is_overaligned(alignment)) [[likely]] {
            return std::realloc(ptr, new_sz);
        }

#if defined(_WIN32)
        return _aligned_realloc(ptr, new_sz, alignment);
#else
        return Alloc::do_try_reallocate(ptr, old_sz, new_sz, alignment);
#endif
    }

    void do_deallocate(void *ptr, USize /*sz*/, USize alignment) noexcept override {
        if (!mem::is_overaligned(alignment)) [[likely]] {
            std::free(ptr);
            return;
        }

#if defined(_WIN32)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

    /**
     * @brief Ownership information is not available for MallocAllocator.
     *
     * @param ptr Pointer to check.
     * @return OwnershipResult::Unknown.
     */
    OwnershipResult owns(void * /*ptr*/) const noexcept override {
        return OwnershipResult::Unknown;
    }

    /**
     * @brief Returns the allocator tag.
     *
     * @return Tag string.
     */
    const char *tag() const noexcept override {
        return "MallocAllocator";
    }
};
} // namespace fr
