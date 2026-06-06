/**
 * @file cmd.cpp
 * @author Kiju
 *
 * @brief Implementations for CmdBatch, CmdSheaf, and command invert methods.
 */

#include "fr/data/cmd.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"

namespace fr {

// ============================================================== Typed Inverses

DetachChildCmd AttachChildCmd::inverse() const noexcept {
    return {
        .parent = parent,
        .child = child,
    };
}
AttachChildCmd DetachChildCmd::inverse() const noexcept {
    return {
        .parent = parent,
        .child = child,
    };
}
KillCmd HandoutCmd::inverse() const noexcept {
    return {.thing = thing};
}
HandoutCmd KillCmd::inverse() const noexcept {
    return {.thing = thing};
}

DestroyCmd InsertCmd::inverse() const noexcept {
    return {
        .tidx = tidx,
        .thing = thing,
        .offset = offset,
    };
}

InsertCmd DestroyCmd::inverse() const noexcept {
    return {
        .tidx = tidx,
        .thing = thing,
        .offset = offset,
    };
}

/// Mutate inverse: swaps prev/next offsets.
MutateCmd MutateCmd::inverse() const noexcept {
    return {
        .tidx = tidx,
        .thing = thing,
        .prev_offset = next_offset,
        .next_offset = prev_offset,
    };
}

// ================================================================ impl Helpers

namespace impl {

/**
 * @brief Sums the arena bytes required for all part snapshots in `cmds`.
 * @param filter If non-nil, only commands belonging to that thing are counted.
 * @return At least 1 (`ArenaAlloc` requires a non-zero buffer).
 */
static USize compute_fitted_arena_size(Slice<const Cmd> cmds,
                                       Thing filter = Thing::nil()) noexcept {
    USize size = 0;

    for (const Cmd &cmd : cmds) {
        if (!filter.is_nil() && cmd.thing() != filter) {
            continue;
        }

        if (cmd.kind == CmdKind::Insert || cmd.kind == CmdKind::Destroy) {
            TypeIdx tidx =
                cmd.kind == CmdKind::Insert ? cmd.insert_part.tidx : cmd.destroy_part.tidx;
            const TypeMeta &m = tidx.meta();
            size += m.size + m.alignment;
        } else if (cmd.kind == CmdKind::Mutate) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            size += 2 * (m.size + m.alignment);
        }
    }

    return size == 0 ? 1 : size;
}

} // namespace impl

// ==================================================================== CmdBatch

void CmdBatch::do_init_storage(USize size, const char *tag) noexcept {
    Byte *buf = static_cast<Byte *>(m_alloc->allocate(size, alignof(std::max_align_t)));

    m_arena_buffer = Slice<Byte>(buf, size);
    m_arena = ArenaAlloc(buf, size, tag);
    m_cmds = DynamicArray<Cmd>::with_alloc(m_alloc);
}

void CmdBatch::do_copy_cmds(Slice<const Cmd> src_cmds, const Byte *src_base,
                            Thing filter) noexcept {
    for (const Cmd &src : src_cmds) {
        if (!filter.is_nil() && src.thing() != filter) {
            continue;
        }

        Cmd dst = src;

        if (src.kind == CmdKind::Insert) {
            const TypeMeta &m = src.insert_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.insert_part.offset = do_get_offset(ptr);
            m.copy_construct(ptr, src_base + src.insert_part.offset);
        } else if (src.kind == CmdKind::Destroy) {
            const TypeMeta &m = src.destroy_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.destroy_part.offset = do_get_offset(ptr);
            m.copy_construct(ptr, src_base + src.destroy_part.offset);

        } else if (src.kind == CmdKind::Mutate) {
            const TypeMeta &m = src.mutate_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *prev = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.prev_offset = do_get_offset(prev);
            m.copy_construct(prev, src_base + src.mutate_part.prev_offset);

            void *next = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.next_offset = do_get_offset(next);
            m.copy_construct(next, src_base + src.mutate_part.next_offset);
        }

        m_cmds.push_back(dst);
    }
}

CmdBatch::CmdBatch() noexcept
    : CmdBatch(get_ambient_ctx().alloc) {
}

CmdBatch::CmdBatch(Alloc *alloc, USize arena_size) noexcept
    : m_alloc(alloc) {
    do_init_storage(arena_size, "CmdBatch");
}

CmdBatch::CmdBatch(const CmdBatch &other) noexcept
    : m_alloc(get_ambient_ctx().alloc) {
    do_init_storage(impl::compute_fitted_arena_size(other.cmds()), "CmdBatch");
    do_copy_cmds(other.cmds(), other.arena());
}

CmdBatch::CmdBatch(CmdBatch &&other) noexcept
    : m_alloc(other.m_alloc),
      m_arena_buffer(other.m_arena_buffer),
      m_cmds(std::move(other.m_cmds)) {
    USize bump = other.m_arena.used();
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdBatch");

    if (bump > 0) {
        void *ptr = m_arena.allocate(bump, 1);
        FR_ASSERT(ptr, "bump advance failed");
    }

    other.m_arena_buffer = Slice<Byte>{};
}

CmdBatch::~CmdBatch() noexcept {
    reset();

    if (m_arena_buffer.data()) {
        m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(),
                            alignof(std::max_align_t));
    }
}

void CmdBatch::reset() noexcept {
    Byte *base = m_arena_buffer.data();

    for (const Cmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::Insert) {
            cmd.insert_part.tidx.meta().destroy(base + cmd.insert_part.offset);
        } else if (cmd.kind == CmdKind::Destroy) {
            cmd.destroy_part.tidx.meta().destroy(base + cmd.destroy_part.offset);
        } else if (cmd.kind == CmdKind::Mutate) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            m.destroy(base + cmd.mutate_part.prev_offset);
            m.destroy(base + cmd.mutate_part.next_offset);
        }
    }

    m_cmds.clear();
    m_arena.reset();
}

void CmdBatch::reset_reserve(USize size) noexcept {
    reset();

    if (size > m_arena_buffer.size()) {
        void *old_buffer = static_cast<void *>(m_arena_buffer.data());
        if (old_buffer) {
            m_alloc->deallocate(old_buffer, m_arena_buffer.size(), alignof(std::max_align_t));
        }

        void *new_buffer = static_cast<Byte *>(m_alloc->allocate(size, alignof(std::max_align_t)));
        m_arena_buffer = Slice<Byte>(static_cast<Byte *>(new_buffer), size);
        m_arena = ArenaAlloc(new_buffer, size, "CmdSheaf");
    }
}

// ==================================================================== CmdSheaf

void CmdSheaf::do_init_storage(USize size, const char *tag) noexcept {
    Byte *buf = static_cast<Byte *>(m_alloc->allocate(size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, size);
    m_arena = ArenaAlloc(buf, size, tag);
    m_cmds = DynamicArray<Cmd>::with_alloc(m_alloc);
}

void CmdSheaf::do_copy_cmds(Slice<const Cmd> src_cmds, const Byte *src_base,
                            Thing filter) noexcept {
    for (const Cmd &src : src_cmds) {
        if (!filter.is_nil() && src.thing() != filter) {
            continue;
        }

        Cmd dst = src;
        if (src.kind == CmdKind::Insert) {
            const TypeMeta &m = src.insert_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.insert_part.offset = do_get_offset(ptr);
            m.copy_construct(ptr, src_base + src.insert_part.offset);
        } else if (src.kind == CmdKind::Destroy) {
            const TypeMeta &m = src.destroy_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.destroy_part.offset = do_get_offset(ptr);
            m.copy_construct(ptr, src_base + src.destroy_part.offset);
        } else if (src.kind == CmdKind::Mutate) {
            const TypeMeta &m = src.mutate_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *prev = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.prev_offset = do_get_offset(prev);
            m.copy_construct(prev, src_base + src.mutate_part.prev_offset);

            void *next = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.next_offset = do_get_offset(next);
            m.copy_construct(next, src_base + src.mutate_part.next_offset);
        }

        m_cmds.push_back(dst);
    }
}

CmdSheaf::CmdSheaf(Alloc *alloc, Thing thing, USize arena_size) noexcept
    : m_alloc(alloc),
      m_thing(thing) {
    do_init_storage(arena_size, "CmdSheaf");
}

CmdSheaf::CmdSheaf(Alloc *alloc, const CmdBatch &batch, Thing thing) noexcept
    : m_alloc(alloc),
      m_thing(thing) {
    do_init_storage(impl::compute_fitted_arena_size(batch.cmds(), thing), "CmdSheaf");
    do_copy_cmds(batch.cmds(), batch.arena(), thing);
}

CmdSheaf::CmdSheaf(const CmdSheaf &other) noexcept
    : m_alloc(get_ambient_ctx().alloc),
      m_thing(other.m_thing) {
    do_init_storage(impl::compute_fitted_arena_size(other.cmds()), "CmdSheaf");
    do_copy_cmds(other.cmds(), other.arena());
}

CmdSheaf::CmdSheaf(CmdSheaf &&other) noexcept
    : m_alloc(other.m_alloc),
      m_thing(other.m_thing),
      m_arena_buffer(other.m_arena_buffer),
      m_cmds(std::move(other.m_cmds)) {
    USize bump = other.m_arena.used();
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdSheaf");

    if (bump > 0) {
        void *ptr = m_arena.allocate(bump, 1);
        FR_ASSERT(ptr, "bump advance failed");
    }

    other.m_arena_buffer = Slice<Byte>{};
}

CmdSheaf::~CmdSheaf() noexcept {
    reset();

    if (m_arena_buffer.data()) {
        m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(),
                            alignof(std::max_align_t));
    }
}

void CmdSheaf::reset() noexcept {
    Byte *base = m_arena_buffer.data();

    for (const Cmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::Insert) {
            cmd.insert_part.tidx.meta().destroy(base + cmd.insert_part.offset);
        } else if (cmd.kind == CmdKind::Destroy) {
            cmd.destroy_part.tidx.meta().destroy(base + cmd.destroy_part.offset);
        } else if (cmd.kind == CmdKind::Mutate) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            m.destroy(base + cmd.mutate_part.prev_offset);
            m.destroy(base + cmd.mutate_part.next_offset);
        }
    }

    m_cmds.clear();
    m_arena.reset();
}

void CmdSheaf::reset_reserve(USize size) noexcept {
    reset();

    if (size > m_arena_buffer.size()) {
        void *old_buffer = static_cast<void *>(m_arena_buffer.data());
        if (old_buffer) {
            m_alloc->deallocate(old_buffer, m_arena_buffer.size(), alignof(std::max_align_t));
        }

        void *new_buffer = static_cast<Byte *>(m_alloc->allocate(size, alignof(std::max_align_t)));
        m_arena_buffer = Slice<Byte>(static_cast<Byte *>(new_buffer), size);
        m_arena = ArenaAlloc(new_buffer, size, "CmdSheaf");
    }
}

// ================================================================= CmdSheaf Factory

/// @todo Optimize with `HashMap`.
CmdSheaf CmdSheaf::merge_compressed(Alloc *alloc, const CmdSheaf &a, const CmdSheaf &b) noexcept {
    // Worst-case arena: full copy of both sheaves (coalescing only saves space).
    USize arena_size =
        impl::compute_fitted_arena_size(a.cmds()) + impl::compute_fitted_arena_size(b.cmds());

    if (arena_size < 1) {
        arena_size = 1;
    }

    CmdSheaf merged(alloc, a.m_thing, arena_size);

    // Copy all of a cmds.
    merged.do_copy_cmds(a.m_cmds.slice(), a.m_arena_buffer.data());

    // Process b commands, coalescing `MutatePart` pairs for the same part.
    const Byte *b_base = b.m_arena_buffer.data();
    Byte *merged_base = merged.m_arena_buffer.data();

    for (const Cmd &src : b.m_cmds) {
        if (src.kind == CmdKind::Mutate) {
            // Check if merged already has a `MutatePart` for the same part type.
            Slice<Cmd> merged_cmds = merged.m_cmds.slice_mut();
            bool coalesced = false;

            for (USize i = 0; i < merged_cmds.size() && !coalesced; ++i) {
                Cmd &existing = merged_cmds[i];
                if (existing.kind != CmdKind::Mutate) {
                    continue;
                }

                if (&existing.mutate_part.tidx.meta() != &src.mutate_part.tidx.meta()) {
                    continue;
                }

                const TypeMeta &m = src.mutate_part.tidx.meta();
                FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

                m.destroy(merged_base + existing.mutate_part.next_offset);

                void *new_next = merged.m_arena.allocate(m.size, m.alignment);
                m.copy_construct(new_next, b_base + src.mutate_part.next_offset);
                existing.mutate_part.next_offset = merged.do_get_offset(new_next);

                coalesced = true;
            }

            if (!coalesced) {
                // First mutate for this part - copy both snapshots normally.
                const TypeMeta &m = src.mutate_part.tidx.meta();
                FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

                void *prev_ptr = merged.m_arena.allocate(m.size, m.alignment);
                m.copy_construct(prev_ptr, b_base + src.mutate_part.prev_offset);

                void *next_ptr = merged.m_arena.allocate(m.size, m.alignment);
                m.copy_construct(next_ptr, b_base + src.mutate_part.next_offset);

                merged.record_mutate_raw({
                    .tidx = src.mutate_part.tidx,
                    .thing = src.mutate_part.thing,
                    .prev_offset = merged.do_get_offset(prev_ptr),
                    .next_offset = merged.do_get_offset(next_ptr),
                });
            }
        } else if (src.kind == CmdKind::Insert) {
            const TypeMeta &m = src.insert_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *ptr = merged.m_arena.allocate(m.size, m.alignment);
            m.copy_construct(ptr, b_base + src.insert_part.offset);
            merged.record_insert_raw({
                .tidx = src.insert_part.tidx,
                .thing = src.insert_part.thing,
                .offset = merged.do_get_offset(ptr),
            });
        } else if (src.kind == CmdKind::Destroy) {
            const TypeMeta &m = src.destroy_part.tidx.meta();
            FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

            void *ptr = merged.m_arena.allocate(m.size, m.alignment);
            m.copy_construct(ptr, b_base + src.destroy_part.offset);
            merged.record_destroy_raw({
                .tidx = src.destroy_part.tidx,
                .thing = src.destroy_part.thing,
                .offset = merged.do_get_offset(ptr),
            });
        } else {
            // Non-part commands (attach/detach/kill).
            merged.m_cmds.push_back(src);
        }
    }

    return merged;
}

// ============================================================ CmdSheafTimeline

bool CmdSheafTimeline::compress() noexcept {
    go_present();

    if (m_count < 2) {
        return false;
    }

    USize prev_idx = do_prev_cursor();
    auto *head_sheaf = reinterpret_cast<CmdSheaf *>(m_storage + m_head * sizeof(CmdSheaf));
    auto *prev_sheaf = reinterpret_cast<CmdSheaf *>(m_storage + prev_idx * sizeof(CmdSheaf));

    CmdSheaf merged = CmdSheaf::merge_compressed(m_alloc, *prev_sheaf, *head_sheaf);

    head_sheaf->~CmdSheaf();
    prev_sheaf->~CmdSheaf();

    USize merged_time = m_calendar[m_head];
    new (prev_sheaf) CmdSheaf(std::move(merged));
    m_calendar[prev_idx] = merged_time;

    m_head = prev_idx;
    m_cursor = m_head;
    --m_count;

    return true;
}
} // namespace fr
