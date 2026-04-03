/**
 * @file lib/globals.cpp
 * @author Kiju
 * @brief Definition of global storage for fr_stl.
 */

#include "fr/core/globals.hpp"
#include "fr/core/allocator.hpp"
#include "fr/core/allocator_debug.hpp"
#include "fr/core/allocator_typedefs.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/malloc_allocator.hpp"
#include "fr/core/new_delete_allocator.hpp"
#include <atomic>

namespace fr::globals {

// ---------------------------------------------------------
// Allocator related globals
// ---------------------------------------------------------

FR_API std::atomic<Allocator *> default_allocator{nullptr};
FR_API std::atomic<OOMHandler> oom_handler{nullptr};
FR_API std::atomic<U8> oom_retries{2};
FR_API std::atomic<AllocationStack *> allocation_stack{nullptr};

// ---------------------------------------------------------
// Accessors
// ---------------------------------------------------------

FR_API OOMHandler get_oom_handler() noexcept {
    return oom_handler.load(std::memory_order_acquire);
}

FR_API OOMHandler set_oom_handler(OOMHandler handler) noexcept {
    return oom_handler.exchange(handler, std::memory_order_acq_rel);
}

FR_API U8 get_oom_retries() noexcept {
    return oom_retries.load(std::memory_order_acquire);
}

FR_API U8 set_oom_retries(U8 retries) noexcept {
    return oom_retries.exchange(retries, std::memory_order_acq_rel);
}

FR_API Allocator *get_new_delete_allocator() noexcept {
    static NewDeleteAllocator a = NewDeleteAllocator();
    return &a;
}

FR_API Allocator *get_malloc_allocator() noexcept {
    static MallocAllocator a = MallocAllocator();
    return &a;
}

FR_API Allocator *get_default_allocator() noexcept {
    Allocator *alloc = default_allocator.load(std::memory_order_acquire);

    if (!alloc) [[unlikely]] {
        alloc = get_new_delete_allocator();
        default_allocator.store(alloc, std::memory_order_release);
    }

    return alloc;
}

FR_API Allocator *set_default_allocator(Allocator *allocator) noexcept {
    FR_ASSERT(allocator, "Default allocator must be non-null");

    if (!allocator) [[unlikely]] {
        return get_default_allocator();
    }

    return default_allocator.exchange(allocator, std::memory_order_acq_rel);
}

FR_API AllocationStack *get_allocation_stack() noexcept {
    AllocationStack *stack = allocation_stack.load(std::memory_order_acquire);

    if (!stack) [[unlikely]] {
        stack = new AllocationStack(1 << 16);
        allocation_stack.store(stack, std::memory_order_release);
    }

    return stack;
}

FR_API AllocationStack *set_allocation_stack(AllocationStack *stack) noexcept {
    FR_ASSERT(
        stack,
        "fr::globals::set_alocation_stack(AllocationStack* stack) -> `stack` cannot be nullptr");

    if (!stack) [[unlikely]] {
        return get_allocation_stack();
    }

    return allocation_stack.exchange(stack, std::memory_order_acquire);
}

} // namespace fr::globals
