/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Commands and command batch for the data layer.
 */

#pragma once

#include <cstddef>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// ================================================================ Command Types

/**
 * @brief Different kinds of commands.
 */
enum class CmdKind : U8 { DestroyPart, InsertPart, MutatePart, AttachChild, DetachChild };

/**
 * @brief Typed view of a recorded destroy command.
 */
template <typename T>
struct DestroyPartCmd {
    using Part = T;
    Thing thing;
    Part part;
};

/**
 * @brief Typed view of a recorded insert command.
 */
template <typename T>
struct InsertPartCmd {
    using Part = T;
    Thing thing;
    Part part;
};

/**
 * @brief Typed view of a recorded mutate command.
 */
template <typename T>
struct MutatePartCmd {
    using Part = T;
    Thing thing;
    Part prev;
    Part next;
};

/**
 * @brief Typed view of a recorded attach-child command.
 */
struct AttachChildCmd {
    Thing parent;
    Thing child;
};

/**
 * @brief Typed view of a recorded detach-child command.
 */
struct DetachChildCmd {
    Thing parent;
    Thing child;
};

// Forward declaration to break the world.hpp <-> cmd.hpp cycle for relation commits.
class World;

} // namespace fr

// ================================================================ Raw Command Types

namespace fr::impl {

/**
 * @brief Type-erased insert command header.
 */
struct RawInsertPartCmd {
    TypeIdx tidx;
    Thing thing;
    void *part_ptr;
    void (*commit_insert_move)(void *registry, Thing, void *part_ptr) noexcept;
    void (*commit_insert_copy)(void *registry, Thing, void *part_ptr) noexcept;
    void (*commit_destroy)(void *registry, Thing) noexcept;
    void (*destroy_part)(void *) noexcept;
};

/**
 * @brief Type-erased destroy command header.
 */
struct RawDestroyPartCmd {
    TypeIdx tidx;
    Thing thing;
    void *part_ptr;
    void (*commit_destroy)(void *registry, Thing) noexcept;
    void (*commit_insert_move)(void *registry, Thing, void *part_ptr) noexcept;
    void (*commit_insert_copy)(void *registry, Thing, void *part_ptr) noexcept;
    void (*destroy_part)(void *) noexcept;
};

/**
 * @brief Type-erased mutate command header.
 */
struct RawMutatePartCmd {
    TypeIdx tidx;
    Thing thing;
    void *prev_ptr;
    void *next_ptr;
    void (*commit_mutate)(void *registry, Thing, void *part_ptr) noexcept;
    void (*destroy_part)(void *) noexcept;
};

/**
 * @brief Discriminated union over all raw command headers.
 * @note Relation commands reuse their public typed structs directly.
 */
struct RawCmd {
    CmdKind kind;
    union {
        RawInsertPartCmd insert_part;
        RawDestroyPartCmd destroy_part;
        RawMutatePartCmd mutate_part;
        AttachChildCmd attach_child;
        DetachChildCmd detach_child;
    };
};

} // namespace fr::impl

// ================================================================ CmdBatch

namespace fr {

/**
 * @brief Self-contained unit of deferred commands.
 */
class CmdBatch {
public:
    static constexpr USize DEFAULT_ARENA_SIZE = 64 * 1024;

    CmdBatch() noexcept
        : CmdBatch(get_ambient_ctx().alloc) {
    }

    explicit CmdBatch(Alloc *alloc, USize arena_size = DEFAULT_ARENA_SIZE) noexcept
        : m_alloc(alloc) {
        FR_ASSERT(alloc != nullptr, "allocator must be non-null");
        FR_ASSERT(arena_size > 0, "arena size must be non-zero");

        Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));
        m_arena_buffer = Slice<Byte>(buf, arena_size);
        m_arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdBatch");
        m_cmds = DynamicArray<impl::RawCmd>::with_alloc(alloc);
    }

    ~CmdBatch() noexcept {
        reset();
        m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(),
                            alignof(std::max_align_t));
    }

    CmdBatch(const CmdBatch &) = delete;
    CmdBatch(CmdBatch &&) = delete;
    CmdBatch &operator=(const CmdBatch &) = delete;
    CmdBatch &operator=(CmdBatch &&) = delete;

    // ----------------------------------------------------------------- Record

    /**
     * @brief Records a destroy command for part `T` on a thing.
     * Captures the current part value into the arena.
     * @note Does nothing if thing is nil, dead, or does not have part `T`.
     */
    template <typename T>
    void record_destroy(impl::Registry &registry, Thing thing) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        T *current = registry.get_checked<T>(thing);
        if (!current) [[unlikely]] {
            return;
        }

        void *part_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (part_ptr) T(*current);

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::DestroyPart;
        cmd.destroy_part = impl::RawDestroyPartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .part_ptr = part_ptr,
            .commit_destroy = &CmdBatch::do_commit_destroy<T>,
            .commit_insert_move = &CmdBatch::do_commit_insert_move<T>,
            .commit_insert_copy = &CmdBatch::do_commit_insert_copy<T>,
            .destroy_part = &TypeInfo<T>::destroy,
        };

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records an insert command for part `T` (copy) on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
     */
    template <typename T>
    void record_insert(impl::Registry &registry, Thing thing, const T &part) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        if (registry.has<T>(thing)) [[unlikely]] {
            return;
        }

        void *part_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (part_ptr) T(part);

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = impl::RawInsertPartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .part_ptr = part_ptr,
            .commit_insert_move = &CmdBatch::do_commit_insert_move<T>,
            .commit_insert_copy = &CmdBatch::do_commit_insert_copy<T>,
            .commit_destroy = &CmdBatch::do_commit_destroy<T>,
            .destroy_part = &TypeInfo<T>::destroy,
        };

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records an insert command for part `T` (move) on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
     */
    template <typename T>
    void record_insert(impl::Registry &registry, Thing thing, T &&part) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        if (registry.has<T>(thing)) [[unlikely]] {
            return;
        }

        void *part_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (part_ptr) T(std::move(part));

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = impl::RawInsertPartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .part_ptr = part_ptr,
            .commit_insert_move = &CmdBatch::do_commit_insert_move<T>,
            .commit_insert_copy = &CmdBatch::do_commit_insert_copy<T>,
            .commit_destroy = &CmdBatch::do_commit_destroy<T>,
            .destroy_part = &TypeInfo<T>::destroy,
        };

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records a mutate command (prev -> next) for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or does not have part `T`.
     */
    template <typename T>
    void record_mutate(impl::Registry &registry, Thing thing, const T &prev,
                       const T &next) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        if (!registry.has<T>(thing)) [[unlikely]] {
            return;
        }

        void *prev_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (prev_ptr) T(prev);

        void *next_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (next_ptr) T(next);

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::MutatePart;
        cmd.mutate_part = impl::RawMutatePartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .prev_ptr = prev_ptr,
            .next_ptr = next_ptr,
            .commit_mutate = &CmdBatch::do_commit_mutate<T>,
            .destroy_part = &TypeInfo<T>::destroy,
        };

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records an attach-child relation command.
     */
    void record_attach_child(Thing parent, Thing child) noexcept {
        impl::RawCmd cmd{};
        cmd.kind = CmdKind::AttachChild;
        cmd.attach_child = AttachChildCmd{.parent = parent, .child = child};
        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records a detach-child relation command.
     */
    void record_detach_child(Thing parent, Thing child) noexcept {
        impl::RawCmd cmd{};
        cmd.kind = CmdKind::DetachChild;
        cmd.detach_child = DetachChildCmd{.parent = parent, .child = child};
        m_cmds.push_back(cmd);
    }

    // ----------------------------------------------------------------- Commit

    /**
     * @brief Commits all destroy commands.
     */
    void commit_destroy_all(impl::Registry *registry) noexcept {
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind != CmdKind::DestroyPart) {
                continue;
            }

            const impl::RawDestroyPartCmd &c = cmd.destroy_part;
            c.commit_destroy(static_cast<void *>(registry), c.thing);
        }
    }

    /**
     * @brief Commits all insert commands by moving arena data into the registry.
     * @note Arena copies are moved-from after this. Do not call again on this batch.
     */
    void commit_insert_all_move(impl::Registry *registry) noexcept {
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind != CmdKind::InsertPart) {
                continue;
            }

            const impl::RawInsertPartCmd &c = cmd.insert_part;
            c.commit_insert_move(static_cast<void *>(registry), c.thing, c.part_ptr);
        }
    }

    /**
     * @brief Commits all insert commands by copying arena data into the registry.
     * @note Arena copies remain intact; the batch can be committed again.
     */
    void commit_insert_all_copy(impl::Registry *registry) noexcept {
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind != CmdKind::InsertPart) {
                continue;
            }

            const impl::RawInsertPartCmd &c = cmd.insert_part;
            c.commit_insert_copy(static_cast<void *>(registry), c.thing, c.part_ptr);
        }
    }

    /**
     * @brief Commits all mutate commands (applies next state).
     */
    void commit_mutate_all(impl::Registry *registry) noexcept {
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind != CmdKind::MutatePart) {
                continue;
            }

            const impl::RawMutatePartCmd &c = cmd.mutate_part;
            c.commit_mutate(static_cast<void *>(registry), c.thing, c.next_ptr);
        }
    }

    /**
     * @brief Commits all attach-child commands.
     * Implemented in cmd.cpp to avoid a circular include with world.hpp.
     */
    void commit_attach_child_all(World *world) noexcept;

    /**
     * @brief Commits all detach-child commands.
     * Implemented in cmd.cpp to avoid a circular include with world.hpp.
     */
    void commit_detach_child_all(World *world) noexcept;

    /**
     * @brief Destroys all arena-allocated part objects and clears the command list.
     * Safe to call with uncommitted commands - arena objects are properly destroyed regardless.
     */
    void reset() noexcept {
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind == CmdKind::InsertPart) {
                cmd.insert_part.destroy_part(cmd.insert_part.part_ptr);
            } else if (cmd.kind == CmdKind::DestroyPart) {
                cmd.destroy_part.destroy_part(cmd.destroy_part.part_ptr);
            } else if (cmd.kind == CmdKind::MutatePart) {
                cmd.mutate_part.destroy_part(cmd.mutate_part.prev_ptr);
                cmd.mutate_part.destroy_part(cmd.mutate_part.next_ptr);
            }
        }

        m_cmds.clear();
        m_arena.reset();
    }

    // ----------------------------------------------------------------- Collect

    DynamicArray<impl::RawInsertPartCmd> collect_insert_part_cmds() const noexcept {
        auto result = DynamicArray<impl::RawInsertPartCmd>::with_alloc(m_alloc);
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind == CmdKind::InsertPart)
                result.push_back(cmd.insert_part);
        }
        return result;
    }

    DynamicArray<impl::RawDestroyPartCmd> collect_destroy_part_cmds() const noexcept {
        auto result = DynamicArray<impl::RawDestroyPartCmd>::with_alloc(m_alloc);
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind == CmdKind::DestroyPart) {
                result.push_back(cmd.destroy_part);
            }
        }

        return result;
    }

    DynamicArray<impl::RawMutatePartCmd> collect_mutate_part_cmds() const noexcept {
        auto result = DynamicArray<impl::RawMutatePartCmd>::with_alloc(m_alloc);
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind == CmdKind::MutatePart) {
                result.push_back(cmd.mutate_part);
            }
        }

        return result;
    }

    DynamicArray<AttachChildCmd> collect_attach_child_cmds() const noexcept {
        auto result = DynamicArray<AttachChildCmd>::with_alloc(m_alloc);
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind == CmdKind::AttachChild) {
                result.push_back(cmd.attach_child);
            }
        }

        return result;
    }

    DynamicArray<DetachChildCmd> collect_detach_child_cmds() const noexcept {
        auto result = DynamicArray<DetachChildCmd>::with_alloc(m_alloc);
        for (const impl::RawCmd &cmd : m_cmds) {
            if (cmd.kind == CmdKind::DetachChild) {
                result.push_back(cmd.detach_child);
            }
        }

        return result;
    }

private:
    template <typename T>
    static void do_commit_insert_move(void *registry_ptr, Thing thing, void *part_ptr) noexcept {
        impl::Registry *registry = static_cast<impl::Registry *>(registry_ptr);
        T *part = static_cast<T *>(part_ptr);
        registry->emplace_checked<T>(thing, std::move(*part));
    }

    template <typename T>
    static void do_commit_insert_copy(void *registry_ptr, Thing thing, void *part_ptr) noexcept {
        impl::Registry *registry = static_cast<impl::Registry *>(registry_ptr);
        T *part = static_cast<T *>(part_ptr);
        registry->emplace_checked<T>(thing, *part);
    }

    template <typename T>
    static void do_commit_destroy(void *registry_ptr, Thing thing) noexcept {
        impl::Registry *registry = static_cast<impl::Registry *>(registry_ptr);
        registry->destroy_checked<T>(thing);
    }

    template <typename T>
    static void do_commit_mutate(void *registry_ptr, Thing thing, void *part_ptr) noexcept {
        impl::Registry *registry = static_cast<impl::Registry *>(registry_ptr);
        T *part = registry->get_checked<T>(thing);
        if (part) [[likely]] {
            *part = std::move(*static_cast<T *>(part_ptr));
        }
    }

    Alloc *m_alloc{nullptr};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<impl::RawCmd> m_cmds{};
};

} // namespace fr
