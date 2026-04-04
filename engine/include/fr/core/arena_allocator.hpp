/**
 * @file arena_allocator.hpp
 * @author Kiju
 *
 * @brief Fixed-capacity arena allocator over caller-provided memory.
 */

#pragma once

#include <cstring>

#include "fr/core/allocator.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Non-owning, non-growing arena allocator.
class ArenaAllocator final : public Allocator {
public:
    ArenaAllocator(void *buffer, USize capacity) noexcept
        : m_buffer(static_cast<Byte *>(buffer)),
          m_capacity(capacity) {
        FR_ASSERT(buffer != nullptr || capacity == 0,
                  "fr::ArenaAllocator(void *buffer, USize capacity): Buffer must be non-null when "
                  "capacity is non-zero");
    }

    /// @brief Reset the arena to its initial empty state.
    void reset() noexcept {
        m_offset = 0;
    }

    [[nodiscard]] USize capacity() const noexcept {
        return m_capacity;
    }

    [[nodiscard]] USize used() const noexcept {
        return m_offset;
    }

    [[nodiscard]] USize remaining() const noexcept {
        return m_capacity - m_offset;
    }

    OwnershipResult debug_owns(void *ptr) const noexcept override {
        if (!ptr || m_capacity == 0) {
            return OwnershipResult::DoesNotOwn;
        }

        Byte *byte_ptr = static_cast<Byte *>(ptr);
        return (byte_ptr >= m_buffer && byte_ptr < (m_buffer + m_capacity))
                   ? OwnershipResult::Owns
                   : OwnershipResult::DoesNotOwn;
    }

    const char *debug_tag() const noexcept override {
        return "ArenaAllocator";
    }

protected:
    void *do_try_allocate(USize sz, USize alignment) noexcept override {
        alignment = mem::normalize_alignment(alignment);
        USize aligned_offset = (m_offset + (alignment - 1)) & ~(alignment - 1);

        if (aligned_offset > m_capacity || sz > (m_capacity - aligned_offset)) {
            return nullptr;
        }

        void *result = static_cast<void *>(m_buffer + aligned_offset);
        m_offset = aligned_offset + sz;

        return result;
    }

    void *do_try_reallocate(void *ptr, USize old_sz, USize new_sz,
                            USize alignment) noexcept override {
        alignment = mem::normalize_alignment(alignment);

        Byte *byte_ptr = static_cast<Byte *>(ptr);
        if (debug_owns(ptr) != OwnershipResult::Owns) {
            return nullptr;
        }

        const USize ptr_offset = static_cast<USize>(byte_ptr - m_buffer);
        if (ptr_offset + old_sz == m_offset) {
            if (ptr_offset > m_capacity || new_sz > (m_capacity - ptr_offset)) {
                return nullptr;
            }

            m_offset = ptr_offset + new_sz;
            return ptr;
        }

        void *new_ptr = do_try_allocate(new_sz, alignment);
        if (!new_ptr) {
            return nullptr;
        }

        std::memcpy(new_ptr, ptr, old_sz < new_sz ? old_sz : new_sz);
        return new_ptr;
    }

    /// @note Arena allocations are reclaimed via reset(), not individual frees.
    void do_deallocate(void * /*ptr*/, USize /*sz*/, USize /*alignment*/) noexcept override {
    }

private:
    Byte *m_buffer{nullptr};
    USize m_capacity{0};
    USize m_offset{0};
};
} // namespace fr
