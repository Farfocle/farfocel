/**
 * @file inline_any.hpp
 * @author Kiju
 *
 * @brief InlineAny represents a type-erased value that is stored inline - without any heap
 * allocation.
 */

#pragma once

#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief InlineAny represents a type-erased value that is stored inline - without any heap
 * allocation.
 *
 * @note sizeof(InlineAny) == Capacity + sizeof(void *)
 */
template <USize Capacity, USize Alignment>
struct InlineAny {
    FR_STATIC_ASSERT(Capacity > 0, "capacity must be greater than 0");
    FR_STATIC_ASSERT((Alignment & (Alignment - 1)) == 0, "alignment must be a power of 2");

public:
    InlineAny() noexcept = default;

    /**
     * @brief Constructs an InlineAny with the given value.
     * @tparam T The type of the value to store.
     * @param value The value to store.
     */
    template <typename T>
        requires(!std::is_same_v<std::decay_t<T>, InlineAny>)
    InlineAny(T &&value) noexcept {
        using DT = std::decay_t<T>;
        FR_STATIC_ASSERT_NOTHROW_BASE(DT);
        FR_STATIC_ASSERT(sizeof(DT) <= Capacity, "value is too large for InlineAny");
        FR_STATIC_ASSERT(alignof(DT) <= Alignment, "alignment is too small for value");

        std::construct_at(std::launder(reinterpret_cast<DT *>(m_storage)), std::forward<T>(value));

        m_handler = [](HandlerAction action, Byte *dst, Byte *src) {
            DT *dst_t = std::launder(reinterpret_cast<DT *>(dst));
            const DT *src_const = src ? std::launder(reinterpret_cast<const DT *>(src)) : nullptr;
            DT *src_t = src ? std::launder(reinterpret_cast<DT *>(src)) : nullptr;

            switch (action) {
            case HandlerAction::Destroy:
                std::destroy_at(dst_t);
                break;
            case HandlerAction::Copy:
                if constexpr (std::is_copy_constructible_v<DT>) {
                    std::construct_at(dst_t, *src_const);
                } else {
                    FR_ASSERT(false, "InlineAny stored type is not copy-constructible");
                }
                break;
            case HandlerAction::Move:
                if constexpr (std::is_move_constructible_v<DT>) {
                    std::construct_at(dst_t, std::move(*src_t));
                } else {
                    FR_ASSERT(false, "InlineAny stored type is not move-constructible");
                }
                break;
            }
        };
    }

    ~InlineAny() noexcept {
        if (m_handler) [[likely]] {
            m_handler(HandlerAction::Destroy, m_storage, nullptr);
        }
    }

    /**
     * @brief Copies the value from another InlineAny.
     * @param other The InlineAny to copy from.
     * @note Copy preserves the stored type and value using its copy constructor.
     */
    InlineAny(const InlineAny &other) noexcept {
        if (other.m_handler) [[likely]] {
            m_handler = other.m_handler;

            /// @safety The source storage is const-casted to avoid modifying the source value.
            m_handler(HandlerAction::Copy, m_storage, const_cast<Byte *>(other.m_storage));
        } else {
            m_handler = nullptr;
        }
    }

    /**
     * @brief Moves the value from another InlineAny.
     * @param other The InlineAny to move from.
     * @note Move preserves the stored type and uses its move constructor.
     */
    InlineAny(InlineAny &&other) noexcept {
        if (other.m_handler) [[likely]] {
            m_handler = other.m_handler;
            m_handler(HandlerAction::Move, m_storage, other.m_storage);
        } else {
            m_handler = nullptr;
        }
    }

    /**
     * @brief Copy-assigns from another InlineAny.
     * @param other The InlineAny to copy from.
     * @note Overwrites the current value.
     */
    InlineAny &operator=(const InlineAny &other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (m_handler) [[likely]] {
            m_handler(HandlerAction::Destroy, m_storage, nullptr);
        }

        if (other.m_handler) [[likely]] {
            m_handler = other.m_handler;
            m_handler(HandlerAction::Copy, m_storage, other.m_storage);
        } else {
            m_handler = nullptr;
        }

        return *this;
    }

    /**
     * @brief Move-assigns from another InlineAny.
     * @param other The InlineAny to move from.
     * @note Overwrites the current value.
     */
    InlineAny &operator=(InlineAny &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (m_handler) [[likely]] {
            m_handler(HandlerAction::Destroy, m_storage, nullptr);
        }

        if (other.m_handler) [[likely]] {
            m_handler = other.m_handler;
            m_handler(HandlerAction::Move, m_storage, other.m_storage);
        } else {
            m_handler = nullptr;
        }

        return *this;
    }

    /**
     * @brief Returns a nil InlineAny.
     * @return A nil InlineAny.
     */
    static InlineAny nil() noexcept {
        return InlineAny();
    }

    /**
     * @brief Returns whether the InlineAny is nil.
     * @return True if the InlineAny is nil, false otherwise.
     */
    bool is_nil() const noexcept {
        return m_handler == nullptr;
    }

    /**
     * @brief Casts the value to the specified type.
     * @tparam T The type to cast to.
     * @return A reference to the casted value.
     * @note No runtime type checking is performed.
     */
    template <typename T>
    T &cast() noexcept {
        FR_STATIC_ASSERT(sizeof(T) <= Capacity, "value is too large for InlineAny");
        FR_STATIC_ASSERT(alignof(T) <= Alignment, "alignment is too small for value");

        return *std::launder(reinterpret_cast<T *>(m_storage));
    }

private:
    enum class HandlerAction : U8 {
        Destroy,
        Copy,
        Move,
    };

    alignas(Alignment) Byte m_storage[Capacity];
    void (*m_handler)(HandlerAction, Byte *, Byte *){nullptr};
};
} // namespace fr
