/**
 * @file cmd.cpp
 * @author Kiju
 *
 * @brief Implementations for CmdBatch, CmdSheaf, and command invert methods.
 */

#include "fr/data/cmd.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/hash_map.hpp"

namespace fr {

// ======================================================================== Cmd

Thing Cmd::thing() const noexcept {
    switch (kind) {
    case CmdKind::Insert:
        return insert_part.thing;
    case CmdKind::Destroy:
        return destroy_part.thing;
    case CmdKind::Mutate:
        return mutate_part.thing;
    case CmdKind::AttachChild:
        return attach_child.parent;
    case CmdKind::DetachChild:
        return detach_child.parent;
    case CmdKind::Handout:
        return handout.thing;
    case CmdKind::Kill:
        return kill.thing;
    default:
        return Thing::nil();
    }
}

Cmd Cmd::inverse() const noexcept {
    switch (kind) {
    case CmdKind::Insert:
        return {.kind = CmdKind::Destroy, .destroy_part = insert_part.inverse()};
    case CmdKind::Destroy:
        return {.kind = CmdKind::Insert, .insert_part = destroy_part.inverse()};
    case CmdKind::Mutate:
        return {.kind = CmdKind::Mutate, .mutate_part = mutate_part.inverse()};
    case CmdKind::AttachChild:
        return {.kind = CmdKind::DetachChild, .detach_child = attach_child.inverse()};
    case CmdKind::DetachChild:
        return {.kind = CmdKind::AttachChild, .attach_child = detach_child.inverse()};
    case CmdKind::Handout:
        return {.kind = CmdKind::Kill, .kill = handout.inverse()};
    case CmdKind::Kill:
        return {.kind = CmdKind::Handout, .handout = kill.inverse()};
    default:
        return nil();
    }
}

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

CmdBatch::CmdBatch(USize arena_size) noexcept
    : CmdBatch(get_ambient_ctx().alloc, arena_size) {
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

USize CmdBatch::do_get_offset(void *ptr) const noexcept {
    return static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
}

CmdBatch &CmdBatch::operator=(const CmdBatch &other) noexcept {
    if (this != &other) {
        this->~CmdBatch();
        new (this) CmdBatch(other);
    }
    return *this;
}

CmdBatch &CmdBatch::operator=(CmdBatch &&other) noexcept {
    if (this != &other) {
        this->~CmdBatch();
        new (this) CmdBatch(std::move(other));
    }
    return *this;
}

void CmdBatch::record_insert_raw(InsertCmd cmd) noexcept {
    FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
    Cmd c{};
    c.kind = CmdKind::Insert;
    c.insert_part = cmd;
    m_cmds.push_back(c);
}

void CmdBatch::record_destroy_raw(DestroyCmd cmd) noexcept {
    FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
    Cmd c{};
    c.kind = CmdKind::Destroy;
    c.destroy_part = cmd;
    m_cmds.push_back(c);
}

void CmdBatch::record_mutate_raw(MutateCmd cmd) noexcept {
    FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
    Cmd c{};
    c.kind = CmdKind::Mutate;
    c.mutate_part = cmd;
    m_cmds.push_back(c);
}

void CmdBatch::record_attach_child_raw(AttachChildCmd cmd) noexcept {
    FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
    Cmd c{};
    c.kind = CmdKind::AttachChild;
    c.attach_child = cmd;
    m_cmds.push_back(c);
}

void CmdBatch::record_detach_child_raw(DetachChildCmd cmd) noexcept {
    FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
    Cmd c{};
    c.kind = CmdKind::DetachChild;
    c.detach_child = cmd;
    m_cmds.push_back(c);
}

void CmdBatch::record_handout_raw(HandoutCmd cmd) noexcept {
    FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
    Cmd c{};
    c.kind = CmdKind::Handout;
    c.handout = cmd;
    m_cmds.push_back(c);
}

void CmdBatch::record_kill_raw(KillCmd cmd) noexcept {
    FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
    Cmd c{};
    c.kind = CmdKind::Kill;
    c.kill = cmd;
    m_cmds.push_back(c);
}

void CmdBatch::record_attach_child(Thing parent, Thing child) noexcept {
    record_attach_child_raw({.parent = parent, .child = child});
}

void CmdBatch::record_detach_child(Thing parent, Thing child) noexcept {
    record_detach_child_raw({.parent = parent, .child = child});
}

void CmdBatch::record_handout(Thing thing) noexcept {
    record_handout_raw({.thing = thing});
}

void CmdBatch::record_kill(Thing thing) noexcept {
    record_kill_raw({.thing = thing});
}

Slice<const Cmd> CmdBatch::cmds() const noexcept {
    return m_cmds.slice();
}

Byte *CmdBatch::arena() noexcept {
    return m_arena_buffer.data();
}

const Byte *CmdBatch::arena() const noexcept {
    return m_arena_buffer.data();
}

CmdBatch CmdBatch::merge(Alloc *alloc, const CmdBatch &a, const CmdBatch &b) noexcept {
    USize arena_size =
        impl::compute_fitted_arena_size(a.cmds()) + impl::compute_fitted_arena_size(b.cmds());
    if (arena_size < 1) {
        arena_size = 1;
    }

    CmdBatch merged(alloc, arena_size);

    // Copy all of a's commands into the merged batch.
    merged.do_copy_cmds(a.m_cmds.slice(), a.m_arena_buffer.data());

    // Build map: pack (thing_raw << 32 | tidx_idx) -> index in merged.m_cmds.
    auto mutate_idx = HashMap<U64, USize>::with_alloc(alloc);
    for (USize i = 0; i < merged.m_cmds.size(); ++i) {
        const Cmd &cmd = merged.m_cmds[i];
        if (cmd.kind == CmdKind::Mutate) {
            U64 key = (static_cast<U64>(cmd.mutate_part.thing.as_raw()) << 32) |
                      static_cast<U64>(cmd.mutate_part.tidx.idx());
            mutate_idx[key] = i;
        }
    }

    // Process b's commands: coalesce mutates, pass everything else through.
    const Byte *b_base = b.m_arena_buffer.data();
    for (const Cmd &src : b.m_cmds) {
        if (src.kind == CmdKind::Mutate) {
            U64 key = (static_cast<U64>(src.mutate_part.thing.as_raw()) << 32) |
                      static_cast<U64>(src.mutate_part.tidx.idx());

            if (auto opt = mutate_idx.find(key)) {
                // Coalesce: keep existing prev, replace next with b's next.
                Cmd &existing = merged.m_cmds[*opt.unwrap()];
                const TypeMeta &m = src.mutate_part.tidx.meta();
                FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

                m.destroy(merged.m_arena_buffer.data() + existing.mutate_part.next_offset);
                void *new_next = merged.m_arena.allocate(m.size, m.alignment);
                m.copy_construct(new_next, b_base + src.mutate_part.next_offset);
                existing.mutate_part.next_offset = merged.do_get_offset(new_next);
            } else {
                // First mutate for this (thing, type) pair.
                const TypeMeta &m = src.mutate_part.tidx.meta();
                FR_ASSERT(m.copy_construct, "part type is not copy-constructible");

                void *prev_ptr = merged.m_arena.allocate(m.size, m.alignment);
                m.copy_construct(prev_ptr, b_base + src.mutate_part.prev_offset);
                void *next_ptr = merged.m_arena.allocate(m.size, m.alignment);
                m.copy_construct(next_ptr, b_base + src.mutate_part.next_offset);

                USize new_idx = merged.m_cmds.size();
                merged.record_mutate_raw({
                    .tidx = src.mutate_part.tidx,
                    .thing = src.mutate_part.thing,
                    .prev_offset = merged.do_get_offset(prev_ptr),
                    .next_offset = merged.do_get_offset(next_ptr),
                });
                mutate_idx[key] = new_idx;
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
            merged.m_cmds.push_back(src);
        }
    }

    return merged;
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

USize CmdSheaf::do_get_offset(void *ptr) const noexcept {
    return static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
}

CmdSheaf &CmdSheaf::operator=(const CmdSheaf &other) noexcept {
    if (this != &other) {
        this->~CmdSheaf();
        new (this) CmdSheaf(other);
    }
    return *this;
}

CmdSheaf &CmdSheaf::operator=(CmdSheaf &&other) noexcept {
    if (this != &other) {
        this->~CmdSheaf();
        new (this) CmdSheaf(std::move(other));
    }
    return *this;
}

void CmdSheaf::record_insert_raw(InsertCmd cmd) noexcept {
    Cmd c{};
    c.kind = CmdKind::Insert;
    c.insert_part = cmd;
    m_cmds.push_back(c);
}

void CmdSheaf::record_destroy_raw(DestroyCmd cmd) noexcept {
    Cmd c{};
    c.kind = CmdKind::Destroy;
    c.destroy_part = cmd;
    m_cmds.push_back(c);
}

void CmdSheaf::record_mutate_raw(MutateCmd cmd) noexcept {
    Cmd c{};
    c.kind = CmdKind::Mutate;
    c.mutate_part = cmd;
    m_cmds.push_back(c);
}

void CmdSheaf::record_attach_child_raw(AttachChildCmd cmd) noexcept {
    Cmd c{};
    c.kind = CmdKind::AttachChild;
    c.attach_child = cmd;
    m_cmds.push_back(c);
}

void CmdSheaf::record_detach_child_raw(DetachChildCmd cmd) noexcept {
    Cmd c{};
    c.kind = CmdKind::DetachChild;
    c.detach_child = cmd;
    m_cmds.push_back(c);
}

void CmdSheaf::record_kill_raw(KillCmd cmd) noexcept {
    Cmd c{};
    c.kind = CmdKind::Kill;
    c.kill = cmd;
    m_cmds.push_back(c);
}

void CmdSheaf::record_attach_child(Thing parent, Thing child) noexcept {
    record_attach_child_raw({.parent = parent, .child = child});
}

void CmdSheaf::record_detach_child(Thing parent, Thing child) noexcept {
    record_detach_child_raw({.parent = parent, .child = child});
}

void CmdSheaf::record_kill() noexcept {
    record_kill_raw({.thing = m_thing});
}

Thing CmdSheaf::thing() const noexcept {
    return m_thing;
}

Slice<const Cmd> CmdSheaf::cmds() const noexcept {
    return m_cmds.slice();
}

Byte *CmdSheaf::arena() noexcept {
    return m_arena_buffer.data();
}

const Byte *CmdSheaf::arena() const noexcept {
    return m_arena_buffer.data();
}

// ================================================================= CmdSheaf Factory

CmdSheaf CmdSheaf::merge(Alloc *alloc, const CmdSheaf &a, const CmdSheaf &b) noexcept {
    // Worst-case arena: full copy of both sheaves (coalescing only saves space).
    USize arena_size =
        impl::compute_fitted_arena_size(a.cmds()) + impl::compute_fitted_arena_size(b.cmds());

    if (arena_size < 1) {
        arena_size = 1;
    }

    CmdSheaf merged(alloc, a.m_thing, arena_size);

    // Copy all of a's commands.
    merged.do_copy_cmds(a.m_cmds.slice(), a.m_arena_buffer.data());

    // Process b's commands, coalescing MutateCmd pairs for the same part type.
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

// ============================================================ CmdBatchTimeline

CmdBatchTimeline::CmdBatchTimeline(Alloc *alloc) noexcept
    : m_alloc(alloc) {
    (void)m_alloc;
    m_calendar = Array<USize, BATCH_RING_SIZE>::from_repeated(0);
}

USize CmdBatchTimeline::time() const noexcept {
    return m_time;
}

USize CmdBatchTimeline::count() const noexcept {
    return m_count;
}

CmdBatch &CmdBatchTimeline::batch() noexcept {
    return m_batches[m_cursor];
}

const CmdBatch &CmdBatchTimeline::batch() const noexcept {
    return m_batches[m_cursor];
}

void CmdBatchTimeline::push(const CmdBatch &b, USize dt) noexcept {
    do_push_advance();
    m_batches[m_head] = b;
    m_cursor = m_head;
    m_time += dt;
    m_calendar[m_head] = m_time;
}

void CmdBatchTimeline::push(CmdBatch &&b, USize dt) noexcept {
    do_push_advance();
    m_batches[m_head] = std::move(b);
    m_cursor = m_head;
    m_time += dt;
    m_calendar[m_head] = m_time;
}

bool CmdBatchTimeline::go_future() noexcept {
    if (m_count == 0 || m_cursor == m_head) {
        return false;
    }
    m_cursor = do_next(m_cursor);
    return true;
}

bool CmdBatchTimeline::go_past() noexcept {
    if (m_count == 0) {
        return false;
    }
    USize oldest = (m_head + BATCH_RING_SIZE - (m_count - 1)) % BATCH_RING_SIZE;
    if (m_cursor == oldest) {
        return false;
    }
    m_cursor = do_prev(m_cursor);
    return true;
}

void CmdBatchTimeline::go_present() noexcept {
    if (m_count > 0) {
        m_cursor = m_head;
    }
}

USize CmdBatchTimeline::count_future() const noexcept {
    if (m_count == 0) {
        return 0;
    }
    return (m_head + BATCH_RING_SIZE - m_cursor) % BATCH_RING_SIZE;
}

USize CmdBatchTimeline::count_past() const noexcept {
    if (m_count == 0) {
        return 0;
    }
    USize oldest = (m_head + BATCH_RING_SIZE - (m_count - 1)) % BATCH_RING_SIZE;
    return (m_cursor + BATCH_RING_SIZE - oldest) % BATCH_RING_SIZE;
}

void CmdBatchTimeline::do_push_advance() noexcept {
    m_head = do_next(m_head);
    if (m_count < BATCH_RING_SIZE) {
        ++m_count;
    }
}

bool CmdBatchTimeline::compress() noexcept {
    go_present();

    if (m_count < 2) {
        return false;
    }

    USize prev_idx = do_prev(m_head);
    m_batches[prev_idx] = CmdBatch::merge(m_alloc, m_batches[prev_idx], m_batches[m_head]);
    m_calendar[prev_idx] = m_calendar[m_head];

    m_head = prev_idx;
    m_cursor = m_head;
    --m_count;

    return true;
}

// ============================================================ CmdSheafTimeline

CmdSheafTimeline::CmdSheafTimeline(Alloc *alloc, Thing thing) noexcept
    : m_alloc(alloc),
      m_thing(thing) {
    m_calendar = Array<USize, SHEAF_RING_SIZE>::from_repeated(0);
}

CmdSheafTimeline::CmdSheafTimeline(const CmdSheafTimeline &other) noexcept
    : m_alloc(other.m_alloc),
      m_thing(other.m_thing),
      m_calendar(other.m_calendar),
      m_head(other.m_head),
      m_cursor(other.m_cursor),
      m_time(other.m_time),
      m_count(other.m_count) {
    for (USize i = 0, idx = m_head; i < m_count; ++i, idx = do_prev(idx)) {
        new (do_sheaf_at(idx)) CmdSheaf(*other.do_sheaf_at(idx));
    }
}

CmdSheafTimeline::CmdSheafTimeline(CmdSheafTimeline &&other) noexcept
    : m_alloc(other.m_alloc),
      m_thing(other.m_thing),
      m_calendar(other.m_calendar),
      m_head(other.m_head),
      m_cursor(other.m_cursor),
      m_time(other.m_time),
      m_count(other.m_count) {
    for (USize i = 0, idx = m_head; i < m_count; ++i, idx = do_prev(idx)) {
        new (do_sheaf_at(idx)) CmdSheaf(std::move(*other.do_sheaf_at(idx)));
    }
    other.do_destroy_all();
    other.m_count = 0;
}

CmdSheafTimeline &CmdSheafTimeline::operator=(const CmdSheafTimeline &other) noexcept {
    if (this == &other) {
        return *this;
    }
    do_destroy_all();
    m_alloc = other.m_alloc;
    m_thing = other.m_thing;
    m_calendar = other.m_calendar;
    m_head = other.m_head;
    m_cursor = other.m_cursor;
    m_time = other.m_time;
    m_count = other.m_count;
    for (USize i = 0, idx = m_head; i < m_count; ++i, idx = do_prev(idx)) {
        new (do_sheaf_at(idx)) CmdSheaf(*other.do_sheaf_at(idx));
    }
    return *this;
}

CmdSheafTimeline &CmdSheafTimeline::operator=(CmdSheafTimeline &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    do_destroy_all();
    m_alloc = other.m_alloc;
    m_thing = other.m_thing;
    m_calendar = other.m_calendar;
    m_head = other.m_head;
    m_cursor = other.m_cursor;
    m_time = other.m_time;
    m_count = other.m_count;
    for (USize i = 0, idx = m_head; i < m_count; ++i, idx = do_prev(idx)) {
        new (do_sheaf_at(idx)) CmdSheaf(std::move(*other.do_sheaf_at(idx)));
    }
    other.do_destroy_all();
    other.m_count = 0;
    return *this;
}

CmdSheafTimeline::~CmdSheafTimeline() noexcept {
    do_destroy_all();
}

USize CmdSheafTimeline::time() const noexcept {
    return m_time;
}

USize CmdSheafTimeline::count() const noexcept {
    return m_count;
}

Thing CmdSheafTimeline::thing() const noexcept {
    return m_thing;
}

CmdSheaf &CmdSheafTimeline::sheaf() noexcept {
    return *do_sheaf_at(m_cursor);
}

const CmdSheaf &CmdSheafTimeline::sheaf() const noexcept {
    return *do_sheaf_at(m_cursor);
}

void CmdSheafTimeline::push(const CmdSheaf &s, USize dt) noexcept {
    do_push_advance();
    new (do_sheaf_at(m_head)) CmdSheaf(s);
    m_cursor = m_head;
    m_time += dt;
    m_calendar[m_head] = m_time;
}

void CmdSheafTimeline::push(CmdSheaf &&s, USize dt) noexcept {
    do_push_advance();
    new (do_sheaf_at(m_head)) CmdSheaf(std::move(s));
    m_cursor = m_head;
    m_time += dt;
    m_calendar[m_head] = m_time;
}

bool CmdSheafTimeline::go_future() noexcept {
    if (m_count == 0 || m_cursor == m_head) {
        return false;
    }
    m_cursor = do_next(m_cursor);
    return true;
}

bool CmdSheafTimeline::go_past() noexcept {
    if (m_count == 0) {
        return false;
    }
    USize oldest = (m_head + SHEAF_RING_SIZE - (m_count - 1)) % SHEAF_RING_SIZE;
    if (m_cursor == oldest) {
        return false;
    }
    m_cursor = do_prev(m_cursor);
    return true;
}

void CmdSheafTimeline::go_present() noexcept {
    if (m_count > 0) {
        m_cursor = m_head;
    }
}

USize CmdSheafTimeline::count_future() const noexcept {
    if (m_count == 0) {
        return 0;
    }
    return (m_head + SHEAF_RING_SIZE - m_cursor) % SHEAF_RING_SIZE;
}

USize CmdSheafTimeline::count_past() const noexcept {
    if (m_count == 0) {
        return 0;
    }
    USize oldest = (m_head + SHEAF_RING_SIZE - (m_count - 1)) % SHEAF_RING_SIZE;
    return (m_cursor + SHEAF_RING_SIZE - oldest) % SHEAF_RING_SIZE;
}

void CmdSheafTimeline::do_destroy_all() noexcept {
    for (USize i = 0, idx = m_head; i < m_count; ++i, idx = do_prev(idx)) {
        do_sheaf_at(idx)->~CmdSheaf();
    }
}

void CmdSheafTimeline::do_push_advance() noexcept {
    USize next = do_next(m_head);
    if (m_count == SHEAF_RING_SIZE) {
        do_sheaf_at(next)->~CmdSheaf();
    } else {
        ++m_count;
    }
    m_head = next;
}

bool CmdSheafTimeline::compress() noexcept {
    go_present();

    if (m_count < 2) {
        return false;
    }

    USize prev_idx = do_prev(m_cursor);
    auto *head_sheaf = do_sheaf_at(m_head);
    auto *prev_sheaf = do_sheaf_at(prev_idx);

    CmdSheaf merged = CmdSheaf::merge(m_alloc, *prev_sheaf, *head_sheaf);

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
