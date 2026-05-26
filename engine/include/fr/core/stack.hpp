/**
 * @file stack.hpp
 * @author Kiju
 * @brief Simple stack implementation.
 *
 * @details
 * Stack is a LIFO data structure.
 * Under the hood it is implemented with a `fr::DynamicArray`.
 * `fr::Stack` is a thin wrapper around `fr::DynamicArray`. This implementation is quite lenghty,
 * but that is for a reason - it uses composition over inhertance. Because of this the API of
 * `fr::Stack` and `fr::DynamicArray` are decoupled, and thus easier to refactor if needed.
 * `fr::Stack` forwards slicing opearations because it stores its data in a contiguous fashion.
 */

#pragma once

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"

namespace fr {

/**
 * @brief Dynamic Stack with a growth factor of 150%.
 *
 * @tparam T Element type.
 * @pre `T` must be nothrow destructible.
 * @pre `T` must be nothrow move constructible.
 * @pre `T` must be nothrow move assignable.
 */
template <typename T>
class Stack {
private:
    // Ensuring the basic preconditions of `T`
    FR_STATIC_ASSERT_NOTHROW_BASE(T);

    // -------------------------------------------------------- Member Variables
    DynamicArray<T> m_array{};

public:
    // ---------------------------------------------------- Constants & Typedefs
    static constexpr USize GROWTH_FACTOR = DynamicArray<T>::GROWTH_FACTOR;
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
     * @brief Constructs an empty `Stack`.
     * @note Does not allocate.
     */
    Stack() noexcept = default;

    /**
     * @brief Constructs a `Stack` from an initializer list.
     * @param list Initializer list of elements to copy.
     *
     * @note Allocates at least the size of `list`.
     * @pre `T` must be nothrow copy constructible.
     */
    Stack(std::initializer_list<T> list) noexcept
        : m_array(list) {
    }

    /**
     * @brief Copy-construct a new `Stack` from an existing one - deep copy.
     * @param other The `Stack` to copy from.
     *
     * @note Allocator does not propagate.
     * @pre `T` must be nothrow copy constructible.
     */
    Stack(const Stack &other) noexcept
        : m_array(other.m_array) {
    }

    /**
     * @brief Move-construct a new `Stack`, stealing storage from other.
     * @param other The `Stack` to move from.
     */
    Stack(Stack &&other) noexcept
        : m_array(std::move(other.m_array)) {
    }

    /**
     * @brief Construct an empty `Stack` with the given allocator.
     * @param alloc Pointer to the allocator.
     *
     * @note Does not allocate.
     * @pre `alloc` must be non-null.
     */
    explicit Stack(Alloc *alloc) noexcept
        : m_array(alloc) {
    }

    /**
     * @brief Destroy all elements and free the underlying storage.
     */
    ~Stack() noexcept = default;

    /**
     * @brief Create an empty `Stack` using the given allocator.
     * @param alloc Pointer to the allocator.
     *
     * @return A new empty `Stack`.
     * @pre `alloc` must be non-null.
     */
    [[nodiscard]] static Stack with_alloc(Alloc *alloc) noexcept {
        return Stack(alloc);
    }

    /**
     * @brief Create an empty `Stack` with an initial reserved capacity using the given allocator.
     *
     * @param alloc Pointer to the allocator.
     * @param capacity The number of elements to reserve.
     * @pre `capacity` must be non-zero.
     * @return A new empty `Stack`.
     *
     * @note Allocates.
     * @pre `alloc` must be non-null.
     */
    [[nodiscard]] static Stack with_capacity(Alloc *alloc, USize capacity) noexcept {
        Stack stack(alloc);
        stack.reserve(capacity);
        return stack;
    }

    /**
     * @brief Create an empty `Stack` with an initial reserved capacity using the ambient
     * allocator.
     *
     * @param capacity The number of elements to reserve.
     * @pre `capacity` must be non-zero.
     * @return A new empty `Stack`.
     *
     * @note Allocates.
     */
    [[nodiscard]] static Stack with_capacity(USize capacity) noexcept {
        return with_capacity(get_ambient_ctx().alloc, capacity);
    }

    /**
     * @brief Create a stack of a specific size using a specific allocator.
     *
     * @param alloc Pointer to the allocator to use.
     * @pre `alloc` must be non-null.
     * @param size Initial number of elements.
     * @return A new stack of size `size`.
     *
     * @pre `T` must be nothrow default constructible.
     */
    [[nodiscard]] static Stack with_size(Alloc *alloc, USize size) noexcept {
        Stack stack(alloc);
        stack.m_array = DynamicArray<T>::with_size(alloc, size);
        return stack;
    }

    /**
     * @brief Create a stack of a specific size with default-initialized elements.
     *
     * @param size Initial number of elements.
     * @return A new Stack instance of the requested size.
     * @pre T must be nothrow default constructible.
     */
    [[nodiscard]] static Stack with_size(USize size) noexcept {
        return with_size(get_ambient_ctx().alloc, size);
    }

    /**
     * @brief Create a stack filled with a specific value using the ambient allocator.
     *
     * @param size Number of elements.
     * @param value Value to copy into every element.
     * @return A new Stack instance filled with value.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static Stack from_repeated(USize size, const T &value) noexcept {
        return from_repeated(get_ambient_ctx().alloc, size, value);
    }

    /**
     * @brief Create a stack filled with a specific value using a specific allocator.
     *
     * @param alloc Pointer to the allocator to use.
     * @param size Number of elements.
     * @param value Value to copy into every element.
     * @return A new Stack instance filled with value.
     * @pre alloc must be non-null.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static Stack from_repeated(Alloc *alloc, USize size, const T &value) noexcept {
        Stack stack(alloc);
        stack.m_array = DynamicArray<T>::from_repeated(alloc, size, value);
        return stack;
    }

    /**
     * @brief Create a stack from a slice using the ambient allocator.
     *
     * @param slice Source slice.
     * @return A new Stack instance containing the slice elements.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static Stack from_slice(Slice<const std::remove_const_t<T>> slice) noexcept {
        return from_slice(get_ambient_ctx().alloc, slice);
    }

    /**
     * @brief Create a stack from a slice using a specific allocator.
     *
     * @param alloc Pointer to the allocator to use.
     * @param slice Source slice.
     * @return A new Stack instance containing the slice elements.
     * @pre alloc must be non-null.
     * @pre T must be nothrow copy constructible.
     */
    [[nodiscard]] static Stack from_slice(Alloc *alloc,
                                          Slice<const std::remove_const_t<T>> slice) noexcept {
        Stack stack(alloc);
        stack.m_array = DynamicArray<T>::from_slice(alloc, slice);
        return stack;
    }

    // --------------------------------------------------- Move & Copy Operators

    /**
     * @brief Copy-assign elements from other.
     *
     * @param other The stack to copy from.
     * @return Reference to this stack.
     * @note Performs a deep copy.
     */
    Stack &operator=(const Stack &other) noexcept = default;

    /**
     * @brief Move-assign storage from other.
     *
     * @param other The stack to move from.
     * @return Reference to this stack.
     */
    Stack &operator=(Stack &&other) noexcept = default;

    // --------------------------------------------------------------- Iterators

    /**
     * @brief Returns an iterator to the first element.
     * @return Pointer to the first element.
     */
    T *begin() noexcept {
        return m_array.begin();
    }

    /**
     * @brief Returns an iterator to the element following the last element.
     * @return Pointer past the last element.
     */
    T *end() noexcept {
        return m_array.end();
    }

    /**
     * @brief Returns a constant iterator to the first element.
     * @return Constant pointer to the first element.
     */
    const T *begin() const noexcept {
        return m_array.begin();
    }

    /**
     * @brief Returns a constant iterator to the element following the last element.
     * @return Constant pointer past the last element.
     */
    const T *end() const noexcept {
        return m_array.end();
    }

    /**
     * @brief Returns a constant iterator to the first element.
     * @return Constant pointer to the first element.
     */
    const T *cbegin() const noexcept {
        return m_array.cbegin();
    }

    /**
     * @brief Returns a constant iterator to the element following the last element.
     * @return Constant pointer past the last element.
     */
    const T *cend() const noexcept {
        return m_array.cend();
    }

    // ---------------------------------------------------------- Element Access

    /**
     * @brief Access the top element.
     *
     * @return Reference to the top element.
     * @pre `!is_empty()`.
     */
    T &top() noexcept {
        return m_array.back();
    }

    /**
     * @brief Access the top element (const).
     *
     * @return Constant reference to the top element.
     * @pre `!is_empty()`.
     */
    const T &top() const noexcept {
        return m_array.back();
    }

    /**
     * @brief Direct access to the underlying storage.
     * @return Pointer to the beginning of the internal buffer.
     */
    T *data() noexcept {
        return m_array.data();
    }

    /**
     * @brief Direct access to the underlying storage (const).
     * @return Constant pointer to the beginning of the internal buffer.
     */
    const T *data() const noexcept {
        return m_array.data();
    }

    // ------------------------------------------------------- Slice Operationas

    /**
     * @brief Create a constant slice view over the entire stack.
     * @return A Slice covering [0, size()).
     */
    Slice<const T> slice() const & noexcept {
        return m_array.slice();
    }

    /**
     * @brief Create a mutable slice view over the entire stack.
     *
     * @return A mutable Slice covering [0, size()).
     * @note Only available if T is not const.
     */
    Slice<T> slice_mut() & noexcept
        requires(!std::is_const_v<T>)
    {
        return m_array.slice_mut();
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
        return m_array.slice(from, to);
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
        return m_array.slice_mut(from, to);
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
        return m_array.slice_from(from);
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
        return m_array.slice_mut_from(from);
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
        return m_array.slice_to(to);
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
        return m_array.slice_mut_to(to);
    }

    Slice<const T> slice_to(USize) const && = delete;
    Slice<T> slice_mut_to(USize) && = delete;

    // ---------------------------------------------------------------- Capacity

    /**
     * @brief Get the current number of elements in the stack.
     * @return Current size.
     */
    USize size() const noexcept {
        return m_array.size();
    }

    /**
     * @brief Get the total number of elements that can be held without reallocation.
     * @return Current capacity.
     */
    USize capacity() const noexcept {
        return m_array.capacity();
    }

    /**
     * @brief Check if the stack contains no elements.
     * @return True if size is 0.
     */
    bool is_empty() const noexcept {
        return m_array.is_empty();
    }

    /**
     * @brief Check if the stack size has reached its capacity.
     * @return True if size equals capacity.
     */
    bool is_full() const noexcept {
        return m_array.is_full();
    }

    /**
     * @brief Get the allocator currently associated with this stack.
     * @return Pointer to the allocator.
     */
    const Alloc *alloc() const noexcept {
        return m_array.alloc();
    }

    /**
     * @brief Ensure the stack has space for at least new_capacity elements.
     * @param new_capacity The minimum desired capacity.
     */
    void reserve(USize new_capacity) noexcept {
        m_array.reserve(new_capacity);
    }

    /**
     * @brief Reduce capacity to match current size.
     */
    void shrink_to_fit() noexcept {
        m_array.shrink_to_fit();
    }

    // --------------------------------------------------------------- Modifiers

    /**
     * @brief Push a copy of value to the top of the stack.
     * @param value The value to copy.
     */
    void push(const T &value) noexcept {
        m_array.push_back(value);
    }

    /**
     * @brief Push value to the top of the stack by moving it.
     * @param value The value to move.
     */
    void push(T &&value) noexcept {
        m_array.push_back(std::move(value));
    }

    /**
     * @brief Remove the top element from the stack.
     * @pre `!is_empty()`.
     */
    void pop() noexcept {
        m_array.pop_back();
    }

    /**
     * @brief Construct an element in-place at the top of the stack.
     *
     * @tparam Args Argument types for T's constructor.
     * @param args Arguments to pass to T's constructor.
     * @return Reference to the newly created element.
     * @pre T must be nothrow constructible from Args.
     */
    template <typename... Args>
    T &emplace(Args &&...args) noexcept {
        return m_array.emplace_back(std::forward<Args>(args)...);
    }

    /**
     * @brief Destroy all elements in the stack, setting size to 0.
     *
     * @note Capacity remains unchanged.
     */
    void clear() noexcept {
        m_array.clear();
    }

    /**
     * @brief Get mutable access to the underlying dynamic array.
     *
     * @return Mutable reference to the underlying array.
     */
    DynamicArray<T> &dynamic_array() noexcept {
        return m_array;
    }

    // ---------------------------------------------------------- Shape Protocol

    template <typename Archive>
    void shape(Archive &archive) {
        m_array.shape(archive);
    }

    template <typename Archive>
    void shape(Archive &archive) const {
        if constexpr (Archive::action == ArchiveAction::Write) {
            m_array.shape(archive);
        }
    }
};
} // namespace fr

