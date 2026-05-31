/**
 * @file part_pool.hpp
 * @author Kiju
 * @brief PartPool stores parts in a densely-packed array, indexed by thing.
 *
 * @details Layout uses three parallel arrays:
 *   - `m_parts`         : dense storage of T values; index 0 is a permanent stub for nil things.
 *   - `m_thing_to_part` : sparse map from thing index → part index in `m_parts`.
 *   - `m_part_to_thing` : dense map from part index → owning thing (mirrors `m_parts`).
 *
 * Slot 0 in all three arrays is permanently reserved as the stub / nil-thing entry.
 * A `m_thing_to_part[idx] == 0` means the thing at `idx` does not own this part.
 *
 * @par Unchecked vs. checked methods
 *   - `*_unchecked` : caller guarantees all preconditions; no bounds checking beyond debug asserts.
 *   - `*_checked`   : verifies bounds and part presence; returns nullptr / false on failure.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/slice.hpp"
#include "fr/data/thing.hpp"

namespace fr::impl {

/**
 * @brief Dense sparse-set storage for a single part type T.
 * @tparam T Part type. Must be default-constructible (for the stub).
 */
template <typename T>
    requires std::is_default_constructible_v<T>
class PartPool {
public:
    // ----------------------------------------------- Constructors & Destructor

    /**
     * @brief Constructs with the ambient allocator.
     */
    PartPool() noexcept;

    /**
     * @brief Constructs with the specified allocator.
     * @pre alloc must be non-null.
     */
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

    /**
     * @brief Slice of all parts including the stub (index 0).
     */
    Slice<T> parts_with_stub_mut() noexcept;

    /**
     * @brief Slice of parts excluding the stub.
     */
    Slice<T> parts_mut() noexcept;

    /**
     * @brief Sparse slice mapping thing index ->part index; includes stub entry.
     */
    Slice<USize> thing_to_part_with_stub_mut() noexcept;

    /**
     * @brief Sparse slice mapping thing index -> part index; excludes stub entry.
     */
    Slice<USize> thing_to_part_mut() noexcept;

    /**
     * @brief Dense slice mapping part index -> owning thing; includes stub entry.
     */
    Slice<Thing> part_to_thing_with_stub_mut() noexcept;

    /**
     * @brief Dense slice mapping part index -> owning thing; includes stub entry.
     */
    Slice<const Thing> part_to_thing_with_stub() const noexcept;

    /**
     * @brief Dense slice mapping part index → owning thing; excludes stub entry.
     */
    Slice<Thing> part_to_thing_mut() noexcept;

    // ------------------------------------------------------------ Part Getters

    /**
     * @brief Returns a reference to the stub (the nil-thing part).
     */
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

} // namespace fr::impl
