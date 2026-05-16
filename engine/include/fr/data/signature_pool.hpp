/**
 * @file signature_pool.hpp
 * @author Kiju
 *
 * @brief SignaturePool stores per-thing signatures.
 */

#pragma once

#include <memory>

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
    using SignatureArray = Array<Signature, MAX_THINGS>;

    explicit SignaturePool(Alloc *alloc = get_ambient_ctx().alloc) noexcept
        : m_alloc(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");
        void *mem = m_alloc->allocate(sizeof(SignatureArray), alignof(SignatureArray));
        m_signatures = std::construct_at(static_cast<SignatureArray *>(mem));
    }

    ~SignaturePool() noexcept {
        std::destroy_at(m_signatures);
        m_alloc->deallocate(m_signatures, sizeof(SignatureArray), alignof(SignatureArray));
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
    SignatureArray *m_signatures{nullptr};
};
} // namespace fr::impl
