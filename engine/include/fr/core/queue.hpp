/**
 * @file queue.hpp
 * @author Kiju
 * @brief Dynamic queue.
 *
 * @brief This implementation is using a `fr::DynamicArray` unnder the good.
 * @todo `fr::DynamicArray` is fine for now, but it causes extra work to happen on storage growth.
 * It can be optimized by creating a custom storage system but I am too lazy to do it right now.
 * Have fun :)
 */

#pragma once

#include <memory>
#include <utility>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"

namespace fr {

/**
 * @brief Dynamic Queue with a growth factor of 150%.
 *
 * @tparam T Element type.
 * @pre `T` must be nothrow destructible.
 * @pre `T` must be nothrow move constructible.
 * @pre `T` must be nothrow move assignable.
 */
template <typename T>
class Queue {
private:
    // Ensuring the basic preconditions of `T`
    FR_STATIC_ASSERT_NOTHROW_BASE(T);

    // -------------------------------------------------------- Member Variables
    DynamicArray<T> m_array;
    USize m_front{0};
    USize m_back{0};
    USize m_size{0};

public:
    // ---------------------------------------------------- Constants & Typedefs
    static constexpr USize GROWTH_FACTOR = DynamicArray<T>::GROWTH_FACTOR;
    using value_type = T;
    using size_type = USize;
    using reference = T &;
    using const_reference = const T &;

    // ----------------------------------- Constructors & Factories & Destructor

    /**
     * @brief Constructs an empty `Queue`.
     * @note Does not allocate.
     */
    Queue() noexcept = default;

    /**
     * @brief Construct an empty `Queue` with the given allocator.
     * @param alloc Pointer to the allocator.
     *
     * @note Does not allocate.
     * @pre `alloc` must be non-null.
     */
    explicit Queue(Alloc *alloc) noexcept
        : m_array(alloc) {
    }

    // ---------------------------------------------------------- Element Access

    /**
     * @brief Access the front element.
     *
     * @return Reference to the front element.
     * @pre `!is_empty()`.
     */
    T &front() noexcept {
        FR_ASSERT(!is_empty(), "empty queue access");
        return m_array[m_front];
    }

    /**
     * @brief Access the front element (const).
     *
     * @return Constant reference to the front element.
     * @pre `!is_empty()`.
     */
    const T &front() const noexcept {
        FR_ASSERT(!is_empty(), "empty queue access");
        return m_array[m_front];
    }

    // ---------------------------------------------------------------- Capacity

    /**
     * @brief Get the current number of elements in the queue.
     * @return Current size.
     */
    USize size() const noexcept {
        return m_size;
    }

    /**
     * @brief Get the total number of storable elements without growth.
     * @return Current capacity.
     */
    USize capacity() const noexcept {
        return m_array.size();
    }

    /**
     * @brief Check if the queue contains no elements.
     * @return True if size is 0.
     */
    bool is_empty() const noexcept {
        return m_size == 0;
    }

    /**
     * @brief Check if the queue size has reached capacity.
     * @return True if size equals capacity.
     */
    bool is_full() const noexcept {
        return m_size == m_array.size();
    }

    /**
     * @brief Get the allocator currently associated with this queue.
     * @return Pointer to the allocator.
     */
    const Alloc *alloc() const noexcept {
        return m_array.alloc();
    }

    // --------------------------------------------------------------- Modifiers

    /**
     * @brief Enqueue a copy of value at the back of the queue.
     * @param value The value to copy.
     */
    void enqueue(const T &value) noexcept {
        FR_STATIC_ASSERT_NOTHROW_COPY_CONSTRUCTIBLE(T);
        emplace_enqueue(value);
    }

    /**
     * @brief Enqueue value at the back of the queue by moving it.
     * @param value The value to move.
     */
    void enqueue(T &&value) noexcept {
        emplace_enqueue(std::move(value));
    }

    /**
     * @brief Construct an element in-place at the back of the queue.
     *
     * @tparam Args Argument types for T's constructor.
     * @param args Arguments to pass to T's constructor.
     * @return Reference to the newly created element.
     * @pre T must be nothrow constructible from Args.
     */
    template <typename... Args>
    T &emplace_enqueue(Args &&...args) noexcept {
        static_assert(std::is_nothrow_constructible_v<T, Args...>,
                      "T must be nothrow constructible from Args");

        if (is_full()) {
            return do_enqueue_when_full(std::forward<Args>(args)...);
        }

        const USize slots = m_array.size();
        T *slot = m_array.data() + m_back;

        std::destroy_at(slot);
        T *ptr = std::construct_at(slot, std::forward<Args>(args)...);

        m_back = (m_back + 1) % slots;
        ++m_size;

        return *ptr;
    }

    /**
     * @brief Remove the front element from the queue.
     * @pre `!is_empty()`.
     */
    void dequeue() noexcept {
        FR_ASSERT(!is_empty(), "empty queue dequeue");

        const USize slots = m_array.size();
        m_front = (m_front + 1) % slots;
        --m_size;

        if (m_size == 0) {
            m_front = 0;
            m_back = 0;
        }
    }

    /**
     * @brief Destroy all elements in the queue and reset indices.
     *
     * @note Capacity remains unchanged.
     */
    void clear() noexcept {
        m_array.clear();
        m_front = 0;
        m_back = 0;
        m_size = 0;
    }

    /**
     * @brief Get mutable access to the underlying dynamic array.
     *
     * @return Mutable reference to the underlying array.
     */
    DynamicArray<T> &dynamic_array() noexcept {
        return m_array;
    }

private:
    /**
     * @brief Grow backing storage and enqueue a new element.
     *
     * @details
     * Appends the new element to trigger `DynamicArray` growth when needed, then manually
     * left-rotates the previous queue range into logical order so `m_front` resets to 0.
     *
     * @tparam Args Argument types for T's constructor.
     * @param args Arguments to pass to T's constructor.
     * @return Reference to the newly created element.
     */
    template <typename... Args>
    T &do_enqueue_when_full(Args &&...args) noexcept {
        const USize old_slots = m_array.size();
        const USize old_front = m_front;
        T &inserted = m_array.emplace_back(std::forward<Args>(args)...);

        if (old_slots != 0 && old_front != 0) {
            T *data = m_array.data();
            do_reverse_range(data, 0, old_front);
            do_reverse_range(data, old_front, old_slots);
            do_reverse_range(data, 0, old_slots);
        }

        ++m_size;
        m_front = 0;
        m_back = m_size;
        return inserted;
    }

    void do_reverse_range(T *data, USize from, USize to) noexcept {
        FR_ASSERT(data != nullptr, "data must be non-null");
        FR_ASSERT(from <= to, "invalid reverse range");

        if (to - from <= 1) {
            return;
        }

        USize left = from;
        USize right = to - 1;
        while (left < right) {
            std::swap(data[left], data[right]);

            ++left;
            --right;
        }
    }
};
} // namespace fr
