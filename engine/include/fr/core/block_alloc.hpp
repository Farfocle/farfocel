/**
 * @file block_allocator.hpp
 * @author Kiju
 *
 * @brief Fixed-size pool allocator over caller-provided memory buffer.
 */

#pragma once

#include <cstdio>

#include "fr/core/alloc.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Pool allocator managing equal-sized blocks.
 *
 * This allocator can only exist in a valid, non-null state.
 */
class BlockAlloc final : public Alloc {
public:
    /**
     * @brief Construct a block allocator.
     *
     * @param buffer Memory buffer to use.
     * @param buffer_sz Size of the buffer in bytes.
     * @param block_sz Size of each block in bytes.
     * @param custom_tag Custom tag for debugging.
     */
    BlockAlloc(void *buffer, USize buffer_sz, USize block_sz,
               const char *custom_tag = "@noname") noexcept
        : m_buffer(static_cast<Byte *>(buffer)),
          m_buffer_size(buffer_sz),
          m_block_size(std::max(block_sz, sizeof(void *))),
          m_custom_tag(custom_tag) {

        FR_ASSERT(buffer, "buffer must be non-null");
        FR_ASSERT(buffer_sz > 0, "size must be non-zero");
        FR_ASSERT(buffer_sz >= m_block_size, "buffer too small");

        std::snprintf(m_full_tag, sizeof(m_full_tag), "BlockAllocator: %s", m_custom_tag);

        reset();
    }

    /**
     * @brief Resets the allocator and rebuilds the free list.
     */
    void reset() noexcept {
        USize n = total_blocks();
        m_free_count = n;
        m_head = reinterpret_cast<FreeBlock *>(m_buffer);

        FreeBlock *current = m_head;

        for (USize i = 0; i < n - 1; ++i) {
            Byte *next_addr = reinterpret_cast<Byte *>(current) + m_block_size;
            current->next = reinterpret_cast<FreeBlock *>(next_addr);
            current = current->next;
        }

        current->next = nullptr;
    }

    /**
     * @brief Returns the total size of the managed buffer.
     *
     * @return Buffer size in bytes.
     */
    [[nodiscard]] USize buffer_size() const noexcept {
        return m_buffer_size;
    }

    /**
     * @brief Returns the size of a single block.
     *
     * @return Block size in bytes.
     */
    [[nodiscard]] USize block_size() const noexcept {
        return m_block_size;
    }

    /**
     * @brief Returns the total number of blocks in the pool.
     *
     * @return Total blocks.
     */
    [[nodiscard]] USize total_blocks() const noexcept {
        return m_buffer_size / m_block_size;
    }

    /**
     * @brief Returns the number of free blocks currently available.
     *
     * @return Free blocks.
     */
    [[nodiscard]] USize free_blocks() const noexcept {
        return m_free_count;
    }

    /**
     * @brief Returns the number of used blocks.
     *
     * @return Used blocks.
     */
    [[nodiscard]] USize used_blocks() const noexcept {
        return total_blocks() - m_free_count;
    }

    /**
     * @brief Checks if a pointer is owned by this block allocator.
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
     * @brief Internal allocation logic for block allocator.
     *
     * @param sz Size in bytes (ignored, uses fixed block size).
     * @param alignment Alignment in bytes.
     * @return Pointer to allocated block or nullptr.
     */
    void *do_try_allocate(USize /*sz*/, USize alignment) noexcept override {
        if (!m_head) {
            return nullptr;
        }

        void *ptr = m_head;
        if (mem::align_forward_padding(ptr, alignment) != 0) {
            FR_PANIC("block alignment mismatch");
            return nullptr;
        }

        m_head = m_head->next;
        --m_free_count;

        return ptr;
    }

    /**
     * @brief Internal reallocation logic for block allocator.
     *
     * @param ptr Pointer to existing allocation.
     * @param old_sz Old size in bytes.
     * @param new_sz New size in bytes.
     * @param alignment Alignment in bytes.
     * @return Pointer to reallocated block or nullptr.
     */
    void *do_try_reallocate(void *ptr, USize /*old_sz*/, USize new_sz,
                            USize alignment) noexcept override {
        FR_ASSERT(owns(ptr) == OwnershipResult::Owns, "unowned pointer");

        if (new_sz <= m_block_size && mem::align_forward_padding(ptr, alignment) == 0) {
            return ptr;
        }

        return nullptr;
    }

    /**
     * @brief Internal deallocation logic for block allocator.
     *
     * @param ptr Pointer to free.
     * @param sz Size in bytes.
     * @param alignment Alignment in bytes.
     */
    void do_deallocate(void *ptr, USize /*sz*/, USize /*alignment*/) noexcept override {
        if (!ptr) {
            return;
        }

        FR_ASSERT(owns(ptr) == OwnershipResult::Owns, "unowned pointer");

        FreeBlock *new_free = static_cast<FreeBlock *>(ptr);
        new_free->next = m_head;
        m_head = new_free;
        ++m_free_count;

        FR_ASSERT(m_free_count <= total_blocks(), "block count overflow");
    }

private:
    struct FreeBlock {
        FreeBlock *next;
    };

    Byte *m_buffer{nullptr};
    USize m_buffer_size{0};
    USize m_block_size{0};
    USize m_free_count{0};
    FreeBlock *m_head{nullptr};

    const char *m_custom_tag{"@noname"};
    char m_full_tag[64]{};
};

} // namespace fr
