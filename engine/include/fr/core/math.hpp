/**
 * @file math.hpp
 * @author Kiju
 *
 * @brief Utility helpers.
 */

#pragma once

#include <bit>

#include "fr/core/typedefs.hpp"

namespace fr::math {

/**
 * @brief Check whether @p n is a power of two.
 * @param n Value to test.
 * @return True if @p n is a power of two, false otherwise.
 */
inline bool is_pow2(USize n) noexcept {
    return n && ((n & (n - 1)) == 0);
}

/**
 * @brief Round @p n up to the next power of two.
 * @param n Value to round.
 * @return Smallest power of two greater than or equal to @p n.
 * @note Returns 1 when @p n is 0.
 */
inline USize round_up_pow2(USize n) noexcept {
    if (n == 0) {
        return 1;
    }

    if (is_pow2(n)) {
        return n;
    }

    return USize{1} << std::bit_width(n - 1);
}

/**
 * @brief Round @p n up to the next multiple of @p multiple.
 * @param n Value to round.
 * @param multiple Multiple to round up to.
 * @return Smallest multiple of @p multiple greater than or equal to @p n.
 */
inline USize round_up_to_multiple_of(USize n, USize multiple) noexcept {
    return (n + multiple - 1) & ~(multiple - 1);
}
} // namespace fr::math
