/**
 * @file pair.hpp
 * @author Kiju
 *
 * @brief Simple and lightweight two-element container.
 *
 * @details Pair exists because fr::Tuple couses a lot of compile time computation resulting in slow
 * compilation times. Pair is much simpler, thus faster to compile, it only supports 2 type slots.
 * This implementaions supports structured bindings and all that nice syntax sugar.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/hash.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief A lean, mean, two-element machine.
 *
 * This is a zero-cost abstraction for holding a duo of types. It supports the
 * full tuple protocol, meaning you can use structured bindings like `auto [a, b] = my_pair;`.
 */
template <typename First, typename Second>
class Pair {
public:
    /// @brief Default constructor. Value-initializes members to keep things sane.
    constexpr Pair() noexcept
        requires(std::is_nothrow_default_constructible_v<First> &&
                 std::is_nothrow_default_constructible_v<Second>)
        : m_first{},
          m_second{} {
    }

    /**
     * @brief Construct a pair from two values.
     * @param first The first element.
     * @param second The second element.
     */
    template <typename A = First, typename B = Second>
    explicit(!std::is_convertible_v<A, First> ||
             !std::is_convertible_v<B, Second>) constexpr Pair(A &&first, B &&second) noexcept
        requires(std::is_nothrow_constructible_v<First, A> &&
                 std::is_nothrow_constructible_v<Second, B>)
        : m_first(std::forward<A>(first)),
          m_second(std::forward<B>(second)) {
    }

public:
    /// @brief Access the first element.
    constexpr First &first() noexcept {
        return m_first;
    }

    /// @brief Access the first element (read-only).
    constexpr const First &first() const noexcept {
        return m_first;
    }

    /// @brief Access the second element.
    constexpr Second &second() noexcept {
        return m_second;
    }

    /// @brief Access the second element (read-only).
    constexpr const Second &second() const noexcept {
        return m_second;
    }

    /**
     * @brief Access item at index I (0 or 1).
     *
     * @tparam I Index to access.
     * @return Reference to the element.
     */
    template <USize I>
    constexpr auto &&at(this auto &&self) noexcept {
        FR_STATIC_ASSERT(I < 2, "index out of bounds");

        if constexpr (I == 0) {
            return std::forward_like<decltype(self)>(self.m_first);
        } else {
            return std::forward_like<decltype(self)>(self.m_second);
        }
    }

    /**
     * @brief Returns a hash of the pair.
     */
    constexpr Hash hash() const noexcept {
        return combine_hashes(call_hash(m_first), call_hash(m_second));
    }

    /// @brief Structured binding protocol.
    template <USize I>
    friend constexpr auto &&get(Pair &self) noexcept {
        return self.template at<I>();
    }

    /// @brief Structured binding support (const).
    template <USize I>
    friend constexpr auto &&get(const Pair &self) noexcept {
        return self.template at<I>();
    }

    /// @brief Structured binding support (move).
    template <USize I>
    friend constexpr auto &&get(Pair &&self) noexcept {
        return std::move(self).template at<I>();
    }

    /// @brief Structured binding support (const move).
    template <USize I>
    friend constexpr auto &&get(const Pair &&self) noexcept {
        return std::move(self).template at<I>();
    }

private:
    First m_first;
    Second m_second;
};

/// @brief Deduction guide so you don't have to repeat yourself.
template <typename First, typename Second>
Pair(First, Second) -> Pair<First, Second>;

} // namespace fr

// Inject into std namespace to satisfy the structured bindings protocol.
namespace std {
template <typename First, typename Second>
struct tuple_size<fr::Pair<First, Second>> : std::integral_constant<USize, 2> {};

template <USize Idx, typename First, typename Second>
struct tuple_element<Idx, fr::Pair<First, Second>> {
    using type = std::conditional_t<Idx == 0, First, Second>;
};
} // namespace std
