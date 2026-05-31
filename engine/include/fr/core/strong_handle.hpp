/**
 * @file strong_handle.hpp
 * @author Tfoedy
 * @brief Strongly-typed handle for resource management.
 * The template is used to differentiate between different types of resources at compile time.
 *
 *
 */

#pragma once

#include "fr/core/slot_map.hpp"

namespace fr {
template <typename Tag>
struct StrongHandle {
    SlotKey key{};

    [[nodiscard]] bool is_valid() const noexcept {
        return key.generation % 2 != 0;
    }

    bool operator==(const StrongHandle &other) const noexcept {
        return key == other.key;
    }
    bool operator!=(const StrongHandle &other) const noexcept {
        return key != other.key;
    }
};

} // namespace fr
