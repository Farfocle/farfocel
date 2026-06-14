/**
 * @file inline_function.hpp
 * @author Kiju
 *
 * @brief InlineFunction is a fixed-size function wrapper with inline storage.
 * @todo Create a global meta information lookup.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

template <typename Signature, USize Capacity, USize Alignment = alignof(std::max_align_t)>
class InlineFunction;

/**
 * @brief Fixed-size callable wrapper for function signatures.
 *
 * @tparam R Return type.
 * @tparam Args Argument pack.
 * @tparam Capacity Inline storage capacity in bytes.
 * @tparam Alignment Inline storage alignment.
 */
template <typename R, typename... Args, USize Capacity, USize Alignment>
class InlineFunction<R(Args...), Capacity, Alignment> {
    FR_STATIC_ASSERT(Capacity > 0, "capacity must be greater than 0");
    FR_STATIC_ASSERT((Alignment & (Alignment - 1)) == 0, "alignment must be a power of 2");

    struct Ops {
        R (*invoke)(Byte *, Args &&...);
        void (*copy)(Byte *, const Byte *);
        void (*move)(Byte *, Byte *);
        void (*destroy)(Byte *);
    };

    template <typename F>
    struct OpsHolder {
        static R invoke(Byte *storage, Args &&...args) {
            F *f = std::launder(reinterpret_cast<F *>(storage));
            return (*f)(std::forward<Args>(args)...);
        }

        static void copy(Byte *dst, const Byte *src) {
            const F *src_f = std::launder(reinterpret_cast<const F *>(src));
            std::construct_at(std::launder(reinterpret_cast<F *>(dst)), *src_f);
        }

        static void move(Byte *dst, Byte *src) {
            F *src_f = std::launder(reinterpret_cast<F *>(src));
            std::construct_at(std::launder(reinterpret_cast<F *>(dst)), std::move(*src_f));
        }

        static void destroy(Byte *storage) {
            std::destroy_at(std::launder(reinterpret_cast<F *>(storage)));
        }

        static constexpr Ops ops{
            &invoke,
            &copy,
            &move,
            &destroy,
        };
    };

public:
    InlineFunction() noexcept = default;

    template <typename F>
        requires(!std::is_same_v<std::decay_t<F>, InlineFunction>)
    InlineFunction(F &&callable) noexcept {
        emplace(std::forward<F>(callable));
    }

    InlineFunction(const InlineFunction &other) noexcept {
        if (other.m_ops) {
            m_ops = other.m_ops;
            m_ops->copy(m_storage, other.m_storage);
        }
    }

    InlineFunction(InlineFunction &&other) noexcept {
        if (other.m_ops) {
            m_ops = other.m_ops;
            m_ops->move(m_storage, other.m_storage);
            other.m_ops->destroy(other.m_storage);
            other.m_ops = nullptr;
        }
    }

    ~InlineFunction() noexcept {
        reset();
    }

    InlineFunction &operator=(const InlineFunction &other) noexcept {
        if (this == &other) {
            return *this;
        }

        reset();
        if (other.m_ops) {
            m_ops = other.m_ops;
            m_ops->copy(m_storage, other.m_storage);
        }
        return *this;
    }

    InlineFunction &operator=(InlineFunction &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        reset();
        if (other.m_ops) {
            m_ops = other.m_ops;
            m_ops->move(m_storage, other.m_storage);
            other.m_ops->destroy(other.m_storage);
            other.m_ops = nullptr;
        }
        return *this;
    }

    template <typename F>
        requires(!std::is_same_v<std::decay_t<F>, InlineFunction>)
    InlineFunction &operator=(F &&callable) noexcept {
        emplace(std::forward<F>(callable));
        return *this;
    }

    /**
     * @brief Replaces the stored callable.
     */
    template <typename F>
        requires(!std::is_same_v<std::decay_t<F>, InlineFunction>)
    void emplace(F &&callable) noexcept {
        using DF = std::decay_t<F>;
        FR_STATIC_ASSERT(sizeof(DF) <= Capacity, "callable is too large for InlineFunction");
        FR_STATIC_ASSERT(alignof(DF) <= Alignment, "alignment is too small for callable");

        reset();
        std::construct_at(std::launder(reinterpret_cast<DF *>(m_storage)),
                          std::forward<F>(callable));
        m_ops = &OpsHolder<DF>::ops;
    }

    /**
     * @brief Clears the stored callable.
     */
    void reset() noexcept {
        if (m_ops) {
            m_ops->destroy(m_storage);
            m_ops = nullptr;
        }
    }

    /**
     * @brief Returns true when no callable is stored.
     */
    bool is_nil() const noexcept {
        return m_ops == nullptr;
    }

    explicit operator bool() const noexcept {
        return !is_nil();
    }

    /**
     * @brief Invokes the stored callable.
     * @warning Asserts if no callable is stored.
     */
    R operator()(Args... args) noexcept {
        FR_ASSERT(m_ops != nullptr, "cannot call a nil InlineFunction");
        return m_ops->invoke(m_storage, std::forward<Args>(args)...);
    }

    /**
     * @brief Invokes the stored callable.
     * @warning Asserts if no callable is stored.
     */
    R operator()(Args... args) const noexcept {
        FR_ASSERT(m_ops != nullptr, "cannot call a nil InlineFunction");
        return m_ops->invoke(const_cast<Byte *>(m_storage), std::forward<Args>(args)...);
    }

private:
    alignas(Alignment) Byte m_storage[Capacity]{};
    const Ops *m_ops{nullptr};
};

template <typename S>
using Fn32 = InlineFunction<S, 32>;

template <typename S>
using Fn64 = InlineFunction<S, 64>;

template <typename S>
using Fn96 = InlineFunction<S, 96>;

template <typename S>
using Fn128 = InlineFunction<S, 128>;

template <typename S>
using Fn512 = InlineFunction<S, 512>;
} // namespace fr
