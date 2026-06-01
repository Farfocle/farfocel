/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Commands and command pool for the data layer.
 * @details Implements `fr::impl::CmdPool`, which allows for lazy, batched mutations of the world
 * state. Internally all recorded commands are stored in a flat `CmdSnapshot`:
 *   - Part data (for insert/mutate) is bump-allocated from a fixed-size arena.
 *   - Command headers are stored in a single `DynamicArray<RawCmd>`.
 */

#pragma once

#include <cstddef>
#include <utility>

#include "fr/core/arena_alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/part.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// =============================================================== Command Types

/**
 * @brief Different kinds of commands.
 */
enum class CmdKind : U8 { DestroyPart, InsertPart, MutatePart, AttachChild, DetachChild };

/**
 * @brief Command to destroy a part owned by a thing.
 * @tparam T Part type.
 */
template <typename T>
struct DestroyPartCmd {
    using Part = T;
    Thing thing;
};

/**
 * @brief Command to insert a part for a thing.
 * @tparam T Part type.
 */
template <typename T>
struct InsertPartCmd {
    using Part = T;
    Thing thing;
    Part part;
};

/**
 * @brief Command to mutate a part for a thing.
 * @tparam T Part type.
 */
template <typename T>
struct MutatePartCmd {
    using Part = T;
    Thing thing;
    Part prev;
    Part next;
};

/**
 * @brief Command to attach a child thing to a parent thing.
 */
struct AttachChildCmd {
    Thing parent;
    Thing child;
};

/**
 * @brief Command to detach a child thing from a parent thing.
 */
struct DetachChildCmd {
    Thing parent;
    Thing child;
};

} // namespace fr

// ================================================================ Command Pool

namespace fr::impl {

// ---------------------------------------------------------- Raw Command Types

/**
 * @brief Type-erased header for an insert command.
 * @details `part_ptr` points into the `CmdSnapshot` arena. `destroy_part` is always called on it
 * during `CmdPool::reset()` — safe whether the part was committed (moved-from, dtor still valid)
 * or not (uncommitted, dtor destroys the live object).
 */
struct RawInsertPartCmd {
    TypeIdx tidx;
    Thing thing;
    void *part_ptr;
    void (*destroy_part)(void *) noexcept;
};

/**
 * @brief Type-erased header for a destroy command.
 * @details No arena data — the part lives in the registry.
 */
struct RawDestroyPartCmd {
    TypeIdx tidx;
    Thing thing;
};

/**
 * @brief Type-erased header for a mutate command.
 * @details Both `prev_ptr` and `next_ptr` point into the `CmdSnapshot` arena.
 */
struct RawMutatePartCmd {
    TypeIdx tidx;
    Thing thing;
    void *prev_ptr;
    void *next_ptr;
    void (*destroy_part)(void *) noexcept;
};

/**
 * @brief Discriminated union of all raw command headers.
 * @details Relation commands reuse their public typed structs directly since they carry no
 * type-erased data and do not need separate Raw variants.
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

// ---------------------------------------------------------------- CmdSnapshot

/**
 * @brief All recorded command data for one commit cycle.
 * @details `arena` provides fast bump-pointer storage for part copies; `cmds` is the ordered flat
 * list of command headers. Contains all state needed to replay commands deterministically.
 */
struct CmdSnapshot {
    ArenaAlloc arena{};
    DynamicArray<RawCmd> cmds{};
};

// ------------------------------------------------------------------ CmdPool

/**
 * @brief Records and commits deferred commands against a registry.
 * @details Uses a fixed-size arena for part data and a single flat `DynamicArray<RawCmd>` for
 * command headers. Call `reset()` once after all commits to destroy arena objects and clear the
 * command list in one shot.
 */
class CmdPool {
public:
    // ----------------------------------------- Constants & Constructors & Destructor

    /// @brief Default arena size (64 KiB). Tune via `CmdPool(alloc, arena_size)` if needed.
    static constexpr USize DEFAULT_ARENA_SIZE = 64 * 1024;

    /**
     * @brief Construct using the ambient allocator and the default arena size.
     */
    CmdPool() noexcept
        : CmdPool(get_ambient_ctx().alloc) {
    }

    /**
     * @brief Construct with an explicit allocator and arena size.
     * @param alloc       Allocator for both the arena backing buffer and the cmd list.
     * @param arena_size  Size in bytes of the arena; tune based on expected part data per frame.
     * @pre `alloc` must be non-null and `arena_size` must be non-zero.
     */
    explicit CmdPool(Alloc *alloc, USize arena_size = DEFAULT_ARENA_SIZE) noexcept
        : m_alloc(alloc) {
        FR_ASSERT(alloc, "allocator must be non-null");
        FR_ASSERT(arena_size > 0, "arena size must be non-zero");

        Byte *buf = static_cast<Byte *>(alloc->allocate(arena_size, alignof(std::max_align_t)));
        m_arena_buffer = Slice<Byte>(buf, arena_size);
        m_snapshot.arena = ArenaAlloc(m_arena_buffer.data(), m_arena_buffer.size(), "CmdPool");
        m_snapshot.cmds = DynamicArray<RawCmd>::with_alloc(alloc);
    }

    /**
     * @brief Destroys any pending arena objects and frees the arena buffer.
     */
    ~CmdPool() noexcept {
        reset();
        m_alloc->deallocate(m_arena_buffer.data(), m_arena_buffer.size(),
                            alignof(std::max_align_t));
    }

    CmdPool(const CmdPool &) = delete;
    CmdPool(CmdPool &&) = delete;
    CmdPool &operator=(const CmdPool &) = delete;
    CmdPool &operator=(CmdPool &&) = delete;

    // ---------------------------------------------------------- Record Commands

    /**
     * @brief Records a destroy command for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or does not have part `T`.
     */
    template <typename T, typename RegistryT>
    void record_destroy(RegistryT &registry, Thing thing) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }
        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }
        if (!registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        TypeIdx tidx = TypeIdx::from_type<T>();
        do_ensure_commit_fns<T, RegistryT>(tidx);

        RawCmd cmd{};
        cmd.kind = CmdKind::DestroyPart;
        cmd.destroy_part = RawDestroyPartCmd{.tidx = tidx, .thing = thing};
        m_snapshot.cmds.push_back(cmd);
    }

    /**
     * @brief Records an insert command for part `T` (copy) on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
     */
    template <typename T, typename RegistryT>
    void record_insert(RegistryT &registry, Thing thing, const T &part) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }
        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }
        if (registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        TypeIdx tidx = TypeIdx::from_type<T>();
        do_ensure_commit_fns<T, RegistryT>(tidx);

        void *part_ptr = m_snapshot.arena.allocate(sizeof(T), alignof(T));
        new (part_ptr) T(part);

        RawCmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = RawInsertPartCmd{
            .tidx = tidx,
            .thing = thing,
            .part_ptr = part_ptr,
            .destroy_part = &TypeInfo<T>::destroy,
        };
        m_snapshot.cmds.push_back(cmd);
    }

    /**
     * @brief Records an insert command for part `T` (move) on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
     */
    template <typename T, typename RegistryT>
    void record_insert(RegistryT &registry, Thing thing, T &&part) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }
        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }
        if (registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        TypeIdx tidx = TypeIdx::from_type<T>();
        do_ensure_commit_fns<T, RegistryT>(tidx);

        void *part_ptr = m_snapshot.arena.allocate(sizeof(T), alignof(T));
        new (part_ptr) T(std::move(part));

        RawCmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = RawInsertPartCmd{
            .tidx = tidx,
            .thing = thing,
            .part_ptr = part_ptr,
            .destroy_part = &TypeInfo<T>::destroy,
        };
        m_snapshot.cmds.push_back(cmd);
    }

    /**
     * @brief Records a mutate command (prev → next) for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or does not have part `T`.
     */
    template <typename T, typename RegistryT>
    void record_mutate(RegistryT &registry, Thing thing, const T &prev, const T &next) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }
        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }
        if (!registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        TypeIdx tidx = TypeIdx::from_type<T>();
        do_ensure_commit_fns<T, RegistryT>(tidx);

        void *prev_ptr = m_snapshot.arena.allocate(sizeof(T), alignof(T));
        new (prev_ptr) T(prev);

        void *next_ptr = m_snapshot.arena.allocate(sizeof(T), alignof(T));
        new (next_ptr) T(next);

        RawCmd cmd{};
        cmd.kind = CmdKind::MutatePart;
        cmd.mutate_part = RawMutatePartCmd{
            .tidx = tidx,
            .thing = thing,
            .prev_ptr = prev_ptr,
            .next_ptr = next_ptr,
            .destroy_part = &TypeInfo<T>::destroy,
        };
        m_snapshot.cmds.push_back(cmd);
    }

    /**
     * @brief Records an attach-child relation command.
     */
    void record_attach_child(Thing parent, Thing child) noexcept {
        RawCmd cmd{};
        cmd.kind = CmdKind::AttachChild;
        cmd.attach_child = AttachChildCmd{.parent = parent, .child = child};
        m_snapshot.cmds.push_back(cmd);
    }

    /**
     * @brief Records a detach-child relation command.
     */
    void record_detach_child(Thing parent, Thing child) noexcept {
        RawCmd cmd{};
        cmd.kind = CmdKind::DetachChild;
        cmd.detach_child = DetachChildCmd{.parent = parent, .child = child};
        m_snapshot.cmds.push_back(cmd);
    }

    // ---------------------------------------------------------- Commit Commands

    /**
     * @brief Commits all destroy commands: calls `destroy_checked<T>` for each.
     */
    template <typename RegistryT>
    void commit_destroy_all(RegistryT *registry) noexcept {
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind != CmdKind::DestroyPart) {
                continue;
            }
            const RawDestroyPartCmd &c = cmd.destroy_part;
            if (m_commit_destroy_fns[c.tidx.idx()]) {
                m_commit_destroy_fns[c.tidx.idx()](static_cast<void *>(registry), c.thing);
            }
        }
    }

    /**
     * @brief Commits all insert commands: moves arena part data into the registry.
     */
    template <typename RegistryT>
    void commit_insert_all(RegistryT *registry) noexcept {
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind != CmdKind::InsertPart) {
                continue;
            }
            const RawInsertPartCmd &c = cmd.insert_part;
            if (m_commit_insert_fns[c.tidx.idx()]) {
                m_commit_insert_fns[c.tidx.idx()](static_cast<void *>(registry), c.thing,
                                                  c.part_ptr);
            }
        }
    }

    /**
     * @brief Commits all mutate commands: moves the `next` state from the arena into the registry.
     */
    template <typename RegistryT>
    void commit_mutate_all(RegistryT *registry) noexcept {
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind != CmdKind::MutatePart) {
                continue;
            }
            const RawMutatePartCmd &c = cmd.mutate_part;
            if (m_commit_mutate_fns[c.tidx.idx()]) {
                m_commit_mutate_fns[c.tidx.idx()](static_cast<void *>(registry), c.thing,
                                                  c.next_ptr);
            }
        }
    }

    /**
     * @brief Commits all attach-child commands.
     * @tparam WorldT Any type exposing `attach_child_now(Thing, Thing) noexcept`.
     */
    template <typename WorldT>
    void commit_attach_child_all(WorldT *world) noexcept {
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind != CmdKind::AttachChild) {
                continue;
            }
            world->attach_child_now(cmd.attach_child.parent, cmd.attach_child.child);
        }
    }

    /**
     * @brief Commits all detach-child commands.
     * @tparam WorldT Any type exposing `detach_child_now(Thing, Thing) noexcept`.
     */
    template <typename WorldT>
    void commit_detach_child_all(WorldT *world) noexcept {
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind != CmdKind::DetachChild) {
                continue;
            }
            world->detach_child_now(cmd.detach_child.parent, cmd.detach_child.child);
        }
    }

    /**
     * @brief Destroys all arena-allocated part objects and clears the command list.
     * @details Must be called once after all commit functions. Safe to call with uncommitted
     * commands — arena objects are properly destroyed regardless.
     */
    void reset() noexcept {
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind == CmdKind::InsertPart) {
                cmd.insert_part.destroy_part(cmd.insert_part.part_ptr);
            } else if (cmd.kind == CmdKind::MutatePart) {
                cmd.mutate_part.destroy_part(cmd.mutate_part.prev_ptr);
                cmd.mutate_part.destroy_part(cmd.mutate_part.next_ptr);
            }
        }
        m_snapshot.cmds.clear();
        m_snapshot.arena.reset();
    }

    // ------------------------------------------------------- Collect (Debug)

    /**
     * @brief Returns a snapshot of all pending insert-part command headers.
     * @note Allocates using the pool's own allocator. Intended for debugging and inspection.
     */
    DynamicArray<RawInsertPartCmd> collect_insert_part_cmds() const noexcept {
        DynamicArray<RawInsertPartCmd> result = DynamicArray<RawInsertPartCmd>::with_alloc(m_alloc);
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind == CmdKind::InsertPart) {
                result.push_back(cmd.insert_part);
            }
        }
        return result;
    }

    /**
     * @brief Returns a snapshot of all pending destroy-part command headers.
     * @note Allocates using the pool's own allocator. Intended for debugging and inspection.
     */
    DynamicArray<RawDestroyPartCmd> collect_destroy_part_cmds() const noexcept {
        DynamicArray<RawDestroyPartCmd> result =
            DynamicArray<RawDestroyPartCmd>::with_alloc(m_alloc);
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind == CmdKind::DestroyPart) {
                result.push_back(cmd.destroy_part);
            }
        }
        return result;
    }

    /**
     * @brief Returns a snapshot of all pending mutate-part command headers.
     * @note Allocates using the pool's own allocator. Intended for debugging and inspection.
     */
    DynamicArray<RawMutatePartCmd> collect_mutate_part_cmds() const noexcept {
        DynamicArray<RawMutatePartCmd> result = DynamicArray<RawMutatePartCmd>::with_alloc(m_alloc);
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind == CmdKind::MutatePart) {
                result.push_back(cmd.mutate_part);
            }
        }
        return result;
    }

    /**
     * @brief Returns a snapshot of all pending attach-child commands.
     * @note Allocates using the pool's own allocator. Intended for debugging and inspection.
     */
    DynamicArray<AttachChildCmd> collect_attach_child_cmds() const noexcept {
        DynamicArray<AttachChildCmd> result = DynamicArray<AttachChildCmd>::with_alloc(m_alloc);
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind == CmdKind::AttachChild) {
                result.push_back(cmd.attach_child);
            }
        }
        return result;
    }

    /**
     * @brief Returns a snapshot of all pending detach-child commands.
     * @note Allocates using the pool's own allocator. Intended for debugging and inspection.
     */
    DynamicArray<DetachChildCmd> collect_detach_child_cmds() const noexcept {
        DynamicArray<DetachChildCmd> result = DynamicArray<DetachChildCmd>::with_alloc(m_alloc);
        for (const RawCmd &cmd : m_snapshot.cmds) {
            if (cmd.kind == CmdKind::DetachChild) {
                result.push_back(cmd.detach_child);
            }
        }
        return result;
    }

private:
    // --------------------------------------------------------- Internal Methods

    using CommitInsertFn = void (*)(void *registry, Thing thing, void *part_ptr) noexcept;
    using CommitDestroyFn = void (*)(void *registry, Thing thing) noexcept;
    using CommitMutateFn = void (*)(void *registry, Thing thing, void *next_ptr) noexcept;

    /**
     * @brief Registers the three commit functions for type T the first time a cmd of that type
     * is recorded. All three are always set together so any of them serves as the "is
     * initialized" sentinel.
     */
    template <typename T, typename RegistryT>
    void do_ensure_commit_fns(TypeIdx tidx) noexcept {
        FR_ASSERT(tidx.idx() < MAX_PARTS, "invalid part type index");
        if (m_commit_insert_fns[tidx.idx()]) {
            return;
        }
        m_commit_insert_fns[tidx.idx()] = &CmdPool::do_commit_insert<T, RegistryT>;
        m_commit_destroy_fns[tidx.idx()] = &CmdPool::do_commit_destroy<T, RegistryT>;
        m_commit_mutate_fns[tidx.idx()] = &CmdPool::do_commit_mutate<T, RegistryT>;
    }

    template <typename T, typename RegistryT>
    static void do_commit_insert(void *registry_ptr, Thing thing, void *part_ptr) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        T *part = static_cast<T *>(part_ptr);
        registry->template emplace_checked<T>(thing, std::move(*part));
    }

    template <typename T, typename RegistryT>
    static void do_commit_destroy(void *registry_ptr, Thing thing) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        registry->template destroy_checked<T>(thing);
    }

    template <typename T, typename RegistryT>
    static void do_commit_mutate(void *registry_ptr, Thing thing, void *next_ptr) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        T *part = registry->template get_checked<T>(thing);
        if (part) [[likely]] {
            *part = std::move(*static_cast<T *>(next_ptr));
        }
    }

    // --------------------------------------------------------- Member Variables

    Alloc *m_alloc{nullptr};
    Slice<Byte> m_arena_buffer{};
    CmdSnapshot m_snapshot{};
    Array<CommitInsertFn, MAX_PARTS> m_commit_insert_fns{};
    Array<CommitDestroyFn, MAX_PARTS> m_commit_destroy_fns{};
    Array<CommitMutateFn, MAX_PARTS> m_commit_mutate_fns{};
};

} // namespace fr::impl
