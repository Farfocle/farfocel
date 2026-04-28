/**
 * @file slice.hpp
 * @author Kiju
 * @brief Non-owning view over a contiguous range of elements.
 */
#pragma once

#include <algorithm>
#include <type_traits>

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Non-owning view over a contiguous range of elements.
 * @tparam T Element type.
 *
 * Slice does not manage lifetime. The caller must ensure the backing storage
 * remains valid for the Slice's lifetime.
 */
template <typename T>
class Slice {
private:
    T *m_data{nullptr};
    USize m_size{0};

public:
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;
    using size_type = USize;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;

    // ---------------------------------------------------------
    // Construction / Destruction
    // ---------------------------------------------------------

    Slice() = default;

    /**
     * @brief Create a view over @p size elements starting at @p data.
     * @param data Pointer to the first element.
     * @param size Number of elements in the view.
     * @pre If @p size > 0, @p data points to a valid contiguous range of @p size elements.
     */
    Slice(T *data, USize size) noexcept
        : m_data(data),
          m_size(size) {
    }

    /**
     * @brief Copy a slice (shallow copy).
     * @param other Source slice.
     */
    Slice(const Slice &other) noexcept {
        do_copy_from(other);
    }

    /**
     * @brief Move a slice (shallow move).
     * @param other Source slice.
     */
    Slice(Slice &&other) noexcept {
        do_move_from(std::move(other));
    }

    /**
     * @brief Copy-assign a slice (shallow copy).
     * @param other Source slice.
     * @return Reference to this slice.
     */
    Slice &operator=(const Slice &other) noexcept {
        do_copy_from(other);
        return *this;
    }

    /**
     * @brief Move-assign a slice (shallow move).
     * @param other Source slice.
     * @return Reference to this slice.
     */
    Slice &operator=(Slice &&other) noexcept {
        do_move_from(std::move(other));
        return *this;
    }

    /**
     * @brief Construct a slice from another slice with a compatible element type.
     * @tparam U Source element type.
     * @param other Source slice.
     * @note Allows implicit conversion (e.g., Slice<T> to Slice<const T>).
     */
    template <typename U>
        requires std::is_convertible_v<U *, T *>
    Slice(const Slice<U> &other) noexcept
        : m_data(other.data()),
          m_size(other.size()) {
    }

    // ---------------------------------------------------------
    // Iterators, Capacity, Operations
    // ---------------------------------------------------------

    /**
     * @brief Returns an iterator to the first element.
     * @return Pointer to the first element.
     */
    T *begin() noexcept {
        return m_data;
    }

    /**
     * @brief Returns an iterator to the element following the last element.
     * @return Pointer past the last element.
     */
    T *end() noexcept {
        return m_data + m_size;
    }

    /**
     * @brief Returns a constant iterator to the first element.
     * @return Constant pointer to the first element.
     */
    const T *begin() const noexcept {
        return m_data;
    }

    /**
     * @brief Returns a constant iterator to the element following the last element.
     * @return Constant pointer past the last element.
     */
    const T *end() const noexcept {
        return m_data + m_size;
    }

    /**
     * @brief Returns a constant iterator to the first element.
     * @return Constant pointer to the first element.
     */
    const T *cbegin() const noexcept {
        return m_data;
    }

    /**
     * @brief Returns a constant iterator to the element following the last element.
     * @return Constant pointer past the last element.
     */
    const T *cend() const noexcept {
        return m_data + m_size;
    }

    /**
     * @brief Access element at index without bounds checking.
     *
     * @param idx Index of the element.
     * @return Reference to the element.
     * @pre idx < size().
     */
    T &operator[](USize idx) const noexcept {
        FR_ASSERT(idx < m_size, "index out of bounds");
        return m_data[idx];
    }

    /**
     * @brief Number of elements in the slice.
     *
     * @return Current size.
     */
    USize size() const noexcept {
        return m_size;
    }

    /**
     * @brief Pointer to the underlying data.
     *
     * @return Raw pointer.
     */
    T *data() const noexcept {
        return m_data;
    }

    /**
     * @brief Check if the slice is empty.
     *
     * @return True if size is 0.
     */
    bool is_empty() const noexcept {
        return m_size == 0;
    }

    // ---------------------------------------------------------
    // Slicing
    // ---------------------------------------------------------

    /**
     * @brief Create a constant slice view over the entire range.
     *
     * @return Constant Slice.
     */
    Slice<const std::remove_const_t<T>> slice() const noexcept {
        return do_slice_self();
    }

    /**
     * @brief Create a mutable slice view over the entire range.
     *
     * @return Mutable Slice.
     * @note Only available if T is not const.
     */
    Slice<T> slice_mut() noexcept
        requires(!std::is_const_v<T>)
    {
        return do_slice_self();
    }

    /**
     * @brief Create a constant sub-slice view.
     *
     * @param from Start index (inclusive).
     * @param to End index (inclusive).
     * @return Constant sub-slice.
     * @pre from <= to < size().
     */
    Slice<const std::remove_const_t<T>> slice(USize from, USize to) const noexcept {
        return do_slice_bound(from, to);
    }

    /**
     * @brief Create a mutable sub-slice view.
     *
     * @param from Start index (inclusive).
     * @param to End index (inclusive).
     * @return Mutable sub-slice.
     */
    Slice<T> slice_mut(USize from, USize to) const noexcept
        requires(!std::is_const_v<T>)
    {
        return do_slice_bound(from, to);
    }

    /**
     * @brief Create a constant sub-slice starting from an index.
     *
     * @param from Start index.
     * @return Constant sub-slice.
     */
    Slice<const std::remove_const_t<T>> slice_from(USize from) const noexcept {
        return do_slice_from(from);
    }

    /**
     * @brief Create a mutable sub-slice starting from an index.
     *
     * @param from Start index.
     * @return Mutable sub-slice.
     */
    Slice<T> slice_mut_from(USize from) const noexcept
        requires(!std::is_const_v<T>)
    {
        return do_slice_from(from);
    }

    /**
     * @brief Create a constant sub-slice up to an index.
     *
     * @param to End index.
     * @return Constant sub-slice.
     */
    Slice<const std::remove_const_t<T>> slice_to(USize to) const noexcept {
        return do_slice_to(to);
    }

    /**
     * @brief Create a mutable sub-slice up to an index.
     *
     * @param to End index.
     * @return Mutable sub-slice.
     */
    Slice<T> slice_mut_to(USize to) const noexcept
        requires(!std::is_const_v<T>)
    {
        return do_slice_to(to);
    }

private:
    /**
     * @brief Internal helper to create a slice of the full range.
     *
     * @return Slice covering full range.
     */
    inline Slice do_slice_self() const noexcept {
        return Slice(m_data, m_size);
    }

    /**
     * @brief Internal helper to create a bounded sub-slice.
     *
     * @param from Start index.
     * @param to End index.
     * @return Bounded sub-slice.
     */
    inline Slice do_slice_bound(USize from, USize to) const noexcept {
        FR_ASSERT(from < m_size, "start index out of bounds");
        FR_ASSERT(to < m_size, "end index out of bounds");
        FR_ASSERT(from <= to, "invalid range");

        return Slice(m_data + from, to - from + 1);
    }

    /**
     * @brief Internal helper to create a sub-slice starting from from.
     *
     * @param from Start index.
     * @return Sub-slice from index.
     */
    inline Slice do_slice_from(USize from) const noexcept {
        FR_ASSERT(from < m_size || (from == 0 && m_size == 0), "index out of bounds");
        return Slice(m_data + from, m_size - from);
    }

    /**
     * @brief Internal helper to create a sub-slice ending at to.
     *
     * @param to End index.
     * @return Sub-slice up to index.
     */
    inline Slice do_slice_to(USize to) const noexcept {
        FR_ASSERT(to < m_size, "index out of bounds");
        return Slice(m_data, to + 1);
    }

    /// @brief Internal helper for shallow copy.
    inline void do_copy_from(const Slice &other) noexcept {
        m_data = other.m_data;
        m_size = other.m_size;
    }

    /// @brief Internal helper for shallow move.
    inline void do_move_from(Slice &&other) noexcept {
        m_data = other.m_data;
        m_size = other.m_size;
    }
};
} // namespace fr
