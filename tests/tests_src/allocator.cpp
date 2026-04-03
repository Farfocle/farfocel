// author: Kiju
#include "fr/core/allocator.hpp"
#include "doctest.h"
#include "fr/core/globals.hpp"
#include "fr/core/malloc_allocator.hpp"

namespace fr {

TEST_CASE("NewDeleteAllocator - Basic") {
    Allocator *alloc = globals::get_new_delete_allocator();
    CHECK(alloc != nullptr);

    // Test basic allocation
    void *ptr = alloc->allocate(1024, 8);
    CHECK(ptr != nullptr);
    alloc->deallocate(ptr, 1024, 8);

    // Test overalignment
    void *ptr2 = alloc->allocate(1024, 64);
    CHECK(ptr2 != nullptr);
    CHECK((reinterpret_cast<USize>(ptr2) % 64) == 0);
    alloc->deallocate(ptr2, 1024, 64);
}

TEST_CASE("MallocAllocator - Basic") {
    Allocator *alloc = globals::get_malloc_allocator();
    CHECK(alloc != nullptr);

    // Test basic allocation
    void *ptr = alloc->allocate(1024, 8);
    CHECK(ptr != nullptr);
    alloc->deallocate(ptr, 1024, 8);

    // Test overalignment
    void *ptr2 = alloc->allocate(1024, 64);
    CHECK(ptr2 != nullptr);
    CHECK((reinterpret_cast<USize>(ptr2) % 64) == 0);
    alloc->deallocate(ptr2, 1024, 64);
}

TEST_CASE("MallocAllocator - Reallocate") {
    Allocator *alloc = globals::get_malloc_allocator();

    void *ptr = alloc->allocate(1024, 8);
    std::memset(ptr, 0xAA, 1024);

    void *new_ptr = alloc->reallocate(ptr, 1024, 2048, 8);
    CHECK(new_ptr != nullptr);

    // Check if data is preserved
    U8 *data = static_cast<U8 *>(new_ptr);
    for (USize i = 0; i < 1024; ++i) {
        if (data[i] != 0xAA) {
            FAIL("Data not preserved after reallocate");
            break;
        }
    }

    alloc->deallocate(new_ptr, 2048, 8);
}

class FailAllocator final : public Allocator {
public:
    U32 call_count = 0;

protected:
    void *do_try_allocate(USize, USize) noexcept override {
        call_count++;
        return nullptr;
    }
    void do_deallocate(void *, USize, USize) noexcept override {
    }
};

static USize g_oom_called_size = 0;
static USize g_oom_called_alignment = 0;
static U32 g_oom_call_count = 0;

static OOMHandlerAction oom_retry_handler(USize sz, USize alignment) noexcept {
    g_oom_called_size = sz;
    g_oom_called_alignment = alignment;
    g_oom_call_count++;
    return OOMHandlerAction::Retry;
}

static OOMHandlerAction oom_fail_handler(USize, USize) noexcept {
    g_oom_call_count++;
    return OOMHandlerAction::Fail;
}

TEST_CASE("OOM Handler Layer") {
    FailAllocator fail_alloc;

    // Reset globals
    g_oom_called_size = 0;
    g_oom_called_alignment = 0;
    g_oom_call_count = 0;

    // Test default retry behavior
    globals::set_oom_handler(oom_retry_handler);
    globals::set_oom_retries(2);

    void *ptr = fail_alloc.try_allocate(100, 16);
    CHECK(ptr == nullptr);
    CHECK(fail_alloc.call_count == 3); // Initial + 2 retries
    CHECK(g_oom_call_count == 2);
    CHECK(g_oom_called_size == 100);
    CHECK(g_oom_called_alignment == 16);

    // Test fail action
    g_oom_call_count = 0;
    fail_alloc.call_count = 0;
    globals::set_oom_handler(oom_fail_handler);

    ptr = fail_alloc.try_allocate(100, 16);
    CHECK(ptr == nullptr);
    CHECK(fail_alloc.call_count == 1);
    CHECK(g_oom_call_count == 1);

    // Test no handler
    globals::set_oom_handler(nullptr);
    fail_alloc.call_count = 0;
    ptr = fail_alloc.try_allocate(100, 16);
    CHECK(ptr == nullptr);
    CHECK(fail_alloc.call_count == 1);

    // Restore default state
    globals::set_oom_retries(2);
}

TEST_CASE("Default Allocator Management") {
    Allocator *original = globals::get_default_allocator();
    MallocAllocator my_alloc;

    globals::set_default_allocator(&my_alloc);
    CHECK(globals::get_default_allocator() == &my_alloc);

    globals::set_default_allocator(original);
    CHECK(globals::get_default_allocator() == original);
}

TEST_CASE("Allocation Debug Stack Management") {
    AllocationStack stack(16);
    AllocationStack *original = globals::get_allocation_stack();

    AllocationStack *previous = globals::set_allocation_stack(&stack);
    CHECK(previous == original);
    CHECK(globals::get_allocation_stack() == &stack);

    stack.record(AllocationFrame{
        .timestamp = 1,
        .action = AllocatorAction::Allocate,
        .prev_pointer = nullptr,
        .next_pointer = reinterpret_cast<void *>(0x1),
        .prev_size = 0,
        .next_size = 64,
        .alignment = 8,
        .tag = "test",
        .success = true,
        .attempt = 0,
    });

    CHECK(stack.count() == 1);
    CHECK(stack.frames().size() == 1);
    CHECK(stack.frames()[0].tag == "test");

    globals::set_allocation_stack(original);
}

} // namespace fr
