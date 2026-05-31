/**
 * @file lib/ctx.cpp
 * @author Kiju
 *
 * @brief Definition of global storage for core.
 */

#include "fr/core/alloc_tracer.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/heap_alloc.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"

namespace fr {
namespace {
alignas(MallocAlloc) Byte core_heap_alloc_mem[sizeof(MallocAlloc)];
alignas(AllocTracer) Byte core_alloc_tracer_mem[sizeof(AllocTracer)];
alignas(TypeRegistry) Byte core_type_registry_mem[sizeof(TypeRegistry)];

Ctx core_ctx{};
} // namespace

namespace glob {
AllocTracer *core_alloc_tracer_ptr{nullptr};
TypeRegistry *core_type_registry_ptr{nullptr};
Alloc *core_heap_alloc_ptr{nullptr};
Ctx *core_ctx_ptr{nullptr};
thread_local Ctx *ambient_ctx_ptr{nullptr};
} // namespace glob

void init_core_ctx() noexcept {
    FR_ASSERT(!glob::core_heap_alloc_ptr, "Heap allocator must not be initialized");
    FR_ASSERT(!glob::core_alloc_tracer_ptr, "Allocation tracer must not be initialized");

    glob::core_alloc_tracer_ptr = new (core_alloc_tracer_mem) AllocTracer(1 << 12);
    glob::core_heap_alloc_ptr = new (core_heap_alloc_mem) MallocAlloc();

    core_ctx.tag = "core";
    core_ctx.alloc = glob::core_heap_alloc_ptr;
    core_ctx.alloc_tracer = glob::core_alloc_tracer_ptr;
    core_ctx.oom_handler = nullptr;
    core_ctx.oom_retries = 2;

    glob::core_ctx_ptr = &core_ctx;
    glob::ambient_ctx_ptr = &core_ctx;

    glob::core_type_registry_ptr = new (core_type_registry_mem) TypeRegistry(core_ctx.alloc);
    core_ctx.type_registry = glob::core_type_registry_ptr;
}

void shutdown_core_ctx() noexcept {
    FR_ASSERT(glob::core_heap_alloc_ptr, "Heap allocator must be initialized");
    FR_ASSERT(glob::core_alloc_tracer_ptr, "Allocation tracer must be initialized");

    glob::core_type_registry_ptr->~TypeRegistry();

    glob::core_alloc_tracer_ptr->~AllocTracer();
    glob::core_alloc_tracer_ptr = nullptr;

    static_cast<MallocAlloc *>(glob::core_heap_alloc_ptr)->~MallocAlloc();
    glob::core_heap_alloc_ptr = nullptr;

    core_ctx.type_registry = nullptr;
    glob::core_ctx_ptr = nullptr;
    glob::ambient_ctx_ptr = nullptr;
}

const Ctx &get_ambient_ctx() noexcept {
    if (glob::ambient_ctx_ptr) [[likely]] {
        return *glob::ambient_ctx_ptr;
    }

    FR_ASSERT(glob::core_ctx_ptr, "Core context must be initialized");
    return *glob::core_ctx_ptr;
}

Ctx &get_ambient_ctx_mut() noexcept {
    if (glob::ambient_ctx_ptr) [[likely]] {
        return *glob::ambient_ctx_ptr;
    }

    FR_ASSERT(glob::core_ctx_ptr, "Core context must be initialized");
    return *glob::core_ctx_ptr;
}

void set_ambient_ctx(Ctx *ctx) noexcept {
    glob::ambient_ctx_ptr = ctx;
}
} // namespace fr
