/**
 * @file new_delete_allocator.hpp
 * @author Kiju
 * @brief Allocator backed by global new and delete.
 */

#pragma once

#include <new>

#include "fr/core/alloc.hpp"
#include "fr/core/mem.hpp"

namespace fr {

/**
 * @brief Allocator backed by global new/delete.
 */
class NewDeleteAlloc final : public Alloc {

protected:
    void *do_try_allocate(USize sz, USize alignment) noexcept override {
        if (mem::is_overaligned(alignment)) [[unlikely]] {
            return ::operator new(sz, std::align_val_t(alignment), std::nothrow);
        }

        return ::operator new(sz, std::nothrow);
    }

    void do_deallocate(void *ptr, USize sz, USize alignment) noexcept override {
        if (mem::is_overaligned(alignment)) [[unlikely]] {
            ::operator delete(ptr, sz, std::align_val_t(alignment));
        } else {
            ::operator delete(ptr, sz);
        }
    }

    /**
     * @brief Ownership information is not available for NewDeleteAllocator.
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
        return "NewDeleteAllocator";
    }
};
} // namespace fr
