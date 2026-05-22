/**
 * @file alloc.hpp
 * @author Kiju
 *
 * @brief Polymorphic allocator interface.
 *
 * @detail Allocator Design.
 * Allocator design is a critical component of Farfocel's memory management. The engine uses a
 * custom allocator interface (`fr::Alloc`) to manage memory efficiently. Allocators are polymorphic
 * and may be stateful. Using polymorphic allocators simplifies memory management across DLL
 * boundaries and enables specialized memory arenas without templating container types.
 *
 * All core owning containers (where "owning" means the container manages its own raw memory) must
 * adhere to the following contract:
 *
 * 1. Interface & Storage Contract
 *
 * - The allocator is stored as a private/protected member variable of type `Alloc* m_alloc`.
 * - The allocator is initialized to `get_ambient_ctx().alloc` by default.
 * - Every owning container provides an explicit constructor that accepts an `Alloc*`.
 * - Every owning container provides a static factory method `with_alloc(Alloc*)`.
 * - Every owning container provides a `const Alloc* alloc() const noexcept` getter to access the
 *   allocator.
 *
 * 2. Propagation Rules
 *
 * Compiler-generated copy and move operations are **fatal** for objects managing raw memory (they
 * lead to double-frees and memory leaks). All owning containers **MUST** explicitly implement or `=
 * delete` their copy/move constructors and assignment operators according to these propagation
 * rules:
 *
 * - Copy Construction: Allocators DO NOT propagate.
 *   - The newly constructed container uses `get_ambient_ctx().alloc` (unless an allocator is
 *     explicitly passed to a specific constructor). It then deep-copies the elements.
 * - Move Construction: Allocators DO propagate.
 *   - The newly constructed container steals both the memory pointer and the `m_alloc` pointer from
 *     the source. The source is left in a valid, empty state.
 * - Copy Assignment: Allocators DO NOT propagate.
 *   - The destination container keeps its existing allocator. It allocates new memory (if
 *     necessary) using its own allocator and deep-copies the elements.
 * - Move Assignment: Allocators DO NOT propagate.
 *   - Fast-Path: If `this->m_alloc == other.m_alloc`, the destination safely steals the memory
 *     pointer from the source.
 *   - Slow-Path: If `this->m_alloc != other.m_alloc`, the destination _cannot_ steal the
 *     memory. It must keep its own allocator, clear its current contents, allocate new memory, and
 *     perform an element-wise move from the source.
 */

#pragma once

#include <algorithm>
#include <cstring>

#include "fr/core/alloc_tracer.hpp"
#include "fr/core/alloc_typedefs.hpp"
#include "fr/core/ctx.hpp"
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
     *
     * @pre `sz != 0`.
     * @pre `alignment` is a power of two.
     * @note Asserts in debug when allocation fails. God only knows what happens in production.
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
     *
     * @pre `ptr` is non-null.
     * @pre `old_sz != 0` and `new_sz != 0`.
     * @pre `alignment` is a power of two.
     * @note Asserts in debug when reallocation fails. God only knows what happens in production.
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
     *
     * @note Respects retry policy.
     * @pre `sz != 0`.
     * @pre `alignment` is a power of two.
     */
    [[nodiscard("Discarding the pointer may lead to memory leaks")]] void *
    try_allocate(USize sz, USize alignment) noexcept {
        FR_ASSERT(sz != 0, "size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "alignment must be power of two");

        const Ctx &ctx = get_ambient_ctx();

        U8 max_retries = ctx.oom_retries;
        for (U8 attempt = 0;; ++attempt) {
            void *result = do_try_allocate(sz, alignment);

#ifdef FR_IS_DEBUG
            ctx.alloc_tracer->record(AllocFrame{
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

            OOMHandler oom_handler = ctx.oom_handler;
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
     *
     * @note Respects retry policy.
     * @pre `ptr` is non-null.
     * @pre `old_sz != 0` and `new_sz != 0`.
     * @pre `alignment` is a power of two.
     */
    [[nodiscard("Discarding the pointer may lead to memory leaks")]] void *
    try_reallocate(void *ptr, USize old_sz, USize new_sz, USize alignment) noexcept {
        FR_ASSERT(ptr, "pointer must be non-null");
        FR_ASSERT(old_sz != 0, "old size must be non-zero");
        FR_ASSERT(new_sz != 0, "new size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "alignment must be power of two");

        const Ctx &ctx = get_ambient_ctx();
        U8 max_retries = ctx.oom_retries;

        for (U8 attempt = 0;; ++attempt) {
            void *result = do_try_reallocate(ptr, old_sz, new_sz, alignment);

#ifdef FR_IS_DEBUG
            ctx.alloc_tracer->record(AllocFrame{
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

            OOMHandler oom_handler = ctx.oom_handler;

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
     *
     * @pre If `ptr` is non-null, `sz != 0`.
     * @pre `alignment` is a power of two.
     */
    void deallocate(void *ptr, USize sz, USize alignment) noexcept {
        if (!ptr) {
            return;
        }

        FR_ASSERT(sz != 0, "size must be non-zero");
        FR_ASSERT(mem::is_valid_alignment(alignment), "alignment must be power of two");

#ifdef FR_IS_DEBUG
        get_ambient_ctx().alloc_tracer->record(AllocFrame{
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
     * @brief Check whether a pointer is owned (has been allocated) by this allocator.
     *
     * @param ptr Pointer to check.
     * @return `OwnershipResult::Unknown` by default.
     */
    virtual OwnershipResult owns(void * /*ptr*/) const noexcept {
        return OwnershipResult::Unknown;
    };

    /**
     * @brief Human-readable allocator name for debugging.
     * @return Allocator `tag`.
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
     * @return Pointer to allocated memory or `nullptr`.
     */
    virtual void *do_try_allocate(USize sz, USize alignment) noexcept = 0;

    /**
     * @brief Default implementation for reallocation.
     *
     * @param ptr Existing allocation.
     * @param old_sz Old size.
     * @param new_sz New size.
     * @param alignment Alignment.
     * @return Pointer to reallocated memory or `nullptr`.
     *
     * @note Reallocates by allocating new memory, copying, then freeing old.
     * @note Copies `min(old_sz, new_sz)` bytes.
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
