/**
 * @file typeidx.hpp
 * @author Kiju
 *
 * @brief TypeIdx is a mechanism for generating monotonic type indexes unique per translation unit.
 */

#pragma once

#include <atomic>

#include "fr/core/typedefs.hpp"

namespace fr {
using TypeIdx = U32;

/**
 * @brief Generates monotonic type indexes unique per translation unit. Indexes are U32.
 * @note Thread-safe.
 */
class TypeIdxGen {
public:
    /**
     * @brief Generates TypeIdx using the global generator.
     * @return Unique monotonic type idx.
     */
    template <typename T>
    static TypeIdx gen() noexcept {
        static const TypeIdx idx = s_counter.fetch_add(1, std::memory_order_relaxed);
        return idx;
    }

private:
    inline static std::atomic<TypeIdx> s_counter{0};
};
} // namespace fr
