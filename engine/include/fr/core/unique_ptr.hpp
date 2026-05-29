/**
 * @file unique_ptr.hpp
 * @author Tfoedy
 *
 * @brief Smart unique pointer implementation with custom allocator support.
 *
 * UniquePtr ensures that dynamically allocated memory and objects are properly
 * destroyed and deallocated when the pointer goes out of scope.
 */

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/typetraits.hpp"

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
[[nodiscard]] inline UniquePtr<T> make_unique_in(Alloc *alloc, Args &&...args);

template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
[[nodiscard]] inline UniquePtr<T> make_unique(USize size);

template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
[[nodiscard]] inline UniquePtr<T> make_unique_in(Alloc *alloc, USize size);

template <typename T>
    requires(!std::is_array_v<T>)
[[nodiscard]] inline UniquePtr<T> adopt_unique(T *raw_ptr, Alloc *alloc = get_ambient_ctx().alloc);

template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
[[nodiscard]] inline UniquePtr<T> adopt_unique(std::remove_extent_t<T> *raw_ptr, USize size,
                                               Alloc *alloc = get_ambient_ctx().alloc);

// ---------------------------------------------------------
// UniquePtr <T> (single object)
// ---------------------------------------------------------

/**
 * @brief Smart pointer managing a single allocated object.
 * @tparam T Type of the managed object.
 *
 * @note Foundational requirements for T are enforced via IsNothrowBase.
 */
template <typename T>
class UniquePtr {
    static_assert(IsNothrowBase<T>, "T must satisfy foundational nothrow requirements");

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

    /// @brief Converting move constructor
    template <typename U>
        requires(!std::is_array_v<U> && std::is_convertible_v<U *, T *>)
    UniquePtr(UniquePtr<U> &&other) noexcept
        : m_ptr(other.leak()),
          m_alloc(other.alloc()) {
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

    /// @brief Converting move assignment
    template <typename U>
        requires(!std::is_array_v<U> && std::is_convertible_v<U *, T *>)
    UniquePtr &operator=(UniquePtr<U> &&other) noexcept {
        if (reinterpret_cast<void *>(this) != reinterpret_cast<void *>(&other)) {
            clear();
            m_ptr = other.leak();
            m_alloc = other.alloc(); // Changed from other.m_alloc
        }
        return *this;
    }

    /// @brief Nullptr assignment. Clears the pointer.
    UniquePtr &operator=(std::nullptr_t) noexcept {
        clear();
        return *this;
    }

    /**
     * @brief Dereference operator.
     *
     * @return Pointer to the managed object.
     */
    T *operator->() const noexcept {
        FR_ASSERT(m_ptr != nullptr, "dereference null pointer");
        return m_ptr;
    }

    /**
     * @brief Dereference operator.
     *
     * @return Reference to the managed object.
     */
    T &operator*() const noexcept {
        FR_ASSERT(m_ptr != nullptr, "dereference null pointer");
        return *m_ptr;
    }

    /**
     * @brief Boolean conversion.
     *
     * @return True if managing an object.
     */
    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    /**
     * @brief Logical NOT operator.
     *
     * @return True if empty.
     */
    bool operator!() const noexcept {
        return !m_ptr;
    }

    /**
     * @brief Equality comparison with nullptr.
     *
     * @return True if empty.
     */
    bool operator==(std::nullptr_t) const noexcept {
        return m_ptr == nullptr;
    }

    /**
     * @brief Gets the underlying raw pointer without transferring ownership.
     *
     * @return Raw pointer.
     */
    T *borrow() const noexcept {
        return m_ptr;
    }

    /**
     * @brief Releases ownership of the managed object.
     *
     * @return Raw pointer. Caller is now responsible for memory.
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
     * @brief Swaps contents with another UniquePtr.
     *
     * @param other Pointer to swap with.
     */
    void swap(UniquePtr &other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_alloc, other.m_alloc);
    }

    /**
     * @brief Returns the allocator used.
     *
     * @return Pointer to allocator.
     */
    Alloc *alloc() const noexcept {
        return m_alloc;
    }

    template <typename Archive>
    void shape(Archive &archive) {
        bool has_value = (m_ptr != nullptr);
        archive.prop("@has_value", has_value);

        if constexpr (Archive::action == ArchiveAction::Write) {
            if (has_value) {
                archive.prop("@value", *m_ptr);
            }
        } else {
            if (has_value) {
                if (!m_ptr) {
                    *this = make_unique<T>();
                }
                archive.prop("@value", *m_ptr);
            } else {
                this->clear();
            }
        }
    }

private:
    T *m_ptr = nullptr;
    Alloc *m_alloc = nullptr;

    explicit UniquePtr(T *ptr, Alloc *alloc)
        : m_ptr(ptr),
          m_alloc(alloc) {
    }

    template <typename U, typename... Args>
        requires(!std::is_array_v<U>)
    friend UniquePtr<U> make_unique_in(Alloc *alloc, Args &&...args);

    template <typename U>
        requires(!std::is_array_v<U>)
    friend UniquePtr<U> adopt_unique(U *raw_ptr, Alloc *alloc);
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
    static_assert(IsNothrowBase<std::remove_extent_t<T>>,
                  "T must satisfy foundational nothrow requirements");

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

    /**
     * @brief Array element accessor.
     *
     * @param index Element index.
     * @return Reference to the element.
     * @pre m_ptr != nullptr.
     * @pre index < size.
     */
    std::remove_extent_t<T> &operator[](USize index) const noexcept {
        FR_ASSERT(m_ptr != nullptr, "dereference null pointer");
        FR_ASSERT(index < m_size, "index out of bounds");
        return m_ptr[index];
    }

    /**
     * @brief Boolean conversion.
     *
     * @return True if managing an array.
     */
    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

    /**
     * @brief Logical NOT operator.
     *
     * @return True if empty.
     */
    bool operator!() const noexcept {
        return !m_ptr;
    }

    /**
     * @brief Equality comparison with nullptr.
     *
     * @return True if empty.
     */
    bool operator==(std::nullptr_t) const noexcept {
        return m_ptr == nullptr;
    }

    /**
     * @brief Returns the underlying pointer.
     *
     * @return Raw pointer.
     */
    std::remove_extent_t<T> *borrow() const noexcept {
        return m_ptr;
    }

    /**
     * @brief Returns the array size.
     *
     * @return Number of elements.
     */
    USize get_size() const noexcept {
        return m_size;
    }

    /**
     * @brief Returns the allocator used.
     *
     * @return Pointer to allocator.
     */
    Alloc *get_allocator() const noexcept {
        return m_alloc;
    }

    template <typename Archive>
    void shape(Archive &archive) {
        USize sz = m_size;
        archive.prop("@size", sz);

        if constexpr (Archive::action == ArchiveAction::Read) {
            if (sz != m_size) {
                this->clear();
                if (sz > 0) {
                    *this = make_unique<T>(sz);
                }
            }
        }
        archive.list("@items", [&](Archive &list_archive) {
            for (USize i = 0; i < m_size; ++i) {
                list_archive.prop("", m_ptr[i]);
            }
        });
    }

    /**
     * @brief Releases ownership of the managed array.
     *
     * @return Raw pointer.
     */
    [[nodiscard]] std::remove_extent_t<T> *leak() noexcept {
        std::remove_extent_t<T> *temp = m_ptr;
        m_ptr = nullptr;
        return temp;
    }

    /**
     * @brief Destroys the array and frees memory.
     */
    void clear() noexcept {
        // maybe throw an exception here?
        // - please don't
        if (m_ptr) {
            fr::mem::destroy_range(m_ptr, m_size);
            if (m_alloc) {
                m_alloc->deallocate(m_ptr, m_size * sizeof(std::remove_extent_t<T>),
                                    alignof(std::remove_extent_t<T>));
            }
            m_ptr = nullptr;
            m_size = 0;
        }
    }

    /**
     * @brief Swaps contents with another UniquePtr.
     *
     * @param other Pointer to swap with.
     */
    void swap(UniquePtr &other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_alloc, other.m_alloc);
        std::swap(m_size, other.m_size);
    }

private:
    std::remove_extent_t<T> *m_ptr = nullptr;
    Alloc *m_alloc = nullptr;
    USize m_size = 0;

    explicit UniquePtr(std::remove_extent_t<T> *ptr, Alloc *alloc, USize size)
        : m_ptr(ptr),
          m_alloc(alloc),
          m_size(size) {
    }

    template <typename U>
        requires(std::is_array_v<U> && std::extent_v<U> == 0)
    friend UniquePtr<U> make_unique_in(Alloc *alloc, USize size);

    template <typename U>
        requires(std::is_array_v<U> && std::extent_v<U> == 0)
    friend UniquePtr<U> adopt_unique(std::remove_extent_t<U> *raw_ptr, USize size, Alloc *alloc);
};

// ---------------------------------------------------------
// Factory Functions
// ---------------------------------------------------------

/**
 * @brief Constructs an object using a specific allocator.
 *
 * @tparam T Type to construct.
 * @param alloc Allocator to use.
 * @param args Arguments for the constructor.
 * @return UniquePtr to the new object.
 */
template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
inline UniquePtr<T> make_unique_in(Alloc *alloc, Args &&...args) {
    FR_ASSERT(alloc != nullptr, "allocator must be non-null");
    void *mem = alloc->allocate(sizeof(T), alignof(T));

    T *ptr = std::construct_at(static_cast<T *>(mem), std::forward<Args>(args)...);
    return UniquePtr<T>(ptr, alloc);
}

/**
 * @brief Constructs an object using the default allocator.
 */
template <typename T, typename... Args>
    requires(!std::is_array_v<T>)
inline UniquePtr<T> make_unique(Args &&...args) {
    return make_unique_in<T>(get_ambient_ctx().alloc, std::forward<Args>(args)...);
}

/**
 * @brief Constructs an array using a specific allocator.
 */
template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
inline UniquePtr<T> make_unique_in(Alloc *alloc, USize size) {
    FR_ASSERT(alloc != nullptr, "allocator must be non-null");
    using ElementType = std::remove_extent_t<T>;

    void *mem = alloc->allocate(size * sizeof(ElementType), alignof(ElementType));
    ElementType *arr = static_cast<ElementType *>(mem);

    fr::mem::default_init_range(arr, size);

    return UniquePtr<T>(arr, alloc, size);
}

/**
 * @brief Constructs an array using the default allocator.
 */
template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
inline UniquePtr<T> make_unique(USize size) {
    return make_unique_in<T>(get_ambient_ctx().alloc, size);
}

/**
 * @brief Takes ownership of a raw pointer.
 */
template <typename T>
    requires(!std::is_array_v<T>)
inline UniquePtr<T> adopt_unique(T *raw_ptr, Alloc *alloc) {
    FR_ASSERT(raw_ptr != nullptr, "pointer must be non-null");
    FR_ASSERT(alloc != nullptr, "allocator must be non-null");
    return UniquePtr<T>(raw_ptr, alloc);
}

/**
 * @brief Takes ownership of a raw array pointer.
 */
template <typename T>
    requires(std::is_array_v<T> && std::extent_v<T> == 0)
inline UniquePtr<T> adopt_unique(std::remove_extent_t<T> *raw_ptr, USize size, Alloc *alloc) {
    FR_ASSERT(raw_ptr != nullptr, "pointer must be non-null");
    FR_ASSERT(alloc != nullptr, "allocator must be non-null");
    return UniquePtr<T>(raw_ptr, alloc, size);
}

} // namespace fr
