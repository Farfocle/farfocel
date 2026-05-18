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
#include "fr/core/typedefs.hpp"
#include "fr/core/typeidx.hpp"
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

    bool check(Thing thing, TypeIdx tidx) const noexcept {
        const Storage &signatures = *m_signatures;
        return signatures[thing.idx()].check(tidx);
    }

    void attach(Thing thing, TypeIdx tidx) noexcept {
        Storage &signatures = *m_signatures;
        signatures[thing.idx()].attach(tidx);
    }

    void detach(Thing thing, TypeIdx tidx) noexcept {
        Storage &signatures = *m_signatures;
        signatures[thing.idx()].detach(tidx);
    }

    void reset(Thing thing) noexcept {
        Storage &signatures = *m_signatures;
        signatures[thing.idx()].clear();
    }

private:
    Alloc *m_alloc{get_ambient_ctx().alloc};
    Storage *m_signatures{nullptr};
};
} // namespace fr::impl
