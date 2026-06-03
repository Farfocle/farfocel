/**
 * @file part.hpp
 * @author Kiju
 *
 * @brief Part is an integral part of the hidden ECS system. Normal people call it a component, but
 * it is too wordy for my taste. So part it is.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/bitset.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/slice.hpp"
#include "fr/data/thing.hpp"

namespace fr {
/**
 * @brief Maximum number of unique parts.
 * @note This limit includes:
 * - parts
 * - scripts
 * - resources
 */
constexpr USize MAX_PARTS = 128;

/**
 * @brief Interface for a bitset representation of parts owned by a thing.
 */
class Signature {
public:
    // ------------------------------------ Typedefs & Constructors & Destructor
    using Storage = Bitset<MAX_PARTS>;

    Signature() = default;
    Signature(const Signature &) noexcept = default;
    Signature(Signature &&) noexcept = default;
    Signature &operator=(const Signature &) noexcept = default;
    Signature &operator=(Signature &&) noexcept = default;
    ~Signature() noexcept = default;

    template <typename... Parts>
    static Signature from_parts() noexcept {
        Signature signature;
        (signature.insert(TypeIdx::from_type<Parts>()), ...);
        return signature;
    }

    // ------------------------------------------------------------ Operatations

    /// @brief Returns the underlying bitset.
    const Storage &bitset() const noexcept {
        return m_bits;
    }

    /// @brief Destroy all parts - clear all bits.
    void destroy_all() noexcept {
        m_bits.zero_all();
    }

    /**
     * @brief Attach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void insert(TypeIdx tidx) noexcept {
        m_bits.one_bit(static_cast<USize>(tidx.idx()));
    }

    /**
     * @brief Detach a part type by its TypeIdx.
     * @param idx Type index of the part.
     */
    void destroy(TypeIdx tidx) noexcept {
        m_bits.zero_bit(static_cast<USize>(tidx.idx()));
    }

    /**
     * @brief Check whether a part type is attached by its TypeIdx.
     * @param idx Type index of the part.
     * @return True if attached, false otherwise.
     */
    bool has(TypeIdx tidx) const noexcept {
        return m_bits.check_bit(static_cast<USize>(tidx.idx()));
    }

    /// @brief Check whether any part is attached.
    bool any() const noexcept {
        return m_bits.any();
    }

    /// @brief Check whether none part is attached.
    bool none() const noexcept {
        return m_bits.none();
    }

    /// @brief Equality comparison for signatures.
    friend bool operator==(const Signature &a, const Signature &b) noexcept {
        return a.m_bits == b.m_bits;
    }

    /// @brief Inequality comparison for signatures.
    friend bool operator!=(const Signature &a, const Signature &b) noexcept {
        return !(a == b);
    }

private:
    // ----------------------------------------------------------------- Members
    Storage m_bits{};
};

// =============================================================== SignaturePool

namespace impl {
class SignaturePool {
public:
    // --------------------------------------------- Constructors and Destructor

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

    // ---------------------------------------------------------- Storage Access

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
     * @brief Returns a reference to the signature storage.
     */
    const Storage &signatures() const noexcept {
        return *m_signatures;
    }

    // ---------------------------------------------------- Per Thing Operations

    const Signature &get(Thing thing) const noexcept {
        const Storage &signatures = *m_signatures;
        return signatures[thing.idx()];
    }

    bool owns(Thing thing, TypeIdx tidx) const noexcept {
        const Storage &signatures = *m_signatures;
        return signatures[thing.idx()].has(tidx);
    }

    void insert(Thing thing, TypeIdx tidx) noexcept {
        Storage &signatures = *m_signatures;
        signatures[thing.idx()].insert(tidx);
    }

    void destroy(Thing thing, TypeIdx tidx) noexcept {
        Storage &signatures = *m_signatures;
        signatures[thing.idx()].destroy(tidx);
    }

    void destroy_all(Thing thing) noexcept {
        Storage &signatures = *m_signatures;
        signatures[thing.idx()].destroy_all();
    }

    // --------------------------------------------------------------- Protocols

    template <typename Archive>
    void shape(Archive &archive) noexcept {
        m_signatures->shape(archive);
    }

private:
    // -------------------------------------------------------- Member Variables
    Alloc *m_alloc{get_ambient_ctx().alloc};
    Storage *m_signatures{nullptr};
};

// ======================================================================== PartPool

/**
 * @brief Dense sparse-set storage for a single part type T.
 * @tparam T Part type. Must be default-constructible (for the stub).
 */
template <typename T>
    requires std::is_default_constructible_v<T>
class PartPool {
public:
    // ----------------------------------------------- Constructors & Destructor

    PartPool() noexcept;
    explicit PartPool(Alloc *alloc) noexcept;

    PartPool(const PartPool &) = delete;
    PartPool(PartPool &&) = delete;
    PartPool &operator=(const PartPool &) = delete;
    PartPool &operator=(PartPool &&) = delete;

    ~PartPool() noexcept = default;

    // ----------------------------------------------------------------- Sizing

    /**
     * @brief Reserves capacity in the parts and part→thing arrays.
     */
    void reserve_parts(USize size) noexcept;

    /**
     * @brief Reserves capacity in the thing→part lookup array.
     */
    void reserve_lookup(USize size) noexcept;

    /**
     * @brief Returns the total number of stored parts including the stub.
     */
    USize part_count() const noexcept;

    // --------------------------------------------------------- Internal Slices

    /// @brief Slice of all parts including the stub (index 0).
    Slice<T> parts_with_stub_mut() noexcept;

    /// @brief Slice of parts excluding the stub.
    Slice<T> parts_mut() noexcept;

    /// @brief Sparse slice mapping thing index → part index; includes stub entry.
    Slice<USize> thing_to_part_with_stub_mut() noexcept;

    /// @brief Sparse slice mapping thing index → part index; excludes stub entry.
    Slice<USize> thing_to_part_mut() noexcept;

    /// @brief Dense slice mapping part index → owning thing; includes stub entry.
    Slice<Thing> part_to_thing_with_stub_mut() noexcept;

    /// @brief Dense slice mapping part index → owning thing; includes stub entry (const).
    Slice<const Thing> part_to_thing_with_stub() const noexcept;

    /// @brief Dense slice mapping part index → owning thing; excludes stub entry.
    Slice<Thing> part_to_thing_mut() noexcept;

    // ------------------------------------------------------------ Part Getters

    /// @brief Returns a reference to the stub (the nil-thing part).
    T &get_stub() noexcept;

    /**
     * @brief Returns a pointer to the part owned by the thing, or nullptr if not present.
     * @note Checks index bounds and whether the thing actually owns the part.
     */
    T *get_checked(Thing thing) noexcept;

    /**
     * @brief Returns a pointer to the part owned by the thing.
     * @pre Caller must ensure thing index is in-bounds and the thing owns this part.
     */
    T *get_unchecked(Thing thing) noexcept;

    // -------------------------------------------------------- Part Mutations

    /**
     * @brief Inserts a new part for a thing.
     * @pre Caller must ensure the thing does NOT already own this part.
     */
    template <typename... Args>
    T &emplace_unchecked(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Overwrites the existing part for a thing in-place.
     * @pre Caller must ensure the thing DOES own this part.
     */
    template <typename... Args>
    T &override_unchecked(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Convenience insert by const reference.
     * @pre Same as emplace_unchecked.
     */
    T &insert_unchecked(Thing thing, const T &part) noexcept;

    /**
     * @brief Convenience insert by rvalue reference.
     * @pre Same as emplace_unchecked.
     */
    T &insert_unchecked(Thing thing, T &&part) noexcept;

    /**
     * @brief Destroys the part owned by the thing. Swaps with the last element.
     * @pre Caller must ensure thing is not nil and DOES own this part.
     */
    void destroy_unchecked(Thing thing) noexcept;

    /**
     * @brief Destroys the part owned by the thing if present.
     * @return true if destroyed, false if the thing did not own this part.
     */
    bool destroy_checked(Thing thing) noexcept;

private:
    // -------------------------------------------------------- Member Variables
    Alloc *m_alloc{get_ambient_ctx().alloc};

    /// Dense part storage; index 0 is the permanent stub.
    DynamicArray<T> m_parts{};

    /// Sparse map: thing.idx() → index in m_parts (0 = not owned / stub).
    DynamicArray<USize> m_thing_to_part{};

    /// Dense map: part index → owning thing (mirrors m_parts).
    DynamicArray<Thing> m_part_to_thing{};
};

// ============================================= PartPool Method Implementations

template <typename T>
    requires std::is_default_constructible_v<T>
inline PartPool<T>::PartPool() noexcept
    : PartPool(get_ambient_ctx().alloc) {
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline PartPool<T>::PartPool(Alloc *alloc) noexcept {
    m_alloc = alloc;
    m_parts = DynamicArray<T>::with_alloc(alloc);
    m_thing_to_part = DynamicArray<USize>::with_alloc(alloc);
    m_part_to_thing = DynamicArray<Thing>::with_alloc(alloc);

    m_parts.push_back(T{});       // stub part at index 0
    m_thing_to_part.push_back(0); // nil thing maps to stub
    m_part_to_thing.push_back(Thing::nil());
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline void PartPool<T>::reserve_parts(USize size) noexcept {
    FR_ASSERT(size <= MAX_THINGS, "size exceeds ThingIdx limit");
    m_parts.reserve(size);
    m_part_to_thing.reserve(size);
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline void PartPool<T>::reserve_lookup(USize size) noexcept {
    FR_ASSERT(size <= MAX_THINGS, "size exceeds ThingIdx limit");
    m_thing_to_part.reserve(size);
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline USize PartPool<T>::part_count() const noexcept {
    return m_parts.size();
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline Slice<T> PartPool<T>::parts_with_stub_mut() noexcept {
    return m_parts.slice_mut();
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline Slice<T> PartPool<T>::parts_mut() noexcept {
    return m_parts.slice_mut_from(1);
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline Slice<USize> PartPool<T>::thing_to_part_with_stub_mut() noexcept {
    return m_thing_to_part.slice_mut();
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline Slice<USize> PartPool<T>::thing_to_part_mut() noexcept {
    return m_thing_to_part.slice_mut_from(1);
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline Slice<Thing> PartPool<T>::part_to_thing_with_stub_mut() noexcept {
    return m_part_to_thing.slice_mut();
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline Slice<const Thing> PartPool<T>::part_to_thing_with_stub() const noexcept {
    return m_part_to_thing.slice();
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline Slice<Thing> PartPool<T>::part_to_thing_mut() noexcept {
    return m_part_to_thing.slice_mut_from(1);
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline T &PartPool<T>::get_stub() noexcept {
    return m_parts[0];
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline T *PartPool<T>::get_checked(Thing thing) noexcept {
    ThingIdx idx = thing.idx();
    if (idx >= m_thing_to_part.size()) {
        return nullptr;
    }
    USize part_idx = m_thing_to_part[idx];
    if (part_idx == 0) {
        return nullptr;
    }
    return &m_parts[part_idx];
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline T *PartPool<T>::get_unchecked(Thing thing) noexcept {
    FR_ASSERT(thing.idx() < m_thing_to_part.size(), "thing index out of bounds");
    return &m_parts[m_thing_to_part[thing.idx()]];
}

template <typename T>
    requires std::is_default_constructible_v<T>
template <typename... Args>
inline T &PartPool<T>::emplace_unchecked(Thing thing, Args &&...args) noexcept {
    ThingIdx idx = thing.idx();

    if (m_thing_to_part.size() <= idx) [[unlikely]] {
        m_thing_to_part.grow_default(idx + 1);
    }

    m_thing_to_part[idx] = m_parts.size();
    m_parts.emplace_back(std::forward<Args>(args)...);
    m_part_to_thing.emplace_back(thing);

    return m_parts.back();
}

template <typename T>
    requires std::is_default_constructible_v<T>
template <typename... Args>
inline T &PartPool<T>::override_unchecked(Thing thing, Args &&...args) noexcept {
    FR_ASSERT(thing.idx() < m_thing_to_part.size(), "thing index out of bounds");
    FR_ASSERT(m_thing_to_part[thing.idx()] != 0, "thing does not own this part");

    T &existing = m_parts[m_thing_to_part[thing.idx()]];
    existing = T(std::forward<Args>(args)...);
    return existing;
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline T &PartPool<T>::insert_unchecked(Thing thing, const T &part) noexcept {
    return emplace_unchecked(thing, part);
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline T &PartPool<T>::insert_unchecked(Thing thing, T &&part) noexcept {
    return emplace_unchecked(thing, std::move(part));
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline void PartPool<T>::destroy_unchecked(Thing thing) noexcept {
    if (thing.is_nil()) [[unlikely]] {
        return;
    }

    USize idx = thing.idx();

    FR_ASSERT(idx < m_thing_to_part.size(), "thing index out of bounds");
    FR_ASSERT(m_parts.size() > 0, "destroy on empty pool");

    USize rem_part_idx = m_thing_to_part[idx];
    USize swap_part_idx = m_parts.size() - 1;
    Thing swap_thing = m_part_to_thing[swap_part_idx];

    if (rem_part_idx != swap_part_idx) {
        m_parts[rem_part_idx] = std::move(m_parts[swap_part_idx]);
        m_thing_to_part[swap_thing.idx()] = rem_part_idx;
        m_part_to_thing[rem_part_idx] = swap_thing;
    }

    m_thing_to_part[idx] = 0;
    m_part_to_thing.pop_back();
    m_parts.pop_back();
}

template <typename T>
    requires std::is_default_constructible_v<T>
inline bool PartPool<T>::destroy_checked(Thing thing) noexcept {
    if (!get_checked(thing)) {
        return false;
    }
    destroy_unchecked(thing);
    return true;
}

// ----------------------------------------------------------- Static Assertions

FR_STATIC_ASSERT(sizeof(PartPool<Byte>) == sizeof(PartPool<U64>),
                 "part pools must have the same size regardless of the element type");

FR_STATIC_ASSERT(alignof(PartPool<Byte>) == alignof(PartPool<U64>),
                 "part pools must have the same alignment regardless of the element type");

} // namespace impl

} // namespace fr
