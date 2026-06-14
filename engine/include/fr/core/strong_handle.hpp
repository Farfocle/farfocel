/**
 * @file strong_handle.hpp
 * @author Tfoedy
 * @brief Strongly typed generational handles.
 */

#pragma once

#include "fr/core/slot_map.hpp"

namespace fr {

template <typename Tag>
struct StrongHandle {
    SlotKey key{};

    /**
     * @brief Returns true if the handle looks non-null.
     *
     * @details This only checks the handle generation parity. It does not prove that the referenced
     * resource is still alive inside its owning SlotMap. Owners must validate handles through their
     * storage before dereferencing.
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return key.is_valid_generation();
    }

    [[nodiscard]] bool operator==(const StrongHandle &other) const noexcept {
        return key == other.key;
    }

    [[nodiscard]] bool operator!=(const StrongHandle &other) const noexcept {
        return key != other.key;
    }
};

} // namespace fr
