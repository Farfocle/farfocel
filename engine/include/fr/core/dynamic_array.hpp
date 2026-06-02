/**
 * @file dynamic_array.hpp
 * @author Kiju
 * @brief Dynamic array.
 *
 * @details
 * This implementation differs signaficantly from `std::vector` in number of ways:
 * - Fully noexcept
 * - Uses `fr::Alloc` polymorphic allocator
 * - `resize()` is split into mutliple methods for more explicit API
 * - Default growth factor is 150% (`std::vector` differs on that depending on implementation)
 * - Provides static factory methods for easy of contructions (avoid heavily overloading the
 * constructor like in `std::vector`)
 * - Provides a safe, integrated slice API (`fr::Slice`, akin to `std::span`)
 * - Removing elements is done with a more explicit API (`remove_swap` and `remove_shift`)
 *
 * Although this implementation is quite unique, it was inspired by facebook folly's `fbvector`.
 * https://github.com/facebook/folly/blob/main/folly/container/FBVector.h
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/slice.hpp"

namespace fr {

/**
 * @brief Dynamic Array with a growth factor of 150%.
 *
 * @tparam T Element type.
 * @pre `T` must be nothrow destructible.
 * @pre `T` must be nothrow move constructible.
 * @pre `T` must be nothrow move assignable.
 */
template <typename T>
class DynamicArray {
private:
    // Ensuring the basic preconditions of `T`
    FR_STATIC_ASSERT_NOTHROW_BASE(T);

    // -------------------------------------------------------- Member Variables
    Alloc *m_alloc{get_ambient_ctx().alloc};
    T *m_data{nullptr};
    USize m_size{0};
    USize m_capacity{0};

public:
    // ---------------------------------------------------- Constants & Typedefs
    static constexpr USize GROWTH_FACTOR = 150;
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
     * @brief Constructs an empty `DynamicArray`.
     * @note Does not allocate.
     */
    DynamicArray() noexcept = default;

    /**
     * @brief Constructs a `DynamicArray` from an initializer list.
     * @param list Initializer list of elements to copy.
     *
     * @note Allocates at least the size of `list`.
     * @pre `T` must be nothrow copy constructible.
     */
    DynamicArray(std::initializer_list<T> list) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        do_grow_if_needed(list.size());

        for (const T &item : list) {
            emplace_back(item);
        }
    }

    /**
     * @brief Copy-construct a new `DynamicArray` from an existing one - deep copy.
     * @param other The `DynamicArray` to copy from.
     *
     * @note Allocator does not propagate.
     * @pre `T` must be nothrow copy constructible.
     */
    DynamicArray(const DynamicArray &other) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        do_copy_from(other);
    }

    /**
     * @brief Move-construct a new `DynamicArray`, stealing storage from other.
     * @param other The `DynamicArray` to move from.
     *
     * @note Allocator propagates.
     */
    DynamicArray(DynamicArray &&other) noexcept {
        do_move_from(std::move(other));
    }

    /**
     * @brief Construct an empty `DynamicArray` with the given allocator.
     * @param alloc Pointer to the allocator.
     *
     * @note Does not allocate.
     * @pre `alloc` must be non-null.
     */
    explicit DynamicArray(Alloc *alloc) noexcept
        : m_alloc(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");
    }

    /**
     * @brief Destroy all elements and free the underlying storage.
     */
    ~DynamicArray() noexcept {
        do_destroy_all();
        do_free_storage();
    }

    /**
     * @brief Create an empty `DynamicArray` using the given allocator.
     * @param alloc Pointer to the allocator.
     *
     * @return A new empty `DynamicArray`.
     * @pre `alloc` must be non-null.
     */
    [[nodiscard]] static DynamicArray with_alloc(Alloc *alloc) noexcept {
        return DynamicArray(alloc);
    }

    /**
     * @brief Create an empty `DynamicArray` with an initial reserved capacity using the given
     * allocator.
     *
     * @param alloc Pointer to the allocator.
     * @param capacity The number of elements to reserve.
     * @pre `capacity` must be non-zero.
     * @return A new empty `DynamicArray`.
     *
     * @note Allocates.
     * @pre `alloc` must be non-null.
     */
    [[nodiscard]] static DynamicArray with_capacity(Alloc *alloc, USize capacity) noexcept {
        DynamicArray darr(alloc);
        darr.do_reserve(capacity);

        return darr;
    }

    /**
     * @brief Create an empty `DynamicArray` with an initial reserved capacity using the ambient
     * allocator.
     *
     * @param capacity The number of elements to reserve.
     * @pre `capacity` must be non-zero.
     * @return A new empty `DynamicArray`.
     *
     * @note Allocates.
     */
    [[nodiscard]] static DynamicArray with_capacity(USize capacity) noexcept {
        return with_capacity(get_ambient_ctx().alloc, capacity);
    }

    /**
     * @brief Create an array of a specific size using a specific allocator.
     *
     * @param alloc Pointer to the allocator to use.
     * @pre `alloc` must be non-null.
     * @param size Initial number of elements.
     * @return A new array of size `size`.
     *
     * @pre `T` must be nothrow default constructible.
     */
    [[nodiscard]] static DynamicArray with_size(Alloc *alloc, USize size) noexcept {
        FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);
        DynamicArray arr(alloc);
        arr.do_reserve(size);

        mem::default_init_range(arr.m_data, size);
        arr.m_size = size;

        return arr;
    }

    /**
     * @brief Create an array of a specific size with default-initialized elements.
     *
     * @param size Initial number of elements.
     * @return A new DynamicArray instance of the requested size.
     * @pre T must be nothrow default constructible.
     */
    [[nodiscard]] static DynamicArray with_size(USize size) noexcept {
        return with_size(get_ambient_ctx().alloc, size);
    }

    /**
     * @brief Create an array filled with a specific value using a specific allocator.
     *
     * @param size Number of elements.
     * @param value Value to copy into every element.
     * @return A new DynamicArray instance filled with fill.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static DynamicArray from_repeated(USize size, const T &value) noexcept {
        return from_repeated(get_ambient_ctx().alloc, size, value);
    }

    /**
     * @brief Create an array filled with a specific value using a specific allocator.
     *
     * @param alloc Pointer to the allocator to use.
     * @param size Number of elements.
     * @param value Value to copy into every element.
     * @return A new DynamicArray instance filled with fill.
     * @pre alloc must be non-null.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static DynamicArray from_repeated(Alloc *alloc, USize size,
                                                    const T &value) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);

        DynamicArray arr(alloc);
        arr.do_reserve(size);

        for (USize i = 0; i < size; ++i) {
            std::construct_at(arr.m_data + i, value);
        }

        arr.m_size = size;

        return arr;
    }

    /**
     * @brief Create an array from a slice using the ambient allocator.
     *
     * @param slice Source slice.
     * @return A new DynamicArray instance containing the slice elements.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static DynamicArray
    from_slice(Slice<const std::remove_const_t<T>> slice) noexcept {
        return from_slice(get_ambient_ctx().alloc, slice);
    }

    /**
     * @brief Create an array from a slice using a specific allocator.
     *
     * @param alloc Pointer to the allocator to use.
     * @param slice Source slice.
     * @return A new DynamicArray instance containing the slice elements.
     * @pre alloc must be non-null.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static DynamicArray
    from_slice(Alloc *alloc, Slice<const std::remove_const_t<T>> slice) noexcept {
        FR_ASSERT(alloc, "allocator must be non-null");
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);

        DynamicArray arr(alloc);
        USize size = slice.size();
        if (size == 0) {
            return arr;
        }

        arr.do_reserve(size);

        for (USize i = 0; i < size; ++i) {
            std::construct_at(arr.m_data + i, slice[i]);
        }

        arr.m_size = size;

        return arr;
    }
    // --------------------------------------------------- Move & Copy Operators

    /**
     * @brief Copy-assign elements from other.
     *
     * @param other The array to copy from.
     * @return Reference to this array.
     * @note Performs a deep copy. If `other` has more elements than this array's capacity,
     *       new storage will be allocated. The allocator does not propagate.
     * @pre `T` is nothrow copy constructible and nothrow copy assignable.
     */
    DynamicArray &operator=(const DynamicArray &other) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        FR_STATIC_ASSERT_NOTHROW_COPY_ASSIGNABLE(T);

        if (this == &other) {
            return *this;
        }

        do_assign_copy(other);
        return *this;
    }

    /**
     * @brief Move-assign storage from other.
     *
     * @param other The array to move from.
     * @return Reference to this array.
     * @note Fast path: if allocators match, steals memory. Slow path: element-wise move.
     */
    DynamicArray &operator=(DynamicArray &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (m_alloc == other.m_alloc) {
            do_destroy_all();
            do_free_storage();

            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;

            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        } else {
            clear();

            do_reserve(other.m_size);
            mem::transfer_init_range(other.m_data, other.m_size, m_data);

            m_size = other.m_size;
            other.m_size = 0;
        }

        return *this;
    }

    // --------------------------------------------------------------- Iterators

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

    // ---------------------------------------------------------- Element Access

    /**
     * @brief Access element at index without bounds checking.
     *
     * @param idx Index of the element to access.
     * @return Reference to the element at idx.
     * @pre `idx < size()`.
     */
    T &operator[](USize idx) noexcept {
        FR_ASSERT(idx < m_size, "index out of bounds");
        return m_data[idx];
    }

    /**
     * @brief Access element at index without bounds checking (const).
     *
     * @param idx Index of the element to access.
     * @return Constant reference to the element at idx.
     * @pre `idx < size()`.
     */
    const T &operator[](USize idx) const noexcept {
        FR_ASSERT(idx < m_size, "index out of bounds");
        return m_data[idx];
    }

    /**
     * @brief Access the first element.
     *
     * @return Reference to the first element.
     * @pre !is_empty().
     */
    T &front() noexcept {
        FR_ASSERT(!is_empty(), "empty array access");
        return m_data[0];
    }

    /**
     * @brief Access the first element (const).
     *
     * @return Constant reference to the first element.
     * @pre `!is_empty()`.
     */
    const T &front() const noexcept {
        FR_ASSERT(!is_empty(), "empty array access");
        return m_data[0];
    }

    /**
     * @brief Access the last element.
     *
     * @return Reference to the last element.
     * @pre `!is_empty()`.
     */
    T &back() noexcept {
        FR_ASSERT(!is_empty(), "empty array access");
        return m_data[m_size - 1];
    }

    /**
     * @brief Access the last element (const).
     *
     * @return Constant reference to the last element.
     * @pre `!is_empty()`.
     */
    const T &back() const noexcept {
        FR_ASSERT(!is_empty(), "empty array access");
        return m_data[m_size - 1];
    }

    /**
     * @brief Direct access to the underlying storage.
     * @return Pointer to the beginning of the internal buffer.
     */
    T *data() noexcept {
        return m_data;
    }

    /**
     * @brief Direct access to the underlying storage (const).
     * @return Constant pointer to the beginning of the internal buffer.
     */
    const T *data() const noexcept {
        return m_data;
    }

    // ------------------------------------------------------- Slice Operationas

    /**
     * @brief Create a constant slice view over the entire array.
     * @return A Slice covering [0, size()).
     */
    Slice<const T> slice() const & noexcept {
        return Slice<const T>(m_data, m_size);
    }

    /**
     * @brief Create a mutable slice view over the entire array.
     *
     * @return A mutable Slice covering [0, size()).
     * @note Only available if T is not const.
     */
    Slice<T> slice_mut() & noexcept
        requires(!std::is_const_v<T>)
    {
        return Slice<T>(m_data, m_size);
    }

    Slice<const T> slice() const && noexcept = delete;
    Slice<T> slice_mut() && noexcept = delete;

    /**
     * @brief Create a constant sub-slice view.
     *
     * @param from Start index (inclusive).
     * @param to End index (inclusive).
     * @return A Slice covering the range [from, to].
     * @pre `from <= to && to < size()`.
     */
    Slice<const T> slice(USize from, USize to) const & noexcept {
        return slice().slice(from, to);
    }

    /**
     * @brief Create a mutable sub-slice view.
     *
     * @param from Start index (inclusive).
     * @param to End index (inclusive).
     * @return A mutable Slice covering the range [from, to].
     * @pre `from <= to && to < size()`.
     * @note Only available if T is not const.
     */
    Slice<T> slice_mut(USize from, USize to) & noexcept
        requires(!std::is_const_v<T>)
    {
        return slice_mut().slice_mut(from, to);
    }

    Slice<const T> slice(USize, USize) const && = delete;
    Slice<T> slice_mut(USize, USize) && = delete;

    /**
     * @brief Create a constant slice starting from a specific index.
     *
     * @param from Start index (inclusive).
     * @return A Slice covering [from, size()).
     * @pre `from < size() || (from == 0 && size() == 0)`.
     */
    Slice<const T> slice_from(USize from) const & noexcept {
        return slice().slice_from(from);
    }

    /**
     * @brief Create a mutable slice starting from a specific index.
     *
     * @param from Start index (inclusive).
     * @return A mutable Slice covering [from, size()).
     * @pre `from < size() || (from == 0 && size() == 0)`.
     * @note Only available if T is not const.
     */
    Slice<T> slice_mut_from(USize from) & noexcept
        requires(!std::is_const_v<T>)
    {
        return slice_mut().slice_mut_from(from);
    }

    Slice<const T> slice_from(USize) const && = delete;
    Slice<T> slice_mut_from(USize) && = delete;

    /**
     * @brief Create a constant slice up to a specific index.
     *
     * @param to End index (inclusive).
     * @return A Slice covering [0, to].
     * @pre `to < size()`.
     */
    Slice<const T> slice_to(USize to) const & noexcept {
        return slice().slice_to(to);
    }

    /**
     * @brief Create a mutable slice up to a specific index.
     *
     * @param to End index (inclusive).
     * @return A mutable Slice covering [0, to].
     * @pre `to < size()`.
     * @note Only available if T is not const.
     */
    Slice<T> slice_mut_to(USize to) & noexcept
        requires(!std::is_const_v<T>)
    {
        return slice_mut().slice_mut_to(to);
    }

    Slice<const T> slice_to(USize) const && = delete;
    Slice<T> slice_mut_to(USize) && = delete;

    // ---------------------------------------------------------------- Capacity

    /**
     * @brief Get the current number of elements in the array.
     * @return Current size.
     */
    USize size() const noexcept {
        return m_size;
    }

    /**
     * @brief Get the total number of elements that can be held without reallocation.
     * @return Current capacity.
     */
    USize capacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Check if the array contains no elements.
     * @return True if size is 0.
     */
    bool is_empty() const noexcept {
        return m_size == 0;
    }

    /**
     * @brief Check if the array size has reached its capacity.
     * @return True if size equals capacity.
     */
    bool is_full() const noexcept {
        return m_size == m_capacity;
    }

    /**
     * @brief Get the allocator currently associated with this array.
     * @return Pointer to the allocator.
     */
    const Alloc *alloc() const noexcept {
        return m_alloc;
    }

    /**
     * @brief Ensure the array has space for at least new_capacity elements.
     * @param new_capacity The minimum desired capacity.
     * @note If new_capacity is less than or equal to current capacity, no action is taken.
     */
    void reserve(USize new_capacity) noexcept {
        if (new_capacity > m_capacity) [[likely]] {
            do_reserve(new_capacity);
        }
    }

    /**
     * @brief Reduce capacity to match current size.
     * @note If size is 0, storage may be freed completely.
     */
    void shrink_to_fit() noexcept {
        if (m_size == m_capacity) {
            return;
        }

        if (m_size == 0) {
            do_free_storage();
            return;
        }

        do_reallocate(m_size);
    }

    // --------------------------------------------------------------- Modifiers

    /**
     * @brief Append a copy of value to the end of the array.
     *
     * @param value The value to copy.
     * @pre T must be nothrow copy constructible.
     * @note May trigger a reallocation if the array is full.
     */
    void push_back(const T &value) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        emplace_back(value);
    }

    /**
     * @brief Append value to the end of the array by moving it.
     *
     * @param value The value to move.
     * @pre T must be nothrow move constructible.
     * @note May trigger a reallocation if the array is full.
     */
    void push_back(T &&value) noexcept {
        emplace_back(std::move(value));
    }

    /**
     * @brief Construct an element in-place at the end of the array.
     *
     * @tparam Args Argument types for T's constructor.
     * @param args Arguments to pass to T's constructor.
     * @return Reference to the newly created element.
     * @pre T must be nothrow constructible from Args.
     * @note May trigger a reallocation if the array is full.
     */
    template <typename... Args>
    T &emplace_back(Args &&...args) noexcept {
        static_assert(std::is_nothrow_constructible_v<T, Args...>,
                      "T must be nothrow constructible from Args");

        do_grow_if_full();
        T *ptr = std::construct_at(m_data + m_size, std::forward<Args>(args)...);
        ++m_size;

        return *ptr;
    }

    /**
     * @brief Remove the last element from the array.
     *
     * @pre !is_empty().
     * @note The removed element is destroyed.
     */
    void pop_back() noexcept {
        FR_ASSERT(!is_empty(), "empty array pop");

        --m_size;
        std::destroy_at(m_data + m_size);
    }

    /**
     * @brief Remove an element at idx by swapping it with the last element.
     *
     * @param idx Index of the element to remove.
     * @pre idx < size().
     * @note This is an O(1) operation but does NOT preserve the relative order of elements.
     */
    void remove_swap(USize idx) noexcept {
        FR_ASSERT(idx < m_size, "index out of bounds");

        if (idx != m_size - 1) {
            m_data[idx] = std::move(m_data[m_size - 1]);
        }

        pop_back();
    }

    /**
     * @brief Remove an element at idx while preserving the order of remaining elements.
     *
     * @param idx Index of the element to remove.
     * @pre idx < size().
     * @note This is an O(n) operation as it shifts all subsequent elements to the left.
     */
    void remove_shift(USize idx) noexcept {
        FR_ASSERT(idx < m_size, "index out of bounds");

        std::destroy_at(m_data + idx);
        mem::shift_range_left(m_data + idx, m_size - idx - 1, 1);
        --m_size;
    }

    /**
     * @brief Insert a copy of value at the specified index.
     *
     * @param idx Index at which to insert.
     * @param value The value to copy.
     * @pre idx <= size().
     * @pre T must be nothrow copy constructible and move constructible.
     * @note All elements from idx onwards are shifted to the right. O(n).
     */
    void insert(USize idx, const T &value) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        emplace(idx, value);
    }

    /**
     * @brief Insert value at the specified index by moving it.
     *
     * @param idx Index at which to insert.
     * @param value The value to move.
     * @pre idx <= size().
     * @pre T must be nothrow move constructible.
     * @note All elements from idx onwards are shifted to the right. O(n).
     */
    void insert(USize idx, T &&value) noexcept {
        emplace(idx, std::move(value));
    }

    /**
     * @brief Construct an element in-place at the specified index.
     *
     * @tparam Args Argument types for T's constructor.
     * @param idx Index at which to insert.
     * @param args Arguments to pass to T's constructor.
     * @pre idx <= size().
     * @pre T must be nothrow constructible and move constructible.
     * @note All elements from idx onwards are shifted to the right. O(n).
     */
    template <typename... Args>
    void emplace(USize idx, Args &&...args) noexcept {
        static_assert(std::is_nothrow_constructible_v<T, Args...>,
                      "T must be nothrow constructible from Args");
        FR_ASSERT(idx <= m_size, "index out of bounds");

        do_grow_if_full();
        mem::shift_range_right(m_data + idx, m_size - idx, 1);
        std::construct_at(m_data + idx, std::forward<Args>(args)...);

        ++m_size;
    }

    /**
     * @brief Destroy all elements in the array, setting size to 0.
     *
     * @note Capacity remains unchanged.
     */
    void clear() noexcept {
        do_destroy_all();
        m_size = 0;
    }

    /**
     * @brief Shrink the array to a new size by destroying trailing elements.
     *
     * @param new_size The new size of the array.
     * @pre new_size <= size().
     * @note Trailing elements are destroyed, but capacity remains unchanged.
     */
    void shrink(USize new_size) noexcept {
        FR_ASSERT(new_size <= m_size, "shrink size overflow");

        mem::destroy_range(m_data + new_size, m_size - new_size);
        m_size = new_size;
    }

    /**
     * @brief Grow the array to a new size by default-initializing new elements.
     *
     * @param new_size The target size.
     * @pre T must be nothrow default constructible.
     * @note If new_size > size(), new elements are default-constructed. Reallocates if needed.
     */
    void grow_default(USize new_size) noexcept {
        FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);
        do_grow_if_needed(new_size);
        mem::default_init_range(m_data + m_size, new_size - m_size);

        m_size = new_size;
    }

    /**
     * @brief Grow the array to a new size by filling new elements with fill.
     *
     * @param new_size The target size.
     * @param fill The value to copy into new elements.
     * @pre T must be nothrow copy constructible.
     * @note If new_size > size(), new elements are copy-constructed from fill.
     */
    void grow_with(USize new_size, const T &fill) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        do_grow_if_needed(new_size);

        for (USize i = m_size; i < new_size; ++i) {
            std::construct_at(m_data + i, fill);
        }

        m_size = new_size;
    }

    // ---------------------------------------------------------- Shape Protocol

    template <typename Archive>
    void shape(Archive &archive) {
        if constexpr (Archive::action == ArchiveAction::Write) {
            USize sz = m_size;
            archive.prop("@size", sz);

            USize cap = m_capacity;
            archive.prop("@capacity", cap);
        } else {
            USize sz = 0;
            archive.prop("@size", sz);

            if (sz != m_size) {
                this->clear();
                this->grow_default(sz);
            }

            USize cap = 0;
            archive.prop("@capacity", cap);

            if (cap > m_capacity) {
                this->reserve(cap);
            }
        }

        archive.list("@items", [&](Archive &list_archive) {
            for (T &item : *this) {
                list_archive.prop("", item);
            }
        });
    }

    template <typename Archive>
    void shape(Archive &archive) const {
        if constexpr (Archive::action == ArchiveAction::Write) {
            USize sz = m_size;
            archive.prop("@size", sz);

            USize cap = m_capacity;
            archive.prop("@capacity", cap);

            archive.list("@items", [&](Archive &list_archive) {
                for (const T &item : *this) {
                    list_archive.prop("", item);
                }
            });
        } else {
            FR_ASSERT(false, "cannot deserialize into const DynamicArray");
        }
    }

private:
    // --------------------------------------------------------------- Internals

    /**
     * @brief Calculate the next capacity based on a growth factor.
     *
     * @param current The current capacity.
     * @return The suggested next capacity.
     */
    USize do_next_capacity(USize current) const noexcept {
        if (current == 0)
            return 8;
        return (current * GROWTH_FACTOR) / 100;
    }

    /**
     * @brief Perform an initial allocation or a reallocation to expand capacity.
     *
     * @param new_capacity The new capacity to reserve.
     * @pre new_capacity > m_capacity.
     */
    void do_reserve(USize new_capacity) noexcept {
        FR_ASSERT(new_capacity > m_capacity, "capacity must grow");

        if (!m_data) {
            m_data = static_cast<T *>(m_alloc->allocate(new_capacity * sizeof(T), alignof(T)));
            m_capacity = new_capacity;
            return;
        }

        do_reallocate(new_capacity);
    }

    /**
     * @brief Reallocate the buffer to a new capacity.
     *
     * @param new_capacity The target capacity.
     * @pre new_capacity >= size().
     * @note Uses trivial relocation if T is trivially copyable.
     */
    void do_reallocate(USize new_capacity) noexcept {
        FR_ASSERT(new_capacity >= m_size, "capacity too small");

        // @todo Implement is_trivially_relocatble
        if constexpr (std::is_trivially_copyable<T>::value) {
            m_data = static_cast<T *>(m_alloc->reallocate(m_data, m_capacity * sizeof(T),
                                                          new_capacity * sizeof(T), alignof(T)));
        } else {
            T *new_data = static_cast<T *>(m_alloc->allocate(new_capacity * sizeof(T), alignof(T)));
            mem::transfer_init_range(m_data, m_size, new_data);
            m_alloc->deallocate(m_data, m_capacity * sizeof(T), alignof(T));
            m_data = new_data;
        }

        m_capacity = new_capacity;
    }

    /**
     * @brief Grows the array if the size has reached capacity.
     */
    void do_grow_if_full() noexcept {
        if (is_full()) {
            do_reserve(do_next_capacity(m_capacity));
        }
    }

    /**
     * @brief Grows the array if required_size exceeds current capacity.
     *
     * @param required_size The minimum capacity needed.
     */
    void do_grow_if_needed(USize required_size) noexcept {
        if (required_size > m_capacity) {
            do_reserve(std::max(do_next_capacity(m_capacity), required_size));
        }
    }

    /**
     * @brief Destroys all elements in the current buffer.
     */
    void do_destroy_all() noexcept {
        mem::destroy_range(m_data, m_size);
    }

    /**
     * @brief Frees the internal storage buffer.
     */
    void do_free_storage() noexcept {
        if (!m_data)
            return;

        m_alloc->deallocate(m_data, m_capacity * sizeof(T), alignof(T));
        m_data = nullptr;
        m_capacity = 0;
    }

    /**
     * @brief Helper for deep copying from another array during construction.
     *
     * @param other Source array.
     */
    void do_copy_from(const DynamicArray &other) noexcept {
        if (other.m_size == 0) {
            return;
        }

        do_reserve(other.m_size);
        mem::copy_init_range(other.m_data, other.m_size, m_data);

        m_size = other.m_size;
    }

    /**
     * @brief Helper for deep copying from another array during assignment.
     *
     * @param other Source array.
     */
    void do_assign_copy(const DynamicArray &other) noexcept {
        const USize overlap = std::min(m_size, other.m_size);

        if (overlap > 0) {
            mem::copy_assign_range(other.m_data, overlap, m_data);
        }

        if (other.m_size > m_size) {
            do_grow_if_needed(other.m_size);
            mem::copy_init_range(other.m_data + overlap, other.m_size - overlap, m_data + overlap);
        } else if (other.m_size < m_size) {
            mem::destroy_range(m_data + other.m_size, m_size - other.m_size);
        }

        m_size = other.m_size;
    }

    /**
     * @brief Helper for moving storage from another array.
     *
     * @param other Source array.
     */
    void do_move_from(DynamicArray &&other) noexcept {
        m_alloc = other.m_alloc;
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;

        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
        other.m_alloc = get_ambient_ctx().alloc;
    }
};
} // namespace fr
