/**
 * @file mem.hpp
 * @author Kiju
 * @brief Memory helper utilities for alignment and range operations.
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#include "fr/core/macros.hpp"
#include "fr/core/math.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::mem {

/**
 * @brief Check whether an alignment is a power of two.
 * @param alignment Alignment value to test.
 * @return True if @p alignment is a power of two.
 */
inline bool is_valid_alignment(USize alignment) noexcept {
    return math::is_pow2(alignment);
}

/**
 * @brief Check whether an alignment is at least alignof(std::max_align_t).
 * @param alignment Alignment value to test.
 * @return True if @p alignment is overaligned.
 */
inline bool is_overaligned(USize alignment) noexcept {
    return alignment >= sizeof(std::max_align_t);
}

/**
 * @brief Clamp alignment to at least alignof(void*).
 *
 * @param alignment Requested alignment.
 * @return max(alignof(void*), alignment).
 * @pre alignment is a power of two.
 */
inline USize normalize_alignment(USize alignment) noexcept {
    FR_ASSERT(math::is_pow2(alignment), "alignment must be power of two");

    return std::max(alignof(void *), alignment);
}

/**
 * @brief Calculate the number of bytes needed to align a pointer.
 *
 * @param ptr Pointer to align.
 * @param alignment Required alignment.
 * @return Number of padding bytes.
 * @pre alignment is a power of two.
 */
inline USize align_forward_padding(const void *ptr, USize alignment) noexcept {
    FR_ASSERT(is_valid_alignment(alignment), "alignment must be power of two");
    const auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr);
    const auto aligned = (ptr_val + alignment - 1) & ~(alignment - 1);
    return static_cast<USize>(aligned - ptr_val);
}

/**
 * @brief Align a pointer forward to a specific alignment.
 *
 * @param ptr Pointer to align.
 * @param alignment Required alignment.
 * @return Aligned pointer.
 * @pre alignment is a power of two.
 */
inline void *align_forward(void *ptr, USize alignment) noexcept {
    FR_ASSERT(is_valid_alignment(alignment), "alignment must be power of two");
    const auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr);
    const auto aligned = (ptr_val + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void *>(aligned);
}

/**
 * @brief Copy a trivially copyable range as raw bytes.
 *
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre T is trivially copyable.
 * @pre Source and destination ranges do not overlap.
 * @pre If sz > 0, src and dst point to valid storage.
 */
template <typename T>
inline void copy_raw_range(const T *src, USize sz, T *dst) noexcept {
    FR_STATIC_ASSERT(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr), "pointers must be non-null");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "range overlap");

    std::memcpy(static_cast<void *>(dst), static_cast<const void *>(src), sz * sizeof(T));
}

/**
 * @brief Set a trivially copyable range to a specific byte value.
 *
 * @tparam T Element type.
 * @param ptr Range start.
 * @param value Byte value to set.
 * @param sz Number of elements.
 * @pre T is trivially copyable.
 * @pre If sz > 0, ptr points to valid storage.
 */
template <typename T>
inline void set_raw_range(T *ptr, int value, USize sz) noexcept {
    FR_STATIC_ASSERT(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    FR_ASSERT(sz == 0 || ptr != nullptr, "pointer must be non-null");

    std::memset(static_cast<void *>(ptr), value, sz * sizeof(T));
}

/**
 * @brief Destroy a single item.
 *
 * @tparam T Element type.
 * @param ptr Pointer to the item.
 */
template <typename T>
inline void destroy_item(T *ptr) noexcept {
    FR_STATIC_ASSERT(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

    if (!ptr) {
        return;
    }

    if constexpr (!std::is_trivially_destructible_v<T>) {
        std::destroy_at(ptr);
    }
}

/**
 * @brief Destroy a range in reverse order.
 *
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 * @pre T is nothrow destructible.
 */
template <typename T>
inline void destroy_range(T *ptr, USize sz) noexcept {
    FR_STATIC_ASSERT(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");

    if (!ptr || sz == 0) {
        return;
    }

    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (USize i = sz; i-- > 0;) {
            std::destroy_at(ptr + i);
        }
    }
}

/**
 * @brief Copy-construct a range into uninitialized storage.
 *
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre pointers are non-null for non-zero size.
 * @pre ranges do not overlap.
 */
template <typename T>
inline void copy_init_range(const T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr), "pointers must be non-null");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "range overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_copy_constructible_v<T>, "T must be nothrow copyable");

        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, src[i]);
        }
    }
}

/**
 * @brief Copy-assign a range into initialized storage.
 *
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 */
template <typename T>
inline void copy_assign_range(const T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr), "pointers must be non-null");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "range overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_copy_assignable_v<T>, "T must be nothrow copyable");

        for (USize i = 0; i < sz; ++i) {
            dst[i] = src[i];
        }
    }
}

/**
 * @brief Move-construct a range into uninitialized storage.
 *
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 */
template <typename T>
inline void move_init_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr), "pointers must be non-null");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "range overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_constructible_v<T>, "T must be nothrow movable");

        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, std::move(src[i]));
        }
    }
}

/**
 * @brief Move-assign a range into initialized storage.
 *
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 */
template <typename T>
inline void move_assign_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr), "pointers must be non-null");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "range overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_assignable_v<T>, "T must be nothrow movable");

        for (USize i = 0; i < sz; ++i) {
            dst[i] = std::move(src[i]);
        }
    }
}

/**
 * @brief Default-initialize a range.
 *
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 */
template <typename T>
inline void default_init_range(T *ptr, USize sz) noexcept {
    FR_ASSERT(sz == 0 || ptr != nullptr, "pointer must be non-null");

    FR_STATIC_ASSERT(std::is_nothrow_default_constructible_v<T>, "T must be nothrow constructible");
    if constexpr (!std::is_trivially_default_constructible_v<T>) {
        for (USize i = 0; i < sz; ++i)
            std::construct_at(ptr + i);
    }
}

/**
 * @brief Zero-initialize a range.
 *
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 */
template <typename T>
inline void zero_init_range(T *ptr, USize sz) noexcept {
    FR_ASSERT(sz == 0 || ptr != nullptr, "pointer must be non-null");

    if constexpr (std::is_trivially_default_constructible_v<T>) {
        std::memset(ptr, 0, sz * sizeof(T));
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_default_constructible_v<T>,
                         "T must be nothrow constructible");
        for (USize i = 0; i < sz; ++i)
            std::construct_at(ptr + i);
    }
}

/**
 * @brief Value-initialize a range.
 *
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 * @param value Fill value.
 */
template <typename T>
inline void value_init_range(T *ptr, USize sz, const T &value) noexcept {
    FR_ASSERT(sz == 0 || ptr != nullptr, "pointer must be non-null");

    FR_STATIC_ASSERT((std::is_nothrow_constructible_v<T, const T &>),
                     "T must be nothrow constructible");

    for (USize i = 0; i < sz; ++i) {
        std::construct_at(ptr + i, value);
    }
}

/**
 * @brief Move-construct into dst and destroy src.
 *
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 */
template <typename T>
inline void transfer_init_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "range overlap");

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, src, sz * sizeof(T));
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_constructible_v<T> &&
                             std::is_nothrow_destructible_v<T>,
                         "T must be nothrow movable and destructible");
        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, std::move(src[i]));
            std::destroy_at(src + i);
        }
    }
}

/**
 * @brief Move-assign into dst and destroy src.
 *
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 */
template <typename T>
inline void transfer_assign_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "range overlap");

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, src, sz * sizeof(T));
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>,
                         "T must be nothrow movable and destructible");
        for (USize i = 0; i < sz; ++i) {
            dst[i] = std::move(src[i]);
            std::destroy_at(src + i);
        }
    }
}

/**
 * @brief Shift a range right in-place.
 *
 * @tparam T Element type.
 * @param ptr Range start.
 * @param count Number of elements to shift.
 * @param by Shift distance.
 */
template <typename T>
inline void shift_range_right(T *ptr, USize count, USize by) noexcept {
    FR_ASSERT(ptr, "pointer must be non-null");

    if (count == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memmove(ptr + by, ptr, count * sizeof(T));
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_constructible_v<T> &&
                             std::is_nothrow_destructible_v<T>,
                         "T must be nothrow movable and destructible");
        for (USize i = count; i-- > 0;) {
            std::construct_at(ptr + by + i, std::move(ptr[i]));
            std::destroy_at(ptr + i);
        }
    }
}

/**
 * @brief Shift a range left in-place.
 *
 * @tparam T Element type.
 * @param ptr Range start.
 * @param count Number of elements to shift.
 * @param by Shift distance.
 */
template <typename T>
inline void shift_range_left(T *ptr, USize count, USize by) noexcept {
    FR_ASSERT(ptr, "pointer must be non-null");

    if (count == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memmove(ptr, ptr + by, count * sizeof(T));
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_constructible_v<T> &&
                             std::is_nothrow_destructible_v<T>,
                         "T must be nothrow movable and destructible");
        for (USize i = 0; i < count; ++i) {
            std::construct_at(ptr + i, std::move(ptr[by + i]));
            std::destroy_at(ptr + by + i);
        }
    }
}

} // namespace fr::mem
