/**
 * @file arena_allocator.hpp
 * @author Kiju
 *
 * @brief Fixed-size arena allocator over caller-provided memory buffer.
 */

#pragma once

#include <cstdio>
#include <cstring>

#include "fr/core/alloc.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Non-owning, non-growing arena allocator.
 *
 * This allocator can only exist in a valid, non-null state.
 */
class ArenaAlloc final : public Alloc {
public:
    /**
     * @brief Construct an arena allocator.
     *
     * @param buffer Memory buffer to use.
     * @param buffer_sz Size of the buffer in bytes.
     * @param tag Custom tag for debugging.
     */
    ArenaAlloc(void *buffer, USize buffer_sz, const char *tag = "@noname") noexcept
        : m_buffer(static_cast<Byte *>(buffer)),
          m_buffer_size(buffer_sz),
          m_custom_tag(tag) {

        FR_ASSERT(buffer, "buffer must be non-null");
        FR_ASSERT(buffer_sz > 0, "size must be non-zero");

        std::snprintf(m_full_tag, sizeof(m_full_tag), "ArenaAllocator: %s", m_custom_tag);
    }

    /**
     * @brief Reset the arena to its initial empty state.
     */
    void reset() noexcept {
        m_offset = 0;
    }

    /**
     * @brief Returns the total capacity of the arena.
     *
     * @return Capacity in bytes.
     */
    [[nodiscard]] USize capacity() const noexcept {
        return m_buffer_size;
    }

    /**
     * @brief Returns the number of bytes used in the arena.
     *
     * @return Used bytes.
     */
    [[nodiscard]] USize used() const noexcept {
        return m_offset;
    }

    /**
     * @brief Returns the number of bytes remaining in the arena.
     *
     * @return Remaining bytes.
     */
    [[nodiscard]] USize remaining() const noexcept {
        return m_buffer_size - m_offset;
    }

    /**
     * @brief Checks if a pointer is owned by this arena.
     *
     * @param ptr Pointer to check.
     * @return Ownership result.
     */
    OwnershipResult owns(void *ptr) const noexcept override {
        Byte *byte_ptr = static_cast<Byte *>(ptr);
        return (byte_ptr >= m_buffer && byte_ptr < (m_buffer + m_buffer_size))
                   ? OwnershipResult::Owns
                   : OwnershipResult::DoesNotOwn;
    }

    /**
     * @brief Returns the allocator tag.
     *
     * @return Tag string.
     */
    const char *tag() const noexcept override {
        return m_full_tag;
    }

protected:
    /**
     * @brief Internal allocation logic for arena.
     *
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated memory or nullptr.
     */
    void *do_try_allocate(USize sz, USize alignment) noexcept override {
        alignment = mem::normalize_alignment(alignment);
        USize padding = mem::align_forward_padding(m_buffer + m_offset, alignment);

        if (padding > m_buffer_size - m_offset || sz > m_buffer_size - m_offset - padding) {
            return nullptr;
        }

        USize aligned_offset = m_offset + padding;
        void *result = static_cast<void *>(m_buffer + aligned_offset);
        m_offset = aligned_offset + sz;

        return result;
    }

    /**
     * @brief Internal reallocation logic for arena.
     *
     * @param ptr Pointer to existing allocation.
     * @param old_sz Old size in bytes.
     * @param new_sz New size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to reallocated memory or nullptr.
     */
    void *do_try_reallocate(void *ptr, USize old_sz, USize new_sz,
                            USize alignment) noexcept override {
        alignment = mem::normalize_alignment(alignment);

        Byte *byte_ptr = static_cast<Byte *>(ptr);
        if (owns(ptr) != OwnershipResult::Owns) {
            return nullptr;
        }

        const USize ptr_offset = static_cast<USize>(byte_ptr - m_buffer);

        if (ptr_offset + old_sz == m_offset) {
            USize padding = mem::align_forward_padding(ptr, alignment);
            if (padding == 0) {
                if (new_sz > (m_buffer_size - ptr_offset)) {
                    return nullptr;
                }

                m_offset = ptr_offset + new_sz;
                return ptr;
            }

            m_offset = ptr_offset;
            void *new_ptr = do_try_allocate(new_sz, alignment);
            if (!new_ptr) {
                m_offset = ptr_offset + old_sz;
                return nullptr;
            }

            std::memmove(new_ptr, ptr, old_sz < new_sz ? old_sz : new_sz);
            return new_ptr;
        }

        if (new_sz <= old_sz && mem::align_forward_padding(ptr, alignment) == 0) {
            return ptr;
        }

        void *new_ptr = do_try_allocate(new_sz, alignment);
        if (!new_ptr) {
            return nullptr;
        }

        std::memcpy(new_ptr, ptr, old_sz < new_sz ? old_sz : new_sz);
        return new_ptr;
    }

    /**
     * @brief Internal deallocation logic (no-op for arena).
     *
     * @param ptr Pointer to free.
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     * @note Arena allocations are reclaimed via reset(), not individual frees.
     */
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
