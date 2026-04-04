/**
 * @file arena_allocator.hpp
 * @author Kiju
 *
 * @brief Fixed-size arena allocator over caller-provided memory buffer.
 */

#pragma once

#include <cstdio>
#include <cstring>

#include "fr/core/allocator.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Non-owning, non-growing arena allocator. This allocator can only exist in a valid,
/// non-null state.
class ArenaAllocator final : public Allocator {
public:
    ArenaAllocator(void *buffer, USize buffer_sz, const char *tag = "@noname") noexcept
        : m_buffer(static_cast<Byte *>(buffer)),
          m_buffer_size(buffer_sz),
          m_custom_tag(tag) {

        FR_ASSERT(
            buffer,
            "fr::ArenaAllocator(void *buffer, USize buffer_sz, const char *tag = \"@noname\"): "
            "Buffer must be non-null");

        FR_ASSERT(
            buffer_sz > 0,
            "fr::ArenaAllocator(void *buffer, USize buffer_sz, const char *tag = \"@noname\"): "
            "Buffer size must be non-zero");

        std::snprintf(m_full_tag, sizeof(m_full_tag), "ArenaAllocator: %s", m_custom_tag);
    }

    /// @brief Reset the arena to its initial empty state.
    void reset() noexcept {
        m_offset = 0;
    }

    [[nodiscard]] USize capacity() const noexcept {
        return m_buffer_size;
    }

    [[nodiscard]] USize used() const noexcept {
        return m_offset;
    }

    [[nodiscard]] USize remaining() const noexcept {
        return m_buffer_size - m_offset;
    }

    OwnershipResult owns(void *ptr) const noexcept override {
        Byte *byte_ptr = static_cast<Byte *>(ptr);
        return (byte_ptr >= m_buffer && byte_ptr < (m_buffer + m_buffer_size))
                   ? OwnershipResult::Owns
                   : OwnershipResult::DoesNotOwn;
    }

    const char *tag() const noexcept override {
        return m_full_tag;
    }

protected:
    void *do_try_allocate(USize sz, USize alignment) noexcept override {
        alignment = mem::normalize_alignment(alignment);
        USize aligned_offset = (m_offset + (alignment - 1)) & ~(alignment - 1);

        if (aligned_offset > m_buffer_size || sz > (m_buffer_size - aligned_offset)) {
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
        if (owns(ptr) != OwnershipResult::Owns) {
            return nullptr;
        }

        const USize ptr_offset = static_cast<USize>(byte_ptr - m_buffer);

        if (ptr_offset + old_sz == m_offset) {
            if (new_sz > (m_buffer_size - ptr_offset)) {
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
    USize m_buffer_size{0};
    USize m_offset{0};
    const char *m_custom_tag{""};
    char m_full_tag[64]{};
};
} // namespace fr
