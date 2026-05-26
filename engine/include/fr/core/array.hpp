/**
 * @file array.hpp
 * @author Kiju
 *
 * @brief Fixed-size.
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "fr/core/hash.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Fixed-size array with contiguous storage on the stack.
 * @tparam T Element type.
 * @tparam Size Number of elements.
 *
 * @note Foundational requirements for T are enforced via FR_STATIC_ASSERT_NOTHROW_BASE.
 */
template <typename T, USize Size>
class Array {
    FR_STATIC_ASSERT_NOTHROW_BASE(T);

private:
    T m_data[Size > 0 ? Size : 1]{};

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

    // ----------------------------------- Constructors & Factories & Destructor

    /**
     * @brief Construct an array with zero-initialized elements.
     *
     * This constructor ensures that all elements are properly initialized.
     * For primitive types (int, float, etc.), they are guaranteed to be zero-filled.
     *
     * @pre T must be nothrow default constructible.
     */
    constexpr Array() noexcept {
        FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);

        if constexpr (Size > 0) {
            mem::zero_init_range(m_data, Size);
        }
    }

    /**
     * @brief Construct an array from an initializer list.
     *
     * If the list contains fewer elements than the array's size, the remaining
     * elements are zero-initialized.
     *
     * @param list Elements to copy.
     * @pre list.size() <= Size.
     * @pre T must be nothrow copy constructible.
     */
    constexpr Array(std::initializer_list<T> list) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        FR_ASSERT(list.size() <= Size, "initializer list too large");

        USize i = 0;
        for (const auto &item : list) {
            m_data[i++] = item;
        }

        if constexpr (Size > 0) {
            if (i < Size) {
                mem::zero_init_range(m_data + i, Size - i);
            }
        }
    }

    /**
     * @brief Create an array filled with a specific value.
     * @param value Value to copy into every element.
     * @return A new Array instance.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static constexpr Array from_repeated(const T &value) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        Array arr;

        for (USize i = 0; i < Size; ++i) {
            arr.m_data[i] = value;
        }

        return arr;
    }

    /**
     * @brief Create an array from a slice.
     * @param slice Source slice.
     * @return A new Array instance containing the slice elements.
     * @pre slice.size() == Size.
     * @pre T must be nothrow default constructible and copy assignable.
     */
    [[nodiscard]] static constexpr Array
    from_slice(Slice<const std::remove_const_t<T>> slice) noexcept {
        FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);
        FR_STATIC_ASSERT_NOTHROW_COPY_ASSIGNABLE(T);
        FR_ASSERT(slice.size() == Size, "slice size mismatch");

        Array arr;
        for (USize i = 0; i < Size; ++i) {
            arr.m_data[i] = slice[i];
        }

        return arr;
    }

    // --------------------------------------------------------------- Iterators

    /**
     * @brief Returns an iterator to the first element.
     * @return Pointer to the first element.
     */
    constexpr T *begin() noexcept {
        return m_data;
    }

    /**
     * @brief Returns an iterator to the element following the last element.
     * @return Pointer past the last element.
     */
    constexpr T *end() noexcept {
        return m_data + Size;
    }

    /**
     * @brief Returns a constant iterator to the first element.
     * @return Constant pointer to the first element.
     */
    constexpr const T *begin() const noexcept {
        return m_data;
    }

    /**
     * @brief Returns a constant iterator to the element following the last element.
     * @return Constant pointer past the last element.
     */
    constexpr const T *end() const noexcept {
        return m_data + Size;
    }

    /**
     * @brief Returns a constant iterator to the first element.
     * @return Constant pointer to the first element.
     */
    constexpr const T *cbegin() const noexcept {
        return m_data;
    }

    /**
     * @brief Returns a constant iterator to the element following the last element.
     * @return Constant pointer past the last element.
     */
    constexpr const T *cend() const noexcept {
        return m_data + Size;
    }

    // ---------------------------------------------------------- Element Access

    /**
     * @brief Access element at index with bounds checking in debug.
     * @param idx Index of the element to access.
     * @return Reference to the element at idx.
     * @pre idx < Size.
     */
    constexpr T &operator[](USize idx) noexcept {
        FR_ASSERT(idx < Size, "index out of bounds");
        return m_data[idx];
    }

    /**
     * @brief Access element at index with bounds checking in debug (const).
     * @param idx Index of the element to access.
     * @return Constant reference to the element at idx.
     * @pre idx < Size.
     */
    constexpr const T &operator[](USize idx) const noexcept {
        FR_ASSERT(idx < Size, "index out of bounds");
        return m_data[idx];
    }

    /**
     * @brief Access the first element.
     * @return Reference to the first element.
     * @pre Size > 0.
     */
    constexpr T &front() noexcept {
        FR_ASSERT(Size > 0, "empty array access");
        return m_data[0];
    }

    /**
     * @brief Access the first element (const).
     * @return Constant reference to the first element.
     * @pre Size > 0.
     */
    constexpr const T &front() const noexcept {
        FR_ASSERT(Size > 0, "empty array access");
        return m_data[0];
    }

    /**
     * @brief Access the last element.
     * @return Reference to the last element.
     * @pre Size > 0.
     */
    constexpr T &back() noexcept {
        FR_ASSERT(Size > 0, "empty array access");
        return m_data[Size - 1];
    }

    /**
     * @brief Access the last element (const).
     * @return Constant reference to the last element.
     * @pre Size > 0.
     */
    constexpr const T &back() const noexcept {
        FR_ASSERT(Size > 0, "empty array access");
        return m_data[Size - 1];
    }

    /**
     * @brief Direct access to the underlying storage.
     * @return Pointer to the beginning of the internal buffer.
     */
    constexpr T *data() noexcept {
        return m_data;
    }

    /**
     * @brief Direct access to the underlying storage (const).
     * @return Constant pointer to the beginning of the internal buffer.
     */
    constexpr const T *data() const noexcept {
        return m_data;
    }

    // ------------------------------------------------------------------ Slices

    /**
     * @brief Create a constant slice view over the entire array.
     * @return A Slice covering the array.
     */
    constexpr Slice<const T> slice() const & noexcept {
        return Slice<const T>(m_data, Size);
    }

    /**
     * @brief Create a mutable slice view over the entire array.
     * @return A mutable Slice covering the array.
     */
    constexpr Slice<T> slice_mut() & noexcept
        requires(!std::is_const_v<T>)
    {
        return Slice<T>(m_data, Size);
    }

    constexpr Slice<const T> slice() const && noexcept = delete;
    constexpr Slice<T> slice_mut() && noexcept = delete;

    /**
     * @brief Create a constant sub-slice view.
     * @param from Start index (inclusive).
     * @param to End index (inclusive).
     * @return A Slice covering the range [from, to].
     * @pre from <= to < Size.
     */
    constexpr Slice<const T> slice(USize from, USize to) const & noexcept {
        return slice().slice(from, to);
    }

    /**
     * @brief Create a mutable sub-slice view.
     * @param from Start index (inclusive).
     * @param to End index (inclusive).
     * @return A mutable Slice covering the range [from, to].
     * @pre from <= to < Size.
     */
    constexpr Slice<T> slice_mut(USize from, USize to) & noexcept
        requires(!std::is_const_v<T>)
    {
        return slice_mut().slice_mut(from, to);
    }

    constexpr Slice<const T> slice(USize, USize) const && = delete;
    constexpr Slice<T> slice_mut(USize, USize) && = delete;

    /**
     * @brief Create a constant slice starting from a specific index.
     * @param from Start index (inclusive).
     * @return A Slice covering [from, Size).
     * @pre from < Size or (from == 0 && Size == 0).
     */
    constexpr Slice<const T> slice_from(USize from) const & noexcept {
        return slice().slice_from(from);
    }

    /**
     * @brief Create a mutable slice starting from a specific index.
     * @param from Start index (inclusive).
     * @return A mutable Slice covering [from, Size).
     * @pre from < Size or (from == 0 && Size == 0).
     */
    constexpr Slice<T> slice_mut_from(USize from) & noexcept
        requires(!std::is_const_v<T>)
    {
        return slice_mut().slice_mut_from(from);
    }

    constexpr Slice<const T> slice_from(USize) const && = delete;
    constexpr Slice<T> slice_mut_from(USize) && = delete;

    /**
     * @brief Create a constant slice up to a specific index.
     * @param to End index (inclusive).
     * @return A Slice covering [0, to].
     * @pre to < Size.
     */
    constexpr Slice<const T> slice_to(USize to) const & noexcept {
        return slice().slice_to(to);
    }

    /**
     * @brief Create a mutable slice up to a specific index.
     * @param to End index (inclusive).
     * @return A mutable Slice covering [0, to].
     * @pre to < Size.
     */
    constexpr Slice<T> slice_mut_to(USize to) & noexcept
        requires(!std::is_const_v<T>)
    {
        return slice_mut().slice_mut_to(to);
    }

    constexpr Slice<const T> slice_to(USize) const && = delete;
    constexpr Slice<T> slice_mut_to(USize) && = delete;

    // ---------------------------------------------------------------  Capacity

    /**
     * @brief Get the number of elements in the array.
     * @return Size.
     */
    constexpr USize size() const noexcept {
        return Size;
    }

    /**
     * @brief Get the capacity of the array.
     * @return Size.
     */
    constexpr USize capacity() const noexcept {
        return Size;
    }

    /**
     * @brief Check if the array is empty.
     * @return True if Size is 0.
     */
    constexpr bool is_empty() const noexcept {
        return Size == 0;
    }

    /**
     * @brief Check if the array is full.
     * @return Always true for Array.
     */
    constexpr bool is_full() const noexcept {
        return true;
    }

    // --------------------------------------------------------------- Protocols

    /**
     * @brief Compute the hash of the array.
     * @return Hash value.
     */
    constexpr Hash hash() const noexcept {
        Hash h = Hash::from_raw(0);
        for (USize i = 0; i < Size; ++i) {
            h = combine_hashes(h, call_hash(m_data[i]));
        }
        return h;
    }

    /**
     * @brief Shape protocol for serialization.
     * @tparam A Archive type.
     * @param archive Archive to use.
     */
    template <typename Archive>
    void shape(Archive &archive) {
        if constexpr (Archive::action == ArchiveAction::Write) {
            USize sz = Size;
            archive.prop("@size", sz);
        } else {
            USize sz = 0;
            archive.prop("@size", sz);
            FR_ASSERT(sz == Size, "deserialized size mismatch for fixed-size Array");
        }

        archive.list("@items", [&](Archive &list_archive) {
            for (USize i = 0; i < Size; ++i) {
                list_archive.prop("", m_data[i]);
            }
        });
    }

    template <typename Archive>
    void shape(Archive &archive) const {
        if constexpr (Archive::action == ArchiveAction::Write) {
            USize sz = Size;
            archive.prop("@size", sz);

            archive.list("@items", [&](Archive &list_archive) {
                for (USize i = 0; i < Size; ++i) {
                    list_archive.prop("", m_data[i]);
                }
            });
        } else {
            FR_ASSERT(false, "cannot deserialize into const Array");
        }
    }

    // ----------------------------------------------------- Structured Bindings

    /**
     * @brief Access item at index I for structured bindings.
     * @tparam I Index to access.
     * @return Reference to the element.
     */
    template <USize I>
    constexpr auto &&at(this auto &&self) noexcept {
        FR_STATIC_ASSERT(I < Size, "index out of bounds");
        return std::forward_like<decltype(self)>(self.m_data[I]);
    }

    /**
     * @brief Structured binding support (friend).
     */
    template <USize I>
    friend constexpr auto &&get(Array &self) noexcept {
        return self.template at<I>();
    }

    template <USize I>
    friend constexpr auto &&get(const Array &self) noexcept {
        return self.template at<I>();
    }

    template <USize I>
    friend constexpr auto &&get(Array &&self) noexcept {
        return std::move(self).template at<I>();
    }

    template <USize I>
    friend constexpr auto &&get(const Array &&self) noexcept {
        return std::move(self).template at<I>();
    }
};

} // namespace fr

// --------------------------------------------------------- std Specializations

namespace std {
template <typename T, USize Size>
struct tuple_size<fr::Array<T, Size>> : std::integral_constant<USize, Size> {};

template <USize I, typename T, USize Size>
struct tuple_element<I, fr::Array<T, Size>> {
    using type = T;
};
} // namespace std
