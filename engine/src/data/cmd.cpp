/**
 * @file cmd.cpp
 * @author Kiju
 *
 * @brief CmdBatch and CmdSheaf implementations.
 */

#include "fr/data/cmd.hpp"
#include "fr/data/world.hpp"

namespace fr {

// ============================================================== Typed Inverses

DetachChildCmd AttachChildCmd::inverse() const noexcept {
    return {parent, child};
}

AttachChildCmd DetachChildCmd::inverse() const noexcept {
    return {parent, child};
}

namespace impl {

RawDestroyPartCmd RawInsertPartCmd::invert() const noexcept {
    return {.tidx = tidx,
            .thing = thing,
            .part_offset = part_offset,
            .commit_destroy = commit_destroy,
            .commit_insert_move = commit_insert_move,
            .commit_insert_copy = commit_insert_copy};
}

RawInsertPartCmd RawDestroyPartCmd::invert() const noexcept {
    return {.tidx = tidx,
            .thing = thing,
            .part_offset = part_offset,
            .commit_insert_move = commit_insert_move,
            .commit_insert_copy = commit_insert_copy,
            .commit_destroy = commit_destroy};
}

RawMutatePartCmd RawMutatePartCmd::invert() const noexcept {
    return {
        .tidx = tidx,
        .thing = thing,
        .prev_offset = next_offset,
        .next_offset = prev_offset,
        .commit_mutate = commit_mutate,
    };
}

// ============================================================= Shared Helpers

void commit_raw_destroy(const DynamicArray<RawCmd> &cmds, Registry *registry) noexcept {
    for (const RawCmd &cmd : cmds) {
        if (cmd.kind != CmdKind::DestroyPart)
            continue;
        const RawDestroyPartCmd &c = cmd.destroy_part;
        c.commit_destroy(static_cast<void *>(registry), c.thing);
    }
}

void commit_raw_destroy_inverse(const DynamicArray<RawCmd> &cmds, Byte *base,
                                Registry *registry) noexcept {
    for (USize i = cmds.size(); i-- > 0;) {
        const RawCmd &cmd = cmds[i];
        if (cmd.kind != CmdKind::DestroyPart) {
            continue;
        }

        const RawDestroyPartCmd &c = cmd.destroy_part;
        c.commit_insert_copy(static_cast<void *>(registry), c.thing, base + c.part_offset);
    }
}

void commit_raw_insert_move(const DynamicArray<RawCmd> &cmds, Byte *base,
                            Registry *registry) noexcept {
    for (const RawCmd &cmd : cmds) {
        if (cmd.kind != CmdKind::InsertPart) {
            continue;
        }

        const RawInsertPartCmd &c = cmd.insert_part;
        c.commit_insert_move(static_cast<void *>(registry), c.thing, base + c.part_offset);
    }
}

void commit_raw_insert_inverse(const DynamicArray<RawCmd> &cmds, Registry *registry) noexcept {
    for (USize i = cmds.size(); i-- > 0;) {
        const RawCmd &cmd = cmds[i];
        if (cmd.kind != CmdKind::InsertPart) {
            continue;
        }

        const RawInsertPartCmd &c = cmd.insert_part;
        c.commit_destroy(static_cast<void *>(registry), c.thing);
    }
}

void commit_raw_insert_copy(const DynamicArray<RawCmd> &cmds, Byte *base,
                            Registry *registry) noexcept {
    for (const RawCmd &cmd : cmds) {
        if (cmd.kind != CmdKind::InsertPart) {
            continue;
        }

        const RawInsertPartCmd &c = cmd.insert_part;
        c.commit_insert_copy(static_cast<void *>(registry), c.thing, base + c.part_offset);
    }
}

void commit_raw_mutate(const DynamicArray<RawCmd> &cmds, Byte *base, Registry *registry) noexcept {
    for (const RawCmd &cmd : cmds) {
        if (cmd.kind != CmdKind::MutatePart) {
            continue;
        }

        const RawMutatePartCmd &c = cmd.mutate_part;
        c.commit_mutate(static_cast<void *>(registry), c.thing, base + c.next_offset);
    }
}

void commit_raw_mutate_inverse(const DynamicArray<RawCmd> &cmds, Byte *base,
                               Registry *registry) noexcept {
    for (USize i = cmds.size(); i-- > 0;) {
        const RawCmd &cmd = cmds[i];
        if (cmd.kind != CmdKind::MutatePart) {
            continue;
        }

        const RawMutatePartCmd &c = cmd.mutate_part;
        c.commit_mutate(static_cast<void *>(registry), c.thing, base + c.prev_offset);
    }
}

void commit_raw_attach_child(const DynamicArray<RawCmd> &cmds, World *world) noexcept {
    for (const RawCmd &cmd : cmds) {
        if (cmd.kind != CmdKind::AttachChild) {
            continue;
        }

        world->attach_child_now(cmd.attach_child.parent, cmd.attach_child.child);
    }
}

void commit_raw_attach_child_inverse(const DynamicArray<RawCmd> &cmds, World *world) noexcept {
    for (USize i = cmds.size(); i-- > 0;) {
        const RawCmd &cmd = cmds[i];
        if (cmd.kind != CmdKind::AttachChild) {
            continue;
        }

        world->detach_child_now(cmd.attach_child.parent, cmd.attach_child.child);
    }
}

void commit_raw_detach_child(const DynamicArray<RawCmd> &cmds, World *world) noexcept {
    for (const RawCmd &cmd : cmds) {
        if (cmd.kind != CmdKind::DetachChild) {
            continue;
        }

        world->detach_child_now(cmd.detach_child.parent, cmd.detach_child.child);
    }
}

void commit_raw_detach_child_inverse(const DynamicArray<RawCmd> &cmds, World *world) noexcept {
    for (USize i = cmds.size(); i-- > 0;) {
        const RawCmd &cmd = cmds[i];
        if (cmd.kind != CmdKind::DetachChild) {
            continue;
        }

        world->attach_child_now(cmd.detach_child.parent, cmd.detach_child.child);
    }
}

void reset_raw_cmds(DynamicArray<RawCmd> &cmds, Byte *base, ArenaAlloc &arena) noexcept {
    for (const RawCmd &cmd : cmds) {
        if (cmd.kind == CmdKind::InsertPart) {
            cmd.insert_part.tidx.meta().destroy(base + cmd.insert_part.part_offset);
        } else if (cmd.kind == CmdKind::DestroyPart) {
            cmd.destroy_part.tidx.meta().destroy(base + cmd.destroy_part.part_offset);
        } else if (cmd.kind == CmdKind::MutatePart) {
            const TypeMeta &m = cmd.mutate_part.tidx.meta();
            m.destroy(base + cmd.mutate_part.prev_offset);
            m.destroy(base + cmd.mutate_part.next_offset);
        }
    }

    cmds.clear();
    arena.reset();
}

} // namespace impl

// ================================================================ CmdBatch

CmdBatch::CmdBatch() noexcept
    : CmdBatch(get_ambient_ctx().alloc) {
}

CmdBatch::CmdBatch(Alloc *alloc, USize arena_size) noexcept
    : m_alloc(alloc) {
    Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdBatch");
    m_cmds = DynamicArray<impl::RawCmd>::with_alloc(alloc);
    m_thing_cmds = HashMap<Thing, impl::ThingCmdEntry>::with_alloc(alloc);
}

CmdBatch::~CmdBatch() noexcept {
    reset();
    m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(), alignof(std::max_align_t));
}

void CmdBatch::reset() noexcept {
    impl::reset_raw_cmds(m_cmds, m_arena_buffer.data(), m_arena);
    m_thing_cmds.clear();
}

// ----------------------------------------------------------- CmdBatch: Collect

DynamicArray<impl::RawInsertPartCmd> CmdBatch::collect_insert_part_cmds() const noexcept {
    auto result = DynamicArray<impl::RawInsertPartCmd>::with_alloc(m_alloc);
    for (const impl::RawCmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::InsertPart) {
            result.push_back(cmd.insert_part);
        }
    }

    return result;
}

DynamicArray<impl::RawDestroyPartCmd> CmdBatch::collect_destroy_part_cmds() const noexcept {
    auto result = DynamicArray<impl::RawDestroyPartCmd>::with_alloc(m_alloc);
    for (const impl::RawCmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::DestroyPart) {
            result.push_back(cmd.destroy_part);
        }
    }

    return result;
}

DynamicArray<impl::RawMutatePartCmd> CmdBatch::collect_mutate_part_cmds() const noexcept {
    auto result = DynamicArray<impl::RawMutatePartCmd>::with_alloc(m_alloc);
    for (const impl::RawCmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::MutatePart) {
            result.push_back(cmd.mutate_part);
        }
    }

    return result;
}

DynamicArray<AttachChildCmd> CmdBatch::collect_attach_child_cmds() const noexcept {
    auto result = DynamicArray<AttachChildCmd>::with_alloc(m_alloc);
    for (const impl::RawCmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::AttachChild) {
            result.push_back(cmd.attach_child);
        }
    }

    return result;
}

DynamicArray<DetachChildCmd> CmdBatch::collect_detach_child_cmds() const noexcept {
    auto result = DynamicArray<DetachChildCmd>::with_alloc(m_alloc);
    for (const impl::RawCmd &cmd : m_cmds) {
        if (cmd.kind == CmdKind::DetachChild) {
            result.push_back(cmd.detach_child);
        }
    }

    return result;
}

// ------------------------------------------------------------ CmdBatch: Commit

void CmdBatch::commit_destroy(impl::Registry *registry) noexcept {
    impl::commit_raw_destroy(m_cmds, registry);
}

void CmdBatch::commit_destroy_inverse(impl::Registry *registry) noexcept {
    impl::commit_raw_destroy_inverse(m_cmds, m_arena_buffer.data(), registry);
}

void CmdBatch::commit_insert_move(impl::Registry *registry) noexcept {
    impl::commit_raw_insert_move(m_cmds, m_arena_buffer.data(), registry);
}

void CmdBatch::commit_insert_inverse(impl::Registry *registry) noexcept {
    impl::commit_raw_insert_inverse(m_cmds, registry);
}

void CmdBatch::commit_insert_copy(impl::Registry *registry) noexcept {
    impl::commit_raw_insert_copy(m_cmds, m_arena_buffer.data(), registry);
}

void CmdBatch::commit_mutate(impl::Registry *registry) noexcept {
    impl::commit_raw_mutate(m_cmds, m_arena_buffer.data(), registry);
}

void CmdBatch::commit_mutate_inverse(impl::Registry *registry) noexcept {
    impl::commit_raw_mutate_inverse(m_cmds, m_arena_buffer.data(), registry);
}

void CmdBatch::commit_attach_child(World *world) noexcept {
    impl::commit_raw_attach_child(m_cmds, world);
}

void CmdBatch::commit_attach_child_inverse(World *world) noexcept {
    impl::commit_raw_attach_child_inverse(m_cmds, world);
}

void CmdBatch::commit_detach_child(World *world) noexcept {
    impl::commit_raw_detach_child(m_cmds, world);
}

void CmdBatch::commit_detach_child_inverse(World *world) noexcept {
    impl::commit_raw_detach_child_inverse(m_cmds, world);
}

// ================================================================ CmdSheaf

CmdSheaf::CmdSheaf(Alloc *alloc, Thing thing, USize arena_size) noexcept
    : m_alloc(alloc),
      m_thing(thing) {
    Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));
    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdSheaf");
    m_cmds = DynamicArray<impl::RawCmd>::with_alloc(alloc);
}

CmdSheaf::CmdSheaf(Alloc *alloc, const CmdBatch &batch, Thing thing) noexcept
    : m_alloc(alloc),
      m_thing(thing) {
    const impl::ThingCmdEntry *entry = batch.m_thing_cmds.find(thing).unwrap_or(nullptr);
    const TypeRegistry *type_registry = get_ambient_ctx().type_registry;

    USize arena_size = 0;
    if (entry) {
        for (U8 i = 0; i < entry->count; ++i) {
            const impl::RawCmd &cmd = batch.m_cmds[entry->indices[i]];

            if (cmd.kind == CmdKind::InsertPart || cmd.kind == CmdKind::DestroyPart) {
                TypeIdx tidx =
                    cmd.kind == CmdKind::InsertPart ? cmd.insert_part.tidx : cmd.destroy_part.tidx;

                const TypeMeta &m = type_registry->meta(tidx);
                arena_size += m.size + m.alignment;
            } else if (cmd.kind == CmdKind::MutatePart) {
                const TypeMeta &m = type_registry->meta(cmd.mutate_part.tidx);
                arena_size += 2 * (m.size + m.alignment);
            }
        }
    }

    if (arena_size == 0) {
        arena_size = 1;
    }

    Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));

    m_arena_buffer = Slice<Byte>(buf, arena_size);
    m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdSheaf");
    m_cmds = DynamicArray<impl::RawCmd>::with_alloc(alloc);

    if (!entry || entry->count == 0) {
        return;
    }

    const Byte *src_base = batch.m_arena_buffer.data();
    for (U8 i = 0; i < entry->count; ++i) {
        const impl::RawCmd &src = batch.m_cmds[entry->indices[i]];
        impl::RawCmd dst = src;

        if (src.kind == CmdKind::InsertPart) {
            const TypeMeta &m = type_registry->meta(src.insert_part.tidx);
            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot extract CmdSheaf");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.insert_part.part_offset =
                static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());

            m.copy_construct(ptr, src_base + src.insert_part.part_offset);
        } else if (src.kind == CmdKind::DestroyPart) {
            const TypeMeta &m = type_registry->meta(src.destroy_part.tidx);
            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot extract CmdSheaf");

            void *ptr = m_arena.allocate(m.size, m.alignment);
            dst.destroy_part.part_offset =
                static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());

            m.copy_construct(ptr, src_base + src.destroy_part.part_offset);
        } else if (src.kind == CmdKind::MutatePart) {
            const TypeMeta &m = type_registry->meta(src.mutate_part.tidx);
            FR_ASSERT(m.copy_construct,
                      "part type is not copy-constructible; cannot extract CmdSheaf");

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

CmdSheaf::~CmdSheaf() noexcept {
    reset();
    m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(), alignof(std::max_align_t));
}

void CmdSheaf::reset() noexcept {
    impl::reset_raw_cmds(m_cmds, m_arena_buffer.data(), m_arena);
}

// ------------------------------------------------------------ CmdSheaf: Commit

void CmdSheaf::commit_destroy(impl::Registry *registry) noexcept {
    impl::commit_raw_destroy(m_cmds, registry);
}

void CmdSheaf::commit_destroy_inverse(impl::Registry *registry) noexcept {
    impl::commit_raw_destroy_inverse(m_cmds, m_arena_buffer.data(), registry);
}

void CmdSheaf::commit_insert_move(impl::Registry *registry) noexcept {
    impl::commit_raw_insert_move(m_cmds, m_arena_buffer.data(), registry);
}

void CmdSheaf::commit_insert_inverse(impl::Registry *registry) noexcept {
    impl::commit_raw_insert_inverse(m_cmds, registry);
}

void CmdSheaf::commit_insert_copy(impl::Registry *registry) noexcept {
    impl::commit_raw_insert_copy(m_cmds, m_arena_buffer.data(), registry);
}

void CmdSheaf::commit_mutate(impl::Registry *registry) noexcept {
    impl::commit_raw_mutate(m_cmds, m_arena_buffer.data(), registry);
}

void CmdSheaf::commit_mutate_inverse(impl::Registry *registry) noexcept {
    impl::commit_raw_mutate_inverse(m_cmds, m_arena_buffer.data(), registry);
}

void CmdSheaf::commit_attach_child(World *world) noexcept {
    impl::commit_raw_attach_child(m_cmds, world);
}

void CmdSheaf::commit_attach_child_inverse(World *world) noexcept {
    impl::commit_raw_attach_child_inverse(m_cmds, world);
}

void CmdSheaf::commit_detach_child(World *world) noexcept {
    impl::commit_raw_detach_child(m_cmds, world);
}

void CmdSheaf::commit_detach_child_inverse(World *world) noexcept {
    impl::commit_raw_detach_child_inverse(m_cmds, world);
}

} // namespace fr
