/**
 * @file signature_pool.hpp
 * @author Kiju
 *
 * @brief SignaturePool stores per-thing signatures.
 */

#pragma once

#include <cstring>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/part.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {
class SignaturePool {
public:
    using Storage = Array<Signature, MAX_THINGS>;

    SignaturePool() noexcept
        : SignaturePool(get_ambient_ctx().alloc) {
    }

    explicit SignaturePool(Alloc *alloc) noexcept {
        m_alloc = alloc;

        void *raw = m_alloc->allocate(sizeof(Storage), alignof(Storage));
        m_signatures = static_cast<Storage *>(raw);

        // Uses std::memset instead of mem::zero_init_range because the bitset uses 1 bit per thing
        // which is probably not aligned properly.
        std::memset(raw, 0, sizeof(Storage));
    }

    ~SignaturePool() noexcept {
        // @safety Bitset is trivially destructible.
        m_alloc->deallocate(m_signatures, sizeof(Storage), alignof(Storage));
    }

    SignaturePool(const SignaturePool &) = delete;
    SignaturePool(SignaturePool &&) = delete;
    SignaturePool &operator=(const SignaturePool &) = delete;
    SignaturePool &operator=(SignaturePool &&) = delete;

    /**
     * @brief Returns the allocator used by this pool.
     */
    const Alloc *alloc() const noexcept {
        return m_alloc;
    }

    /**
     * @brief Returns the capacity of this pool - the maximum number of things (MAX_THINGS).
     */
    USize capacity() const noexcept {
        return MAX_THINGS;
    }

    /**
     * @brief Returns the signature by index.
     * @pre idx < MAX_THINGS.
     */
    const Signature &get_by_idx(ThingIdx idx) const noexcept {
        FR_ASSERT(idx < MAX_THINGS, "idx out of bounds");
        return (*m_signatures)[idx];
    }

    /**
     * @brief Returns the signature by index.
     * @pre idx < MAX_THINGS.
     * @warning Thread-unsafe. Caller must ensure no concurrent access.
     */
    Signature &get_mut_by_idx(ThingIdx idx) noexcept {
        FR_ASSERT(idx < MAX_THINGS, "idx out of bounds");
        return (*m_signatures)[idx];
    }

    /**
     * @brief Returns the signature for a thing.
     */
    const Signature &get(Thing thing) const noexcept {
        return get_by_idx(thing.idx());
    }

    /**
     * @brief Returns the signature for a thing.
     * @warning Thread-unsafe. Caller must ensure no concurrent access.
     */
    Signature &get_mut(Thing thing) noexcept {
        return get_mut_by_idx(thing.idx());
    }

private:
    Alloc *m_alloc{get_ambient_ctx().alloc};
    Storage *m_signatures{nullptr};
};
} // namespace fr::impl
