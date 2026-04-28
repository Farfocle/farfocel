/**
 * @file lib/globals.cpp
 * @author Kiju
 * @brief Definition of global storage for fr_stl.
 */

#include <atomic>

#include "fr/core/alloc.hpp"
#include "fr/core/alloc_tracer.hpp"
#include "fr/core/alloc_typedefs.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/malloc_alloc.hpp"
#include "fr/core/new_delete_alloc.hpp"

namespace fr::globals {

// ---------------------------------------------------------
// Allocator related globals
// ---------------------------------------------------------

FR_API std::atomic<Alloc *> default_allocator{nullptr};
FR_API std::atomic<OOMHandler> oom_handler{nullptr};
FR_API std::atomic<U8> oom_retries{2};
FR_API std::atomic<AllocTracer *> allocation_stack{nullptr};

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

FR_API Alloc *get_new_delete_allocator() noexcept {
    static NewDeleteAlloc a = NewDeleteAlloc();
    return &a;
}

FR_API Alloc *get_malloc_allocator() noexcept {
    static MallocAlloc a = MallocAlloc();
    return &a;
}

FR_API Alloc *get_default_allocator() noexcept {
    Alloc *alloc = default_allocator.load(std::memory_order_acquire);

    if (!alloc) [[unlikely]] {
        alloc = get_new_delete_allocator();
        default_allocator.store(alloc, std::memory_order_release);
    }

    return alloc;
}

FR_API Alloc *set_default_allocator(Alloc *alloc) noexcept {
    FR_ASSERT(alloc, "allocator must be non-null");

    if (!alloc) [[unlikely]] {
        return get_default_allocator();
    }

    return default_allocator.exchange(alloc, std::memory_order_acq_rel);
}

FR_API AllocTracer *get_allocation_stack() noexcept {
    AllocTracer *stack = allocation_stack.load(std::memory_order_acquire);

    if (!stack) [[unlikely]] {
        stack = new AllocTracer(1 << 16);
        allocation_stack.store(stack, std::memory_order_release);
    }

    return stack;
}

FR_API AllocTracer *set_allocation_stack(AllocTracer *stack) noexcept {
    FR_ASSERT(stack, "stack must be non-null");

    if (!stack) [[unlikely]] {
        return get_allocation_stack();
    }

    return allocation_stack.exchange(stack, std::memory_order_acquire);
}

} // namespace fr::globals
