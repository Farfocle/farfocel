/**
 * @file block_allocator.hpp
 * @author Kiju
 *
 * @brief Fixed-size pool allocator over caller-provided memory buffer.
 */

#pragma once

#include <cstdio>

#include "fr/core/allocator.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/// @brief Pool allocator managing equal-sized blocks. This allocator can only exist in a valid,
/// non-null state.
class BlockAllocator final : public Allocator {
public:
    BlockAllocator(void *buffer, USize buffer_sz, USize block_sz,
                   const char *custom_tag = "@noname") noexcept
        : m_buffer(static_cast<Byte *>(buffer)),
          m_buffer_size(buffer_sz),
          m_block_size(std::max(block_sz, sizeof(void *))),
          m_custom_tag(custom_tag) {

        FR_ASSERT(buffer,
                  "fr::BlockAllocator(void* buffer, USize buffer_sz, USize block_sz, const char* "
                  "custom_tag = \"@noname\"): Buffer must be non-null");

        FR_ASSERT(buffer_sz > 0,
                  "fr::BlockAllocator(void* buffer, USize buffer_sz, USize block_sz, const char* "
                  "custom_tag = \"@noname\"): Buffer size must be non-zero");

        FR_ASSERT(buffer_sz >= m_block_size,
                  "fr::BlockAllocator:(void* buffer, USize buffer_sz, USize block_sz, const char* "
                  "custom_tag = \"@noname\") Buffer size too small for even one block");

        std::snprintf(m_full_tag, sizeof(m_full_tag), "BlockAllocator: %s", m_custom_tag);

        reset();
    }

    /// @brief Resets the allocator and rebuilds the free list.
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

    [[nodiscard]] USize buffer_size() const noexcept {
        return m_buffer_size;
    }

    [[nodiscard]] USize block_size() const noexcept {
        return m_block_size;
    }

    [[nodiscard]] USize total_blocks() const noexcept {
        return m_buffer_size / m_block_size;
    }

    [[nodiscard]] USize free_blocks() const noexcept {
        return m_free_count;
    }

    [[nodiscard]] USize used_blocks() const noexcept {
        return total_blocks() - m_free_count;
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
        FR_ASSERT(sz <= m_block_size, "fr::BlockAllocator::do_try_allocate(USize sz, USize): Size "
                                      "`sz` has to be smaller or equal to the block size");

        if (!m_head) {
            return nullptr;
        }

        void *ptr = m_head;
        if (mem::align_forward_padding(ptr, alignment) != 0) {
            FR_PANIC("BlockAllocator block does not satisfy requested alignment");
            return nullptr;
        }

        m_head = m_head->next;
        --m_free_count;

        return ptr;
    }

    /// @note Rellocation only happens if the new size is smaller or equal to the old one. Othewise
    /// it returns a nullptr - signaling an error state.
    void *do_try_reallocate(void *ptr, USize /*old_sz*/, USize new_sz,
                            USize alignment) noexcept override {
        FR_ASSERT(owns(ptr) == OwnershipResult::Owns,
                  "fr::BlockAllocator::reallocate(void *ptr, USize, USize new_sz, USize): Pointer "
                  "not owned by this allocator");

        if (new_sz <= m_block_size && mem::align_forward_padding(ptr, alignment) == 0) {
            return ptr;
        }

        return nullptr;
    }

    void do_deallocate(void *ptr, USize /*sz*/, USize /*alignment*/) noexcept override {
        if (!ptr) {
            return;
        }

        FR_ASSERT(owns(ptr) == OwnershipResult::Owns,
                  "fr::BlockAllocator::deallocate(void *ptr, USize, USize): Pointer not owned by "
                  "this allocator");

        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_buffer);

        FR_ASSERT(offset % m_block_size == 0, "fr::BlockAllocator::deallocate(void *ptr, USize, "
                                              "USize): Pointer is not block-aligned");

        FreeBlock *new_free = static_cast<FreeBlock *>(ptr);
        new_free->next = m_head;
        m_head = new_free;
        ++m_free_count;

        FR_ASSERT(m_free_count <= total_blocks(), "fr::BlockAllocator::deallocate(void *ptr, "
                                                  "USize, USize): Free count exceeds total blocks");
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
