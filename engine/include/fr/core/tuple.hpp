/**
 * @file tuple.hpp
 * @author Kiju
 *
 * @brief Tuple implementation. Mostly for my own personal misery. Tuples are hard. This tuple is
 * implemented in a so-to-speak standard way, namely it uses recursive template instantiation to
 * attach indices to items. Let's say the user wants a tuple with types A, B, C:
 * Tuple<A, B, C> := TupleLeaf<0, A> + TupleLeaf<1, B> + TupleLeaf<2, C>
 * Actually there is one more level of abstraction here - TupleBase. TupleBase
 * inherits from the leaves and thus creates a sequential memory storage for the data (from index 0
 * to 2 in this case). It is my favorite pastime activity lately - abusing the C++ language.
 *
 * Exercise for an astute reader:
 * An absolute madman (and a mathematician, which shows) wrote this funky article on how to build a
 * Tuple in modern C++ abusing closures in lambdas, effectively implementing church encoding. I
 * highly recommend the read!
 * https://mcyoung.xyz/2022/07/13/tuples-the-hard-way/
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
namespace impl {

/// @brief Inheriting multiple times from the same type is not legal, so we construct a storage
/// struct parameterized by the index `I` and the type `T`.
template <USize I, typename T>
struct TupleLeaf {
    // [[no_unique_address]] enables EBO for empty element types.
    [[no_unique_address]] T value;

    constexpr TupleLeaf() noexcept
        requires(std::is_default_constructible_v<T> && std::is_nothrow_default_constructible_v<T>)
    = default;

    template <typename U>
        requires(!std::is_same_v<std::remove_cvref_t<U>, TupleLeaf> &&
                 std::is_nothrow_constructible_v<T, U>)
    constexpr explicit TupleLeaf(U &&u) noexcept
        : value(std::forward<U>(u)) {
    }
};

template <typename I, typename... Ts>
struct TupleBase;

template <USize... Is, typename... Ts>
struct TupleBase<std::index_sequence<Is...>, Ts...> : TupleLeaf<Is, Ts>... {
    constexpr TupleBase() noexcept
        requires((std::is_default_constructible_v<Ts> &&
                  std::is_nothrow_default_constructible_v<Ts>) &&
                 ...)
    = default;

    /// @brief Expands the parameter pack to construct all leaves in index order.
    template <typename... Us>
        requires(sizeof...(Us) == sizeof...(Ts))
    constexpr explicit TupleBase(Us &&...us) noexcept
        requires(std::is_nothrow_constructible_v<Ts, Us> && ...)
        : TupleLeaf<Is, Ts>(std::forward<Us>(us))... {
    }
};

template <USize I, typename... Ts>
struct pick;

template <typename T, typename... Us>
struct pick<0, T, Us...> {
    using type = T;
};

template <USize I, typename T, typename... Us>
struct pick<I, T, Us...> : pick<I - 1, Us...> {};

template <USize I, typename... Ts>
using pick_t = typename pick<I, Ts...>::type;
} // namespace impl

/** @brief Heterogeneous container with tuple protocol support.
 * @note Inherits privately from `TupleBase` and exposes a typed API through `at`, `each`, and
 * `map`.
 */
template <typename... Ts>
class Tuple : impl::TupleBase<std::index_sequence_for<Ts...>, Ts...> {
    using Base = impl::TupleBase<std::index_sequence_for<Ts...>, Ts...>;

public:
    constexpr Tuple() noexcept
        requires((std::is_default_constructible_v<Ts> &&
                  std::is_nothrow_default_constructible_v<Ts>) &&
                 ...)
    = default;

    template <typename... Us>
        requires(sizeof...(Ts) == sizeof...(Us))
    constexpr explicit Tuple(Us &&...args) noexcept
        requires(std::is_nothrow_constructible_v<Ts, Us> && ...)
        : Base(std::forward<Us>(args)...) {
    }

    /**
     * @brief Returns item at index I.
     * @note Explicit object parameters are used here to avoid duplication of the `at` method. This
     * way one method perfectly forwards all the possibilities (Tuple&, const Tuple&, Tuple&&, const
     * Tuple&&).
     */
    template <USize I>
    constexpr auto &&at(this auto &&self) noexcept {
        FR_STATIC_ASSERT(I < sizeof...(Ts),
                         "fr::Tuple::at<USize I>(this auto &&self) -> Index (I) out of bounds");

        using ItemType = impl::pick_t<I, Ts...>;
        using LeafType = impl::TupleLeaf<I, ItemType>;

        return std::forward_like<decltype(self)>(static_cast<LeafType &>(self).value);
    }

    /**
     * @brief Invokes the callback for every item in the tuple.
     * @note Invocation order matches the tuple index order.
     */
    template <typename F>
    constexpr void each(this auto &&self, F &&f) noexcept {
        [&]<USize... Is>(std::index_sequence<Is...>) {
            FR_STATIC_ASSERT(
                (std::is_invocable_v<
                     F, decltype(std::forward_like<decltype(self)>(self).template at<Is>())> &&
                 ...),
                "fr::Tuple::each(F &&f): Callback must be invocable with each element of the "
                "tuple.");
            (f(std::forward_like<decltype(self)>(self).template at<Is>()), ...);
        }(std::index_sequence_for<Ts...>{});
    }

    /**
     * @brief Invokes the callback for every item in the tuple and maps the result onto another
     * tuple.
     * @note The returned tuple element type at index `I` is the callback result type for item `I`.
     */
    template <typename F>
    constexpr auto map(this auto &&self, F &&f) noexcept {
        return [&]<USize... Is>(std::index_sequence<Is...>) {
            FR_STATIC_ASSERT(
                (std::is_invocable_v<
                     F, decltype(std::forward_like<decltype(self)>(self).template at<Is>())> &&
                 ...),
                "fr::Tuple::map(F &&f): Callback must be invocable with each element of the "
                "tuple.");
            FR_STATIC_ASSERT(
                (!std::is_void_v<std::invoke_result_t<
                     F, decltype(std::forward_like<decltype(self)>(self).template at<Is>())>> &&
                 ...),
                "fr::Tuple::map(F &&f): Callback must return a non-void value for each element of "
                "the tuple.");
            return fr::Tuple(f(std::forward_like<decltype(self)>(self).template at<Is>())...);
        }(std::index_sequence_for<Ts...>{});
    }

    USize size() const noexcept {
        return sizeof...(Ts);
    }

    /// @brief Returns the first item in the tuple.
    constexpr auto &&first(this auto &&self) noexcept {
        FR_STATIC_ASSERT(sizeof...(Ts) > 0, "fr::Tuple::first(): Tuple must not be empty.");
        return std::forward_like<decltype(self)>(self).template at<0>();
    }

    /// @brief Returns the second item in the tuple.
    constexpr auto &&second(this auto &&self) noexcept {
        FR_STATIC_ASSERT(sizeof...(Ts) > 1,
                         "fr::Tuple::second(): Tuple must have at least two elements.");
        return std::forward_like<decltype(self)>(self).template at<1>();
    }

    /// @brief Returns the last item in the tuple.
    constexpr auto &&last(this auto &&self) noexcept {
        FR_STATIC_ASSERT(sizeof...(Ts) > 0, "fr::Tuple::last(): Tuple must not be empty.");
        return std::forward_like<decltype(self)>(self).template at<sizeof...(Ts) - 1>();
    }

    /// @brief This annotation is needed for the tuple protocol to work. It is declared utilizing
    /// the hidden friend idiom.
    template <USize I>
    friend constexpr auto &&get(Tuple &self) noexcept {
        return self.template at<I>();
    }

    /// @brief This annotation is needed for the tuple protocol to work. It is declared utilizing
    /// the hidden friend idiom.
    template <USize I>
    friend constexpr auto &&get(const Tuple &self) noexcept {
        return self.template at<I>();
    }

    /// @brief This annotation is needed for the tuple protocol to work. It is declared utilizing
    /// the hidden friend idiom.
    template <USize I>
    friend constexpr auto &&get(Tuple &&self) noexcept {
        return std::move(self).template at<I>();
    }

    /// @brief This annotation is needed for the tuple protocol to work. It is declared utilizing
    /// the hidden friend idiom.
    template <USize I>
    friend constexpr auto &&get(const Tuple &&self) noexcept {
        return std::move(self).template at<I>();
    }
};

/// @brief Deduction guide, so the user can write Tuple(42, 0.42f)
/// instead of Tuple<U32, F32>(42, 0.42f).
template <typename... Ts>
Tuple(Ts...) -> Tuple<Ts...>;
} // namespace fr

// These specializations are part of the tuple protocol and enable structured bindings.
namespace std {
template <typename... Ts>
struct tuple_size<fr::Tuple<Ts...>> : std::integral_constant<USize, sizeof...(Ts)> {};

template <USize I, typename... Ts>
struct tuple_element<I, fr::Tuple<Ts...>> {
    using type = fr::impl::pick_t<I, Ts...>;
};
} // namespace std
