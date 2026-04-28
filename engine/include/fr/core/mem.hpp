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
 * @param alignment Requested alignment.
 * @return max(alignof(void*), alignment).
 * @pre @p alignment is a power of two.
 */
inline USize normalize_alignment(USize alignment) noexcept {
    FR_ASSERT(math::is_pow2(alignment),
              "fr::mem::normalize_alignment(USize alignment): Alignment must be a power of two");

    return std::max(alignof(void *), alignment);
}

/**
 * @brief Calculate the number of bytes needed to align a pointer.
 * @param ptr Pointer to align.
 * @param alignment Required alignment.
 * @return Number of padding bytes.
 * @pre @p alignment is a power of two.
 */
inline USize align_forward_padding(const void *ptr, USize alignment) noexcept {
    FR_ASSERT(is_valid_alignment(alignment), "Alignment must be a power of two");
    const auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr);
    const auto aligned = (ptr_val + alignment - 1) & ~(alignment - 1);
    return static_cast<USize>(aligned - ptr_val);
}

/**
 * @brief Align a pointer forward to a specific alignment.
 * @param ptr Pointer to align.
 * @param alignment Required alignment.
 * @return Aligned pointer.
 * @pre @p alignment is a power of two.
 */
inline void *align_forward(void *ptr, USize alignment) noexcept {
    FR_ASSERT(is_valid_alignment(alignment), "Alignment must be a power of two");
    const auto ptr_val = reinterpret_cast<std::uintptr_t>(ptr);
    const auto aligned = (ptr_val + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void *>(aligned);
}

/**
 * @brief Copy a trivially copyable range as raw bytes.
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre @p T is trivially copyable.
 * @pre Source and destination ranges do not overlap.
 * @pre If @p sz > 0, @p src and @p dst point to valid storage for @p sz elements.
 */
template <typename T>
inline void copy_raw_range(const T *src, USize sz, T *dst) noexcept {
    FR_STATIC_ASSERT(std::is_trivially_copyable_v<T>,
                     "T must be trivially copyable for copy_raw_range");
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr),
              "Source and destination pointers must be non-null for non-zero size");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "Source and destination ranges must not overlap");

    std::memcpy(static_cast<void *>(dst), static_cast<const void *>(src), sz * sizeof(T));
}

/**
 * @brief Set a trivially copyable range to a specific byte value.
 * @tparam T Element type.
 * @param ptr Range start.
 * @param value Byte value to set.
 * @param sz Number of elements.
 * @pre @p T is trivially copyable.
 * @pre If @p sz > 0, @p ptr points to valid storage for @p sz elements.
 */
template <typename T>
inline void set_raw_range(T *ptr, int value, USize sz) noexcept {
    FR_STATIC_ASSERT(std::is_trivially_copyable_v<T>,
                     "T must be trivially copyable for set_raw_range");
    FR_ASSERT(sz == 0 || ptr != nullptr, "Pointer must be non-null for non-zero size");

    std::memset(static_cast<void *>(ptr), value, sz * sizeof(T));
}

template <typename T>
inline void destroy_item(T *ptr) noexcept {
    FR_STATIC_ASSERT(std::is_nothrow_destructible_v<T>,
                     "T must be nothrow destructible for destroy_item");

    if (!ptr) {
        return;
    }

    if constexpr (!std::is_trivially_destructible_v<T>) {
        std::destroy_at(ptr);
    }
}

/**
 * @brief Destroy a range in reverse order.
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 * @pre @p T is nothrow destructible.
 * @pre If @p ptr is non-null and @p sz > 0, @p ptr points to @p sz constructed objects.
 */
template <typename T>
inline void destroy_range(T *ptr, USize sz) noexcept {
    FR_STATIC_ASSERT(std::is_nothrow_destructible_v<T>,
                     "T must be nothrow destructible for destroy_range");

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
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre @p src and @p dst are non-null.
 * @pre Source and destination ranges do not overlap.
 * @pre @p dst points to uninitialized storage for @p sz objects.
 * @pre If @p T is non-trivial, it is nothrow copy constructible.
 */
template <typename T>
inline void copy_init_range(const T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr),
              "Source and destination pointers must be non-null for non-zero size");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "Source and destination ranges must not overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_copy_constructible_v<T>,
                         "T must be nothrow copy constructible for copy_init_range");

        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, src[i]);
        }
    }
}

/**
 * @brief Copy-assign a range into initialized storage.
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre @p src and @p dst are non-null.
 * @pre Source and destination ranges do not overlap.
 * @pre @p dst points to @p sz constructed objects.
 * @pre If @p T is non-trivial, it is nothrow copy assignable.
 */
template <typename T>
inline void copy_assign_range(const T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr),
              "Source and destination pointers must be non-null for non-zero size");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "Source and destination ranges must not overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_copy_assignable_v<T>,
                         "T must be nothrow copy assignable for copy_assign_range");

        for (USize i = 0; i < sz; ++i) {
            dst[i] = src[i];
        }
    }
}

/**
 * @brief Move-construct a range into uninitialized storage.
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre @p src and @p dst are non-null.
 * @pre Source and destination ranges do not overlap.
 * @pre @p dst points to uninitialized storage for @p sz objects.
 * @pre If @p T is non-trivial, it is nothrow move constructible.
 */
template <typename T>
inline void move_init_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr),
              "Source and destination pointers must be non-null for non-zero size");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "Source and destination ranges must not overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_constructible_v<T>,
                         "T must be nothrow move constructible for move_init_range");

        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, std::move(src[i]));
        }
    }
}

/**
 * @brief Move-assign a range into initialized storage.
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre @p src and @p dst are non-null.
 * @pre Source and destination ranges do not overlap.
 * @pre @p dst points to @p sz constructed objects.
 * @pre If @p T is non-trivial, it is nothrow move assignable.
 */
template <typename T>
inline void move_assign_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(sz == 0 || (src != nullptr && dst != nullptr),
              "Source and destination pointers must be non-null for non-zero size");
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "Source and destination ranges must not overlap");

    if (sz == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        copy_raw_range(src, sz, dst);
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_move_assignable_v<T>,
                         "T must be nothrow move assignable for move_assign_range");

        for (USize i = 0; i < sz; ++i) {
            dst[i] = std::move(src[i]);
        }
    }
}

/**
 * @brief Default-initialize a range.
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 * @pre @p ptr is non-null.
 * @pre @p ptr points to uninitialized storage for @p sz objects.
 * @pre @p T is nothrow default constructible.
 * @note For trivially default constructible types this performs no initialization.
 */
template <typename T>
inline void default_init_range(T *ptr, USize sz) noexcept {
    FR_ASSERT(sz == 0 || ptr != nullptr, "Pointer must be non-null for non-zero size");

    FR_STATIC_ASSERT(std::is_nothrow_default_constructible_v<T>,
                     "T must be nothrow default constructible for default_init_range");
    if constexpr (!std::is_trivially_default_constructible_v<T>) {
        for (USize i = 0; i < sz; ++i)
            std::construct_at(ptr + i);
    }
}

/**
 * @brief Zero-initialize a range.
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 * @pre @p ptr is non-null.
 * @pre @p ptr points to uninitialized storage for @p sz objects.
 * @pre If @p T is non-trivial, it is nothrow default constructible.
 */
template <typename T>
inline void zero_init_range(T *ptr, USize sz) noexcept {
    FR_ASSERT(sz == 0 || ptr != nullptr, "Pointer must be non-null for non-zero size");

    if constexpr (std::is_trivially_default_constructible_v<T>) {
        std::memset(ptr, 0, sz * sizeof(T));
    } else {
        FR_STATIC_ASSERT(std::is_nothrow_default_constructible_v<T>,
                         "T must be nothrow default constructible for value_init_range");
        for (USize i = 0; i < sz; ++i)
            std::construct_at(ptr + i);
    }
}

/**
 * @brief Value-initialize a range.
 * @tparam T Element type.
 * @param ptr Range start.
 * @param sz Number of elements.
 * @param value Fill value.
 * @pre @p ptr is non-null.
 * @pre @p ptr points to uninitialized storage for @p sz objects.
 * @pre If @p T is non-trivial, it is nothrow default constructible.
 */
template <typename T>
inline void value_init_range(T *ptr, USize sz, const T &value) noexcept {
    FR_ASSERT(sz == 0 || ptr != nullptr, "Pointer must be non-null for non-zero size");

    FR_STATIC_ASSERT((std::is_nothrow_constructible_v<T, const T &>),
                     "T must be nothtow constructible with const T &");

    for (USize i = 0; i < sz; ++i) {
        std::construct_at(ptr + i, value);
    }
}

/**
 * @brief Move-construct into dst and destroy src.
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre Source and destination ranges do not overlap.
 * @pre @p src points to @p sz constructed objects.
 * @pre @p dst points to uninitialized storage for @p sz objects.
 * @pre If @p T is non-trivial, it is nothrow move constructible and nothrow destructible.
 * @note For trivially copyable types this performs a raw copy and does not destroy @p src.
 */
template <typename T>
inline void transfer_init_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "Source and destination ranges must not overlap");

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, src, sz * sizeof(T));
    } else {
        FR_STATIC_ASSERT(
            std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>,
            "T must be nothrow move constructible and destructible for transfer_init_range");
        for (USize i = 0; i < sz; ++i) {
            std::construct_at(dst + i, std::move(src[i]));
            std::destroy_at(src + i);
        }
    }
}

/**
 * @brief Move-assign into dst and destroy src.
 * @tparam T Element type.
 * @param src Source range.
 * @param sz Number of elements.
 * @param dst Destination range.
 * @pre Source and destination ranges do not overlap.
 * @pre @p src points to @p sz constructed objects.
 * @pre @p dst points to @p sz constructed objects.
 * @pre If @p T is non-trivial, it is nothrow move assignable and nothrow destructible.
 * @note For trivially copyable types this performs a raw copy and does not destroy @p src.
 */
template <typename T>
inline void transfer_assign_range(T *src, USize sz, T *dst) noexcept {
    FR_ASSERT(dst >= src + sz || src >= dst + sz, "Source and destination ranges must not overlap");

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, src, sz * sizeof(T));
    } else {
        FR_STATIC_ASSERT(
            std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>,
            "T must be nothrow move assignable and destructible for transfer_assign_range");
        for (USize i = 0; i < sz; ++i) {
            dst[i] = std::move(src[i]);
            std::destroy_at(src + i);
        }
    }
}

/**
 * @brief Shift a range right in-place.
 * @tparam T Element type.
 * @param ptr Range start.
 * @param count Number of elements to shift.
 * @param by Shift distance.
 * @pre @p ptr is non-null.
 * @pre The buffer has capacity for @p count + @p by elements.
 * @pre Elements in [0, @p count) are constructed.
 * @pre For non-trivial types, destination slots [@p by, @p by + @p count) are uninitialized.
 * @pre If @p T is non-trivial, it is nothrow move constructible and nothrow destructible.
 */
template <typename T>
inline void shift_range_right(T *ptr, USize count, USize by) noexcept {
    FR_ASSERT(ptr, "Pointer must be non-null");

    if (count == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memmove(ptr + by, ptr, count * sizeof(T));
    } else {
        FR_STATIC_ASSERT(
            std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>,
            "T must be nothrow move constructible and destructible for shift_range_right");
        for (USize i = count; i-- > 0;) {
            std::construct_at(ptr + by + i, std::move(ptr[i]));
            std::destroy_at(ptr + i);
        }
    }
}

/**
 * @brief Shift a range left in-place.
 * @tparam T Element type.
 * @param ptr Range start.
 * @param count Number of elements to shift.
 * @param by Shift distance.
 * @pre @p ptr is non-null.
 * @pre Elements in [@p by, @p by + @p count) are constructed.
 * @pre Destination slots [0, @p count) are uninitialized for non-trivial types.
 * @pre If @p T is non-trivial, it is nothrow move constructible and nothrow destructible.
 */
template <typename T>
inline void shift_range_left(T *ptr, USize count, USize by) noexcept {
    FR_ASSERT(ptr, "Pointer must be non-null");

    if (count == 0) {
        return;
    }

    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memmove(ptr, ptr + by, count * sizeof(T));
    } else {
        FR_STATIC_ASSERT(
            std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T>,
            "T must be nothrow move constructible and destructible for shift_range_left");
        for (USize i = 0; i < count; ++i) {
            std::construct_at(ptr + i, std::move(ptr[by + i]));
            std::destroy_at(ptr + by + i);
        }
    }
}

} // namespace fr::mem
