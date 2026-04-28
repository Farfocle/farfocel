/**
 * @file optional.hpp
 * @author Kiju
 *
 * @brief A noexcept Optional<T> with nil optimization support. This implementation is more minimal
 * than std::optional in terms of API (which is think is bloated anyways) but still contains a
 * monadic `map` and rust inspired `unwrap` and `unwrap_or`. It also overloads the arrow operator
 * `->` for quick and easy access. If something goes wrong it asserts, simple and fast. The main
 * selling point of this implementation is nil optimization. If a generic type `T` supports a nil
 * value (or is a raw pointer) the optional of this type does not add any additional memory
 * footprint - it is a zero const abstraction. For example: every raw pointer `T*` already has a nil
 * value `nullptr` thus, there is no need for extra bookkeeping. `nullptr` is an empty optional,
 * everthing else is a valid, value optional of `T`.
 */

#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "fr/core/macros.hpp"
#include "fr/core/nil.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

namespace impl {

/**
 * @brief Storage for Optional<T> where T is Nillable.
 * This storage uses the nil value of T to represent an empty state.
 */
template <typename T>
struct NillableOptionalStorage {
    T data;

    constexpr NillableOptionalStorage() noexcept
        : data(call_nil<T>()) {
    }

    constexpr NillableOptionalStorage(NilTag) noexcept
        : data(call_nil<T>()) {
    }

    template <typename... Args>
    constexpr NillableOptionalStorage(Args &&...args) noexcept
        : data(std::forward<Args>(args)...) {
        FR_STATIC_ASSERT((std::is_nothrow_constructible_v<T, Args...>),
                         "T must be nothrow constructible for Optional");
    }

    [[nodiscard]] constexpr bool is_nil() const noexcept {
        return call_is_nil(data);
    }

    T &get() noexcept {
        return data;
    }

    const T &get() const noexcept {
        return data;
    }

    void reset() noexcept {
        if (!is_nil()) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_at(&data);
            }
        }

        data = call_nil<T>();
    }

    template <typename... Args>
    void emplace(Args &&...args) noexcept {
        FR_STATIC_ASSERT((std::is_nothrow_constructible_v<T, Args...>),
                         "T must be nothrow constructible for Optional::emplace");
        reset();
        std::construct_at(&data, std::forward<Args>(args)...);
    }
};

/// @brief General storage for Optional<T> using a bool flag.
template <typename T>
struct GenericOptionalStorage {
    union {
        T value;
        U8 dummy;
    };

    bool engaged;

    constexpr GenericOptionalStorage() noexcept
        : dummy(0),
          engaged(false) {
    }

    constexpr GenericOptionalStorage(NilTag) noexcept
        : dummy(0),
          engaged(false) {
    }

    template <typename... Args>
    constexpr GenericOptionalStorage(Args &&...args) noexcept
        : value(std::forward<Args>(args)...),
          engaged(true) {
        FR_STATIC_ASSERT((std::is_nothrow_constructible_v<T, Args...>),
                         "T must be nothrow constructible for Optional");
    }

    GenericOptionalStorage(const GenericOptionalStorage &storage) noexcept
        : dummy(0),
          engaged(storage.engaged) {
        if (storage.engaged) {
            FR_STATIC_ASSERT((std::is_nothrow_copy_constructible_v<T>),
                             "T must be nothrow copy constructible");
            std::construct_at(&value, storage.value);
        }
    }

    GenericOptionalStorage(GenericOptionalStorage &&storage) noexcept
        : dummy(0),
          engaged(storage.engaged) {
        if (storage.engaged) {
            FR_STATIC_ASSERT((std::is_nothrow_move_constructible_v<T>),
                             "T must be nothrow move constructible");
            std::construct_at(&value, std::move(storage.value));
            storage.reset();
        }
    }

    ~GenericOptionalStorage() noexcept {
        reset();
    }

    [[nodiscard]] constexpr bool is_nil() const noexcept {
        return !engaged;
    }

    T &get() noexcept {
        return value;
    }

    const T &get() const noexcept {
        return value;
    }

    void reset() noexcept {
        if (engaged) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                std::destroy_at(&value);
            }
            engaged = false;
        }
    }

    template <typename... Args>
    void emplace(Args &&...args) noexcept {
        FR_STATIC_ASSERT((std::is_nothrow_constructible_v<T, Args...>),
                         "T must be nothrow constructible for Optional::emplace");
        reset();
        std::construct_at(&value, std::forward<Args>(args)...);
        engaged = true;
    }
};

} // namespace impl

/// @brief Factory function for NilTag to match some().
[[nodiscard]] constexpr NilTag none() noexcept {
    return NilTag{};
}

/**
 * @brief Container that may or may not contain a value of type T.
 *
 * If T is nillable (like a pointer), it uses NilValue trait for niche optimization.
 */
template <typename T>
class Optional {
    using Storage = std::conditional_t<IsNillable<T>, impl::NillableOptionalStorage<T>,
                                       impl::GenericOptionalStorage<T>>;

    Storage m_storage;

public:
    /// @brief Constructs an empty Optional.
    constexpr Optional() noexcept
        : m_storage() {
    }

    /// @brief Constructs an empty Optional explicitly from NilTag.
    constexpr Optional(NilTag tag) noexcept
        : m_storage(tag) {
    }

    /// @brief Implicit construction from T.
    template <typename U = T>
        requires(!std::is_same_v<std::decay_t<U>, Optional<T>> &&
                 !std::is_same_v<std::decay_t<U>, NilTag>)
    constexpr Optional(U &&v) noexcept
        : m_storage(std::forward<U>(v)) {
        FR_STATIC_ASSERT((std::is_nothrow_constructible_v<T, U>),
                         "T must be nothrow constructible from U");
    }

    Optional(const Optional &o) noexcept = default;
    Optional(Optional &&o) noexcept = default;

    Optional &operator=(const Optional &o) noexcept {
        if (this == &o) {
            return *this;
        }

        if (!o.is_nil()) {
            if (!is_nil()) {
                m_storage.get() = o.m_storage.get();
            } else {
                m_storage.emplace(o.m_storage.get());
            }
        } else {
            m_storage.reset();
        }

        return *this;
    }

    Optional &operator=(Optional &&o) noexcept {
        if (this == &o) {
            return *this;
        }

        if (!o.is_nil()) {
            if (!is_nil()) {
                m_storage.get() = std::move(o.m_storage.get());
            } else {
                m_storage.emplace(std::move(o.m_storage.get()));
            }
            o.m_storage.reset();
        } else {
            m_storage.reset();
        }

        return *this;
    }

    /// @brief Assignment from NilTag clears the Optional.
    Optional &operator=(NilTag) noexcept {
        m_storage.reset();
        return *this;
    }

    ~Optional() noexcept = default;

    /// @brief Boolean conversion for easy checks.
    [[nodiscard]] explicit operator bool() const noexcept {
        return !is_nil();
    }

    /// @brief Interface for consitancy.
    [[nodiscard]] constexpr bool is_none() const noexcept {
        return is_nil();
    }

    /// @brief Interface for consitancy.
    [[nodiscard]] constexpr bool is_some() const noexcept {
        return !is_nil();
    }

    /// @brief Static member for nil state.
    [[nodiscard]] static constexpr Optional nil() noexcept {
        return Optional(fr::none());
    }

    /// @brief Member to check if nil.
    [[nodiscard]] constexpr bool is_nil() const noexcept {
        return m_storage.is_nil();
    }

    /**
     * @brief Unwraps the value. Asserts in debug if empty.
     *
     * @return Reference to the value.
     */
    [[nodiscard]] T &unwrap() noexcept {
        FR_ASSERT(!is_nil(), "unwrap on nil");
        return m_storage.get();
    }

    /**
     * @brief Unwraps the value. Asserts in debug if empty.
     *
     * @return Constant reference to the value.
     */
    [[nodiscard]] const T &unwrap() const noexcept {
        FR_ASSERT(!is_nil(), "unwrap on nil");
        return m_storage.get();
    }

    /**
     * @brief Arrow operator — asserted.
     *
     * @return Pointer to the value.
     */
    [[nodiscard]] T *operator->() noexcept {
        FR_ASSERT(!is_nil(), "arrow on nil");
        return &m_storage.get();
    }

    /**
     * @brief Arrow operator — asserted.
     *
     * @return Constant pointer to the value.
     */
    [[nodiscard]] const T *operator->() const noexcept {
        FR_ASSERT(!is_nil(), "arrow on nil");
        return &m_storage.get();
    }

    /// @brief Returns a pointer to the value, or nullptr if empty.
    [[nodiscard]] T *ptr_or_null() noexcept {
        return !is_nil() ? &m_storage.get() : nullptr;
    }

    /// @brief Returns a pointer to the value, or nullptr if empty.
    [[nodiscard]] const T *ptr_or_null() const noexcept {
        return !is_nil() ? &m_storage.get() : nullptr;
    }

    /// @brief Returns the value if present, otherwise returns fallback.
    [[nodiscard]] T unwrap_or(T fallback) const noexcept {
        FR_STATIC_ASSERT((std::is_nothrow_copy_constructible_v<T>),
                         "T must be nothrow copy constructible for unwrap_or");

        return !is_nil() ? m_storage.get() : fallback;
    }

    /// @brief Applies a function to the contained value if present.
    template <typename F>
    [[nodiscard]] auto map(F &&f) noexcept -> Optional<std::invoke_result_t<F, T &>> {
        using FRet = std::invoke_result_t<F, T &>;
        FR_STATIC_ASSERT((std::is_nothrow_invocable_v<F, T &>), "F must be noexcept for map");

        if (!is_nil()) {
            return Optional<FRet>(std::invoke(std::forward<F>(f), m_storage.get()));
        }

        return Optional<FRet>(fr::none());
    }

    /// @brief Applies a function to the contained value if present.
    template <typename F>
    [[nodiscard]] auto map(F &&f) const noexcept -> Optional<std::invoke_result_t<F, const T &>> {
        using FRet = std::invoke_result_t<F, const T &>;
        FR_STATIC_ASSERT((std::is_nothrow_invocable_v<F, const T &>), "F must be noexcept for map");

        if (!is_nil()) {
            return Optional<FRet>(std::invoke(std::forward<F>(f), m_storage.get()));
        }

        return Optional<FRet>(fr::none());
    }

    /// @brief Constructs the value in-place.
    template <typename... Args>
    T &emplace(Args &&...args) noexcept {
        m_storage.emplace(std::forward<Args>(args)...);
        return m_storage.get();
    }

    /// @brief Clears the value.
    void reset() noexcept {
        m_storage.reset();
    }

    [[nodiscard]] bool operator==(const Optional &o) const noexcept {
        if (is_nil() != o.is_nil()) {
            return false;
        }
        if (is_nil()) {
            return true;
        }

        return m_storage.get() == o.m_storage.get();
    }

    [[nodiscard]] bool operator!=(const Optional &o) const noexcept {
        return !(*this == o);
    }

    [[nodiscard]] bool operator==(NilTag) const noexcept {
        return is_nil();
    }

    [[nodiscard]] bool operator!=(NilTag) const noexcept {
        return !is_nil();
    }
};

/// @brief Factory function for engaged Optional.
template <typename T>
[[nodiscard]] constexpr Optional<std::decay_t<T>> some(T &&v) noexcept {
    return Optional<std::decay_t<T>>(std::forward<T>(v));
}

FR_STATIC_ASSERT((sizeof(Optional<S32 *>) == sizeof(S32 *)),
                 "Optional<T*> niche optimization broken — should be pointer-sized");

} // namespace fr
