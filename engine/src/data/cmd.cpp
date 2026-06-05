/**
 * @file cmd.cpp
 * @author Kiju
 *
 * @brief Implementations for CmdBatch, CmdSheaf, and command invert methods.
 */

#include "fr/data/cmd.hpp"

namespace fr {

// ============================================================== Typed Inverses

DetachChildCmd AttachChildCmd::inverse() const noexcept {
    return {parent, child};
}

AttachChildCmd DetachChildCmd::inverse() const noexcept {
    return {parent, child};
}

KillCmd HandoutCmd::inverse() const noexcept {
    return {thing};
}

HandoutCmd KillCmd::inverse() const noexcept {
    return {thing};
}

// `RawInsertPartCmd` and `RawDestroyPartCmd` mirror each other as inverses.
DestroyCmd InsertCmd::inverse() const noexcept {
    return {.tidx = tidx, .thing = thing, .offset = offset};
}

InsertCmd DestroyCmd::inverse() const noexcept {
    return {.tidx = tidx, .thing = thing, .offset = offset};
}

// Mutate inverse swaps prev and next.
MutateCmd MutateCmd::inverse() const noexcept {
    return {
        .tidx = tidx,
        .thing = thing,
        .prev_offset = next_offset,
        .next_offset = prev_offset,
    };
}

// ==================================================================== CmdBatch

CmdBatch::CmdBatch() noexcept
    : CmdBatch(get_ambient_ctx().alloc) {
}

CmdBatch::CmdBatch(Alloc *alloc, USize arena_size) noexcept
    : m_alloc(alloc) {
    Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdBatch");
    m_cmds = DynamicArray<Cmd>::with_alloc(alloc);
}

CmdBatch::~CmdBatch() noexcept {
    reset();
    // m_arena_buffer.data() is null in the moved-from state — skip deallocation.
    if (m_arena_buffer.data()) {
        m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(),
                            alignof(std::max_align_t));
    }
}

CmdBatch::CmdBatch(const CmdBatch &other) noexcept {
    m_alloc = get_ambient_ctx().alloc;

    // Compute fitted arena size.
    USize arena_size = 0;
    for (const Cmd &cmd : other.cmds()) {
        if (cmd.kind == CmdKind::InsertPart || cmd.kind == CmdKind::DestroyPart) {
            TypeIdx tidx =
                cmd.kind == CmdKind::InsertPart ? cmd.insert_part.tidx : cmd.destroy_part.tidx;
            const TypeMeta &m = tidx.meta();
            arena_size += m.size + m.alignment;
        } else if (cmd.kind == CmdKind::MutatePart) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            arena_size += 2 * (m.size + m.alignment);
        }
    }
    if (arena_size == 0) {
        arena_size = 1;
    }

    Byte *buf = static_cast<Byte *>(m_alloc->allocate(arena_size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(buf, arena_size, "CmdBatch");
    m_cmds = DynamicArray<Cmd>::with_alloc(m_alloc);

    // Copy-construct each part snapshot into the new arena, patching stored offsets.
    const Byte *src_base = other.arena();
    for (const Cmd &src : other.cmds()) {
        Cmd dst = src;

        if (src.kind == CmdKind::InsertPart) {
            const TypeMeta &m = src.insert_part.tidx.meta();

            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot copy CmdBatch");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.insert_part.offset = static_cast<USize>(static_cast<Byte *>(ptr) - buf);
            m.copy_construct(ptr, src_base + src.insert_part.offset);
        } else if (src.kind == CmdKind::DestroyPart) {
            const TypeMeta &m = src.destroy_part.tidx.meta();

            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot copy CmdBatch");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.destroy_part.offset = static_cast<USize>(static_cast<Byte *>(ptr) - buf);
            m.copy_construct(ptr, src_base + src.destroy_part.offset);
        } else if (src.kind == CmdKind::MutatePart) {
            const TypeMeta &m = src.mutate_part.tidx.meta();

            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot copy CmdBatch");

            void *prev = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.prev_offset = static_cast<USize>(static_cast<Byte *>(prev) - buf);
            m.copy_construct(prev, src_base + src.mutate_part.prev_offset);

            void *next = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.next_offset = static_cast<USize>(static_cast<Byte *>(next) - buf);
            m.copy_construct(next, src_base + src.mutate_part.next_offset);
        }

        m_cmds.push_back(dst);
    }
}

CmdBatch::CmdBatch(CmdBatch &&other) noexcept
    : m_alloc(other.m_alloc),
      m_arena_buffer(other.m_arena_buffer),
      m_cmds(std::move(other.m_cmds)) {
    USize bump = other.m_arena.used();
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdBatch");

    if (bump > 0) {
        void *ptr = m_arena.allocate(bump, 1);
        FR_ASSERT(ptr, "allocation failed");
    }

    // Zero source's buffer slice so its destructor skips the deallocate call.
    other.m_arena_buffer = Slice<Byte>{};
    // other.m_cmds is already empty after std::move.
}

void CmdBatch::reset() noexcept {
    // Destroy all arena-placed part objects before resetting the arena pointer.
    Byte *base = m_arena_buffer.data();

    for (const Cmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::InsertPart) {
            cmd.insert_part.tidx.meta().destroy(base + cmd.insert_part.offset);
        } else if (cmd.kind == CmdKind::DestroyPart) {
            cmd.destroy_part.tidx.meta().destroy(base + cmd.destroy_part.offset);
        } else if (cmd.kind == CmdKind::MutatePart) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            m.destroy(base + cmd.mutate_part.prev_offset);
            m.destroy(base + cmd.mutate_part.next_offset);
        }
    }

    m_cmds.clear();
    m_arena.reset();
}

// ==================================================================== CmdSheaf

CmdSheaf::CmdSheaf(Alloc *alloc, Thing thing, USize arena_size) noexcept
    : m_alloc(alloc),
      m_thing(thing) {
    Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdSheaf");
    m_cmds = DynamicArray<Cmd>::with_alloc(alloc);
}

CmdSheaf::CmdSheaf(Alloc *alloc, const CmdBatch &batch, Thing thing) noexcept
    : m_alloc(alloc),
      m_thing(thing) {
    USize arena_size = 0;

    for (const Cmd &cmd : batch.cmds()) {
        if (cmd.thing() != thing) {
            continue;
        }

        if (cmd.kind == CmdKind::InsertPart || cmd.kind == CmdKind::DestroyPart) {
            TypeIdx tidx =
                cmd.kind == CmdKind::InsertPart ? cmd.insert_part.tidx : cmd.destroy_part.tidx;
            const TypeMeta &m = tidx.meta();
            arena_size += m.size + m.alignment;
        } else if (cmd.kind == CmdKind::MutatePart) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            arena_size += 2 * (m.size + m.alignment);
        }
    }

    if (arena_size == 0) {
        arena_size = 1;
    }

    // Allocate the fitted arena.
    Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdSheaf");
    m_cmds = DynamicArray<Cmd>::with_alloc(alloc);

    const Byte *src_base = batch.arena();
    for (const Cmd &src : batch.cmds()) {
        if (src.thing() != thing) {
            continue;
        }

        Cmd dst = src;

        if (src.kind == CmdKind::InsertPart) {
            const TypeMeta &m = src.insert_part.tidx.meta();
            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot extract into CmdSheaf");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.insert_part.offset =
                static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
            m.copy_construct(ptr, src_base + src.insert_part.offset);

        } else if (src.kind == CmdKind::DestroyPart) {
            const TypeMeta &m = src.destroy_part.tidx.meta();
            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot extract into CmdSheaf");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.destroy_part.offset =
                static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
            m.copy_construct(ptr, src_base + src.destroy_part.offset);

        } else if (src.kind == CmdKind::MutatePart) {
            const TypeMeta &m = src.mutate_part.tidx.meta();
            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot extract into CmdSheaf");

            void *prev_ptr = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.prev_offset =
                static_cast<USize>(static_cast<Byte *>(prev_ptr) - m_arena_buffer.data());
            m.copy_construct(prev_ptr, src_base + src.mutate_part.prev_offset);

            void *next_ptr = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.next_offset =
                static_cast<USize>(static_cast<Byte *>(next_ptr) - m_arena_buffer.data());
            m.copy_construct(next_ptr, src_base + src.mutate_part.next_offset);
        }

        m_cmds.push_back(dst);
    }
}

CmdSheaf::CmdSheaf(const CmdSheaf &other) noexcept {
    m_alloc = get_ambient_ctx().alloc;
    m_thing = other.m_thing;

    USize arena_size = 0;

    for (const Cmd &cmd : other.cmds()) {
        if (cmd.kind == CmdKind::InsertPart || cmd.kind == CmdKind::DestroyPart) {
            TypeIdx tidx =
                cmd.kind == CmdKind::InsertPart ? cmd.insert_part.tidx : cmd.destroy_part.tidx;
            const TypeMeta &m = tidx.meta();
            arena_size += m.size + m.alignment;
        } else if (cmd.kind == CmdKind::MutatePart) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            arena_size += 2 * (m.size + m.alignment);
        }
    }

    if (arena_size == 0) {
        arena_size = 1;
    }

    Byte *buf = static_cast<Byte *>(m_alloc->allocate(arena_size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(buf, arena_size, "CmdSheaf");
    m_cmds = DynamicArray<Cmd>::with_alloc(m_alloc);

    const Byte *src_base = other.arena();
    for (const Cmd &src : other.cmds()) {
        Cmd dst = src;

        if (src.kind == CmdKind::InsertPart) {
            const TypeMeta &m = src.insert_part.tidx.meta();

            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot copy CmdSheaf");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.insert_part.offset = static_cast<USize>(static_cast<Byte *>(ptr) - buf);
            m.copy_construct(ptr, src_base + src.insert_part.offset);
        } else if (src.kind == CmdKind::DestroyPart) {
            const TypeMeta &m = src.destroy_part.tidx.meta();

            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot copy CmdSheaf");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.destroy_part.offset = static_cast<USize>(static_cast<Byte *>(ptr) - buf);
            m.copy_construct(ptr, src_base + src.destroy_part.offset);
        } else if (src.kind == CmdKind::MutatePart) {
            const TypeMeta &m = src.mutate_part.tidx.meta();

            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot copy CmdSheaf");

            void *prev = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.prev_offset = static_cast<USize>(static_cast<Byte *>(prev) - buf);
            m.copy_construct(prev, src_base + src.mutate_part.prev_offset);

            void *next = m_arena.allocate(m.size, m.alignment);
            dst.mutate_part.next_offset = static_cast<USize>(static_cast<Byte *>(next) - buf);
            m.copy_construct(next, src_base + src.mutate_part.next_offset);
        }

        m_cmds.push_back(dst);
    }
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
        FR_ASSERT(ptr, "allocation failed");
    }

    other.m_arena_buffer = Slice<Byte>{};
}

CmdSheaf::~CmdSheaf() noexcept {
    reset();
    m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(), alignof(std::max_align_t));
}

void CmdSheaf::reset() noexcept {
    Byte *base = m_arena_buffer.data();

    for (const Cmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::InsertPart) {
            cmd.insert_part.tidx.meta().destroy(base + cmd.insert_part.offset);
        } else if (cmd.kind == CmdKind::DestroyPart) {
            cmd.destroy_part.tidx.meta().destroy(base + cmd.destroy_part.offset);
        } else if (cmd.kind == CmdKind::MutatePart) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            m.destroy(base + cmd.mutate_part.prev_offset);
            m.destroy(base + cmd.mutate_part.next_offset);
        }
    }

    m_cmds.clear();
    m_arena.reset();
}
} // namespace fr
