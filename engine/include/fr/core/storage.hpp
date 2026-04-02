/**
 *
 * @file storage.hpp
 * @author Kiju
 * @brief Dumb storage descriptor for allocator-backed buffers.
 */
#pragma once

#include "fr/core/allocator.hpp"
#include "fr/core/globals.hpp"

namespace fr::mem {

/**
 * @brief Dumb storage descriptor for allocator-backed buffers.
 *
 * Holds allocator pointer and raw buffer metadata without owning or managing
 * allocation or lifetime.
 *
 * Invariants:
 * - size <= capacity
 * - if data is nullptr then size == 0 and capacity == 0
 *
 * @tparam T Element type.
 */
template <typename T>
struct Storage {
    /**
     * @brief Allocator used for (de)allocation of @p data.
     * @note This pointer is used whenever storage needs to grow or be freed.
     */
    Allocator *allocator{globals::get_default_allocator()};

    /**
     * @brief Pointer to the raw memory buffer.
     * @note May be nullptr if the storage is empty.
     */
    T *data{nullptr};

    /**
     * @brief Number of currently constructed elements in @p data.
     * @note Represents the logical size of the container.
     */
    USize size{0};

    /**
     * @brief Total number of elements that the buffer can hold.
     * @note Represents the allocated capacity of the buffer.
     */
    USize capacity{0};

    /**
     * @brief Return an empty storage using the default allocator.
     * @return Empty storage descriptor.
     */
    inline static Storage empty() noexcept {
        return Storage{};
    }

    /**
     * @brief Return an empty storage using @p allocator.
     * @param allocator Allocator to associate with the storage.
     * @return Empty storage descriptor bound to @p allocator.
     * @note This does not allocate memory.
     */
    inline static Storage with_allocator(Allocator *allocator) {
        return {
            .allocator = allocator,
            .data = nullptr,
            .size = 0,
            .capacity = 0,
        };
    }
};
} // namespace fr::mem
