/**
 * @file part.hpp
 * @author Kiju
 *
 * @brief Part is an intergral part of the hidden ECS system. Normal people call it a component, but
 * it is too wordy for my taste. So part it is.
 * @details This file defines the Signature class, which is a bitset representation of parts
 * attached to a thing.
 */

#pragma once

#include "fr/core/bitset.hpp"
#include "fr/core/meta.hpp"

namespace fr {

// ==================================================================== Typedefs
constexpr USize MAX_PARTS = 128;

// =================================================================== Signature

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

    /**
     * @brief Returns the underlying bitset.
     */
    const Storage &bitset() const noexcept {
        return m_bits;
    }

    /**
     * @brief Destroy all parts - clear all bits.
     */
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

    /**
     * @brief Check whether any part is attached.
     */
    bool any() const noexcept {
        return m_bits.any();
    }

    /**
     * @brief Check whether none part is attached.
     */
    bool none() const noexcept {
        return m_bits.none();
    }

    /**
     * @brief Equality comparison for signatures.
     */
    friend bool operator==(const Signature &a, const Signature &b) noexcept {
        return a.m_bits == b.m_bits;
    }

    /**
     * @brief Inequality comparison for signatures.
     */
    friend bool operator!=(const Signature &a, const Signature &b) noexcept {
        return !(a == b);
    }

private:
    // -------------------------------------------------------- Member Variables
    Storage m_bits{};
};

// ================================================================ QueryOptions

/**
 * @brief Filtering options for a query.
 * @note Use Signature::from_parts<...>() to construct the fields.
 */
struct QueryOptions {
    Signature with{};
    Signature without{};
};

} // namespace fr
