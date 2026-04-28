/**
 * @file alloc_types.hpp
 * @author Kiju
 *
 * @brief Common types for allocators.
 */
#pragma once

#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Action requested by an out-of-memory handler.
 */
enum class OOMHandlerAction : U8 { Fail, Retry };

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
     * @brief Allocator does NOT own this pointer.
     */
    DoesNotOwn,

    /**
     * @brief Allocator cannot determine ownership of this pointer.
     *
     * Often the case when dealing with general purpose heap allocators.
     */
    Unknown,
};

/**
 * @brief Recorded allocator action for debugging.
 */
enum class AllocAction : U8 {
    Allocate,
    Reallocate,
    Deallocate,
};

/**
 * @brief Recorded allocation frame for debugging.
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
};

} // namespace fr
