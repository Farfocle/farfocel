/**
 * @file alloc_typedefs.hpp
 * @author Kiju
 * @brief Shared allocator system types.
 */
#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {

// --------------------------------------------------------- Out of Memory - OOM

/**
 * @brief Action requested by an out-of-memory handler.
 */
enum class OOMHandlerAction : U8 { Fail, Retry };

template <typename A>
void shape(A &a, OOMHandlerAction &value) {
    a.prop("@value", value == OOMHandlerAction::Fail ? "fail" : "retry");
}

/**
 * @brief Out-of-memory callback.
 *
 * @param sz Allocation size in bytes.
 * @param alignment Alignment in bytes.
 * @return Action to perform (Fail or Retry).
 */
using OOMHandler = OOMHandlerAction (*)(USize sz, USize alignment) noexcept;

/**
 * @brief Ownership inspection result for debug tooling.
 */
enum class OwnershipResult : U8 {
    /**
     * @brief Allocator owns this pointer.
     */
    Owns,

    /**
     * @brief Allocator does not own this pointer.
     */
    DoesNotOwn,

    /**
     * @brief Allocator cannot determine ownership of this pointer.
     * @note Often the case when dealing with general purpose heap allocators.
     */
    Unknown,
};

template <typename A>
void shape(A &a, OwnershipResult &value) {
    const char *str = "@unknown";

    if (value == OwnershipResult::Owns) {
        str = "owns";
    } else if (value == OwnershipResult::DoesNotOwn) {
        str = "does_not_own";
    }

    a.prop("@value", str);
}

// ----------------------------------------------------------------------- Debug

/**
 * @brief Recorded allocator action for debugging.
 */
enum class AllocAction : U8 {
    Allocate,
    Reallocate,
    Deallocate,
};

template <typename A>
void shape(A &a, AllocAction &value) {
    const char *str = "allocate";

    if (value == AllocAction::Reallocate) {
        str = "reallocate";
    } else if (value == AllocAction::Deallocate) {
        str = "deallocate";
    }

    a.prop("@value", str);
}

/**
 * @brief Recorded allocation frame for debugging.
 * @note Because `AllocFrame` stores raw pointers, serialization and deserialization results may
 * be invalid.
 */
struct AllocFrame {
    U64 timestamp{0};
    AllocAction action{AllocAction::Allocate};
    void *prev_pointer{nullptr};
    void *next_pointer{nullptr};
    USize prev_size{0};
    USize next_size{0};
    USize alignment{alignof(void *)};
    const char *tag{"@noname"};
    bool success{false};
    U8 attempt{0};

    template <typename A>
    void shape(A &archive) {
        archive.prop("timestamp", timestamp);
        archive.prop("action", action);

        U64 prev = reinterpret_cast<U64>(prev_pointer);
        U64 next = reinterpret_cast<U64>(next_pointer);

        archive.prop("prev_pointer", prev);
        archive.prop("next_pointer", next);

        archive.prop("prev_size", prev_size);
        archive.prop("next_size", next_size);
        archive.prop("alignment", alignment);
        archive.prop("tag", tag);
        archive.prop("success", success);
        archive.prop("attempt", attempt);
    }
};
} // namespace fr
