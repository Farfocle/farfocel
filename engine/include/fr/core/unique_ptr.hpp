/**
 * @file unique_ptr.hpp
 * @author Tfoedy
 *
 * @brief Smart unique pointer implementation with custom allocator support.
 * * UniquePtr ensures that dynamically allocated memory and objects are properly
 * destroyed and deallocated when the pointer goes out of scope.
 */

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "fr/core/allocator.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

// ---------------------------------------------------------
// Forward Declarations
// ---------------------------------------------------------

template <typename T>
class UniquePtr;

template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
[[nodiscard]] inline UniquePtr<T> make_unique(Args &&...args);

template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
[[nodiscard]] inline UniquePtr<T> make_unique_in(Allocator *alloc, Args &&...args);

template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
[[nodiscard]] inline UniquePtr<T> make_unique(USize size);

template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
[[nodiscard]] inline UniquePtr<T> make_unique_in(Allocator *alloc, USize size);

template <typename T>
    requires(!std::is_array_v<T>)
[[nodiscard]] inline UniquePtr<T> adopt_unique(T *raw_ptr,
                                               Allocator *alloc = globals::get_default_allocator());

template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
[[nodiscard]] inline UniquePtr<T> adopt_unique(std::remove_extent_t<T> *raw_ptr, USize size,
                                               Allocator *alloc = globals::get_default_allocator());

// ---------------------------------------------------------
// UniquePtr <T> (single object)
// ---------------------------------------------------------

/**
 * @brief Smart pointer managing a single allocated object.
 * @tparam T Type of the managed object.
 */
template <typename T>
class UniquePtr {
public:
    /// @brief Creates an empty UniquePtr.
    UniquePtr() = default;

    /// @brief Creates an empty UniquePtr from nullptr.
    UniquePtr(std::nullptr_t) noexcept
        : m_ptr(nullptr),
          m_alloc(nullptr) {
    }

    /// @brief Move constructor. Transfers ownership.
    UniquePtr(UniquePtr &&other) noexcept
        : m_ptr(other.leak()),
          m_alloc(other.m_alloc) {
    }

    /// @brief Destroys the managed object and frees memory.
    ~UniquePtr() noexcept {
        clear();
    }

    UniquePtr(const UniquePtr &) = delete;
    UniquePtr &operator=(const UniquePtr &) = delete;

    /// @brief Move assignment.
    UniquePtr &operator=(UniquePtr &&other) noexcept {
        if (this != &other) {
            UniquePtr temp(std::move(other));
            swap(temp);
        }
        return *this;
    }

    /// @brief Nullptr assignment. Clears the pointer.
    UniquePtr &operator=(std::nullptr_t) noexcept {
        clear();
        return *this;
    }

    T *operator->() const noexcept {
        FR_ASSERT(m_ptr != nullptr, "Dereferencing null UniquePtr");
        return m_ptr;
    }

    T &operator*() const noexcept {
        FR_ASSERT(m_ptr != nullptr, "Dereferencing null UniquePtr");
        return *m_ptr;
    }

    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    bool operator!() const noexcept {
        return !m_ptr;
    }

    bool operator==(std::nullptr_t) const noexcept {
        return m_ptr == nullptr;
    }

    /**
     * @brief Gets the underlying raw pointer without transferring ownership.
     * @return Raw pointer to the object.
     */
    T *borrow() const noexcept {
        return m_ptr;
    }

    /**
     * @brief Releases ownership of the managed object.
     * @return Raw pointer to the object. Caller is now responsible for memory.
     */
    [[nodiscard]] T *leak() noexcept {
        T *temp = m_ptr;
        m_ptr = nullptr;
        return temp;
    }

    /**
     * @brief Destroys the object and frees memory.
     */
    void clear() noexcept {
        if (m_ptr) {
            std::destroy_at(m_ptr);
            if (m_alloc) {
                m_alloc->deallocate(m_ptr, sizeof(T), alignof(T));
            }
            m_ptr = nullptr;
        }
    }

    /**
     * @brief Swaps the contents of this pointer with another.
     */
    void swap(UniquePtr &other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_alloc, other.m_alloc);
    }

    /**
     * @brief Returns the allocator used to manage this object.
     */
    Allocator *get_allocator() const noexcept {
        return m_alloc;
    }

private:
    T *m_ptr = nullptr;
    Allocator *m_alloc = nullptr;

    explicit UniquePtr(T *ptr, Allocator *alloc)
        : m_ptr(ptr),
          m_alloc(alloc) {
    }

    template <typename U, typename... Args>
        requires(!std::is_array_v<U>)
    friend UniquePtr<U> make_unique_in(Allocator *alloc, Args &&...args);

    template <typename U>
        requires(!std::is_array_v<U>)
    friend UniquePtr<U> adopt_unique(U *raw_ptr, Allocator *alloc);
};

// ---------------------------------------------------------
// UniquePtr <T[]> (array of objects)
// ---------------------------------------------------------

/**
 * @brief Smart pointer managing a dynamically allocated array of objects.
 * @tparam T Array type (e.g., int[]).
 */
template <typename T>
class UniquePtr<T[]> {
public:
    UniquePtr() = default;

    UniquePtr(std::nullptr_t) noexcept
        : m_ptr(nullptr),
          m_alloc(nullptr),
          m_size(0) {
    }

    UniquePtr(UniquePtr &&other) noexcept
        : m_ptr(other.leak()),
          m_alloc(other.m_alloc),
          m_size(other.m_size) {
        other.m_size = 0;
    }

    ~UniquePtr() noexcept {
        clear();
    }

    UniquePtr(const UniquePtr &) = delete;
    UniquePtr &operator=(const UniquePtr &) = delete;

    UniquePtr &operator=(UniquePtr &&other) noexcept {
        if (this != &other) {
            UniquePtr temp(std::move(other));
            swap(temp);
        }
        return *this;
    }

    UniquePtr &operator=(std::nullptr_t) noexcept {
        clear();
        return *this;
    }

    T &operator[](USize index) const noexcept {
        FR_ASSERT(m_ptr != nullptr, "Dereferencing null UniquePtr array");
        FR_ASSERT(index < m_size, "Index out of bounds");
        return m_ptr[index];
    }

    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    bool operator!() const noexcept {
        return !m_ptr;
    }

    bool operator==(std::nullptr_t) const noexcept {
        return m_ptr == nullptr;
    }

    T *borrow() const noexcept {
        return m_ptr;
    }

    USize get_size() const noexcept {
        return m_size;
    }

    Allocator *get_allocator() const noexcept {
        return m_alloc;
    }

    [[nodiscard]] T *leak() noexcept {
        T *temp = m_ptr;
        m_ptr = nullptr;
        return temp;
    }

    void clear() noexcept {
        // maybe throw an exception here?
        if (m_ptr) {
            fr::mem::destroy_range(m_ptr, m_size);
            if (m_alloc) {
                m_alloc->deallocate(m_ptr, m_size * sizeof(T), alignof(T));
            }
            m_ptr = nullptr;
            m_size = 0;
        }
    }

    void swap(UniquePtr &other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_alloc, other.m_alloc);
        std::swap(m_size, other.m_size);
    }

private:
    T *m_ptr = nullptr;
    Allocator *m_alloc = nullptr;
    USize m_size = 0;

    explicit UniquePtr(T *ptr, Allocator *alloc, USize size)
        : m_ptr(ptr),
          m_alloc(alloc),
          m_size(size) {
    }

    template <typename U>
        requires(std::is_array_v<U> && std::extent_v<U> == 0)
    friend UniquePtr<U> make_unique_in(Allocator *alloc, USize size);

    template <typename U>
        requires(std::is_array_v<U> && std::extent_v<U> == 0)
    friend UniquePtr<U> adopt_unique(std::remove_extent_t<U> *raw_ptr, USize size,
                                     Allocator *alloc);
};

// ---------------------------------------------------------
// Factory Functions
// ---------------------------------------------------------

/**
 * @brief Constructs an object using a specific allocator and wraps it in UniquePtr.
 */
template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
inline UniquePtr<T> make_unique_in(Allocator *alloc, Args &&...args) {
    FR_ASSERT(alloc != nullptr, "Allocator must not be null");
    void *mem = alloc->allocate(sizeof(T), alignof(T));

    T *ptr = std::construct_at(static_cast<T *>(mem), std::forward<Args>(args)...);
    return UniquePtr<T>(ptr, alloc);
}

/**
 * @brief Constructs an object using the default allocator and wraps it in UniquePtr.
 */
template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
inline UniquePtr<T> make_unique(Args &&...args) {
    return make_unique_in<T>(globals::get_default_allocator(), std::forward<Args>(args)...);
}

/**
 * @brief Constructs an array of objects using a specific allocator and wraps it in UniquePtr.
 */
template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
inline UniquePtr<T> make_unique_in(Allocator *alloc, USize size) {
    FR_ASSERT(alloc != nullptr, "Allocator must not be null");
    using ElementType = std::remove_extent_t<T>;

    void *mem = alloc->allocate(size * sizeof(ElementType), alignof(ElementType));
    ElementType *arr = static_cast<ElementType *>(mem);

    fr::mem::value_init_range(arr, size);

    return UniquePtr<T>(arr, alloc, size);
}

/**
 * @brief Constructs an array of objects using the default allocator and wraps it in UniquePtr.
 */
template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
inline UniquePtr<T> make_unique(USize size) {
    return make_unique_in<T>(globals::get_default_allocator(), size);
}

/**
 * @brief Takes ownership of a raw pointer.
 * @warning The pointer MUST have been allocated with the provided allocator.
 */
template <typename T>
    requires(!std::is_array_v<T>)
inline UniquePtr<T> adopt_unique(T *raw_ptr, Allocator *alloc) {
    FR_ASSERT(raw_ptr != nullptr, "Adopted pointer must not be null");
    FR_ASSERT(alloc != nullptr, "Allocator must not be null");
    return UniquePtr<T>(raw_ptr, alloc);
}

/**
 * @brief Takes ownership of a raw array pointer.
 * @warning The array MUST have been allocated with the provided allocator.
 */
template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
inline UniquePtr<T> adopt_unique(std::remove_extent_t<T> *raw_ptr, USize size, Allocator *alloc) {
    FR_ASSERT(raw_ptr != nullptr, "Adopted pointer must not be null");
    FR_ASSERT(alloc != nullptr, "Allocator must not be null");
    return UniquePtr<T>(raw_ptr, alloc, size);
}

} // namespace fr
