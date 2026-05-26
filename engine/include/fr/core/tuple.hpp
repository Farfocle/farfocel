/**
 * @file tuple.hpp
 * @author Kiju
 *
 * @brief Tuple implementation.
 *
 * Tuple provides a heterogeneous container with full tuple protocol and structured binding support.
 */

#pragma once

#include <type_traits>
#include <utility>

#include "fr/core/macros.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/typetraits.hpp"

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

template <typename A, typename TupleT, USize... Is>
void shape_tuple_items(A &archive, TupleT &value, std::index_sequence<Is...>) noexcept {
    (archive.prop("", value.template at<Is>()), ...);
}
} // namespace impl

/**
 * @brief Heterogeneous container with tuple protocol support.
 *
 * @note Foundational requirements for all Ts are enforced via FR_STATIC_ASSERT_NOTHROW_BASE.
 */
template <typename... Ts>
class Tuple : impl::TupleBase<std::index_sequence_for<Ts...>, Ts...> {
    static_assert((IsNothrowBase<Ts> && ...),
                  "All Ts must satisfy foundational nothrow requirements");

    using Base = impl::TupleBase<std::index_sequence_for<Ts...>, Ts...>;

public:
    /**
     * @brief Construct a tuple with default-initialized elements.
     * @pre All Ts must be nothrow default constructible.
     */
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
     */
    template <USize I>
    constexpr auto &&at(this auto &&self) noexcept {
        FR_STATIC_ASSERT(I < sizeof...(Ts), "index out of bounds");

        using ItemType = impl::pick_t<I, Ts...>;
        using LeafType = impl::TupleLeaf<I, ItemType>;

        if constexpr (std::is_reference_v<ItemType>) {
            return static_cast<LeafType &>(self).value;
        } else {
            if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
                return std::forward_like<decltype(self)>(static_cast<const LeafType &>(self).value);
            } else {
                return std::forward_like<decltype(self)>(static_cast<LeafType &>(self).value);
            }
        }
    }

    /**
     * @brief Invokes the callback for every item in the tuple.
     */
    template <typename F>
    constexpr void each(this auto &&self, F &&f) noexcept {
        [&]<USize... Is>(std::index_sequence<Is...>) {
            static_assert((std::is_invocable_v<F, decltype(std::forward_like<decltype(self)>(self)
                                                               .template at<Is>())> &&
                           ...),
                          "callback not invocable");
            (f(std::forward_like<decltype(self)>(self).template at<Is>()), ...);
        }(std::index_sequence_for<Ts...>{});
    }

    /**
     * @brief Invokes the callback for every item in the tuple and maps the result onto another
     * tuple.
     */
    template <typename F>
    constexpr auto map(this auto &&self, F &&f) noexcept {
        return [&]<USize... Is>(std::index_sequence<Is...>) {
            static_assert((std::is_invocable_v<F, decltype(std::forward_like<decltype(self)>(self)
                                                               .template at<Is>())> &&
                           ...),
                          "callback not invocable");
            static_assert(
                (!std::is_void_v<std::invoke_result_t<
                     F, decltype(std::forward_like<decltype(self)>(self).template at<Is>())>> &&
                 ...),
                "callback returns void");
            return fr::Tuple(f(std::forward_like<decltype(self)>(self).template at<Is>())...);
        }(std::index_sequence_for<Ts...>{});
    }

    constexpr USize size() const noexcept {
        return sizeof...(Ts);
    }

    /// @brief Returns the first item in the tuple.
    constexpr auto &&first(this auto &&self) noexcept {
        FR_STATIC_ASSERT(sizeof...(Ts) > 0, "empty tuple");
        return std::forward_like<decltype(self)>(self).template at<0>();
    }

    /// @brief Returns the second item in the tuple.
    constexpr auto &&second(this auto &&self) noexcept {
        FR_STATIC_ASSERT(sizeof...(Ts) > 1, "tuple too small");
        return std::forward_like<decltype(self)>(self).template at<1>();
    }

    /// @brief Returns the last item in the tuple.
    constexpr auto &&last(this auto &&self) noexcept {
        FR_STATIC_ASSERT(sizeof...(Ts) > 0, "empty tuple");
        return std::forward_like<decltype(self)>(self).template at<sizeof...(Ts) - 1>();
    }

    template <typename A>
    void shape(A &archive) {
        USize sz = size();

        archive.prop("@size", sz);
        archive.list("@items", [&](A &list_archive) {
            impl::shape_tuple_items(list_archive, *this, std::index_sequence_for<Ts...>{});
        });
    }

    template <typename A>
    void shape(A &archive) const {
        if constexpr (A::action == ArchiveAction::Write) {
            USize sz = size();

            archive.prop("@size", sz);
            archive.list("@items", [&](A &list_archive) {
                impl::shape_tuple_items(list_archive, *this, std::index_sequence_for<Ts...>{});
            });
        } else {
            FR_ASSERT(false, "cannot deserialize into const Tuple");
        }
    }

    template <USize I>
    friend constexpr auto &&get(Tuple &self) noexcept {

        return self.template at<I>();
    }

    template <USize I>
    friend constexpr auto &&get(const Tuple &self) noexcept {
        return self.template at<I>();
    }

    template <USize I>
    friend constexpr auto &&get(Tuple &&self) noexcept {
        return std::move(self).template at<I>();
    }

    template <USize I>
    friend constexpr auto &&get(const Tuple &&self) noexcept {
        return std::move(self).template at<I>();
    }
};

/// @brief Deduction guide, so the user can write Tuple(42, 0.42f)
template <typename... Ts>
Tuple(Ts...) -> Tuple<Ts...>;
} // namespace fr

// std specializations for structured bindings
namespace std {
template <typename... Ts>
struct tuple_size<fr::Tuple<Ts...>> : std::integral_constant<USize, sizeof...(Ts)> {};

template <USize I, typename... Ts>
struct tuple_element<I, fr::Tuple<Ts...>> {
    using type = fr::impl::pick_t<I, Ts...>;
};
} // namespace std
