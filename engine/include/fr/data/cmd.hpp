/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Commands and command batch for the data layer.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// ================================================================ Command Types

/// @brief Maximum number of commands in a CmdBatch.
static constexpr USize MAX_CMDS = 1 << 16;

/// @brief Maximum number of commands recorded per thing in a CmdBatch.
static constexpr USize MAX_CMDS_PER_THING = 16;

/// @brief Index into the CmdBatch command list.
using CmdIdx = U16;

enum class CmdKind : U8 { Nil, DestroyPart, InsertPart, MutatePart, AttachChild, DetachChild };

inline CmdKind nil(CmdKind *) noexcept {
    return CmdKind::Nil;
}

inline bool is_nil(CmdKind v) noexcept {
    return v == CmdKind::Nil;
}

template <typename T>
struct DestroyPartCmd;

template <typename T>

struct InsertPartCmd;
template <typename T>
struct MutatePartCmd;

struct AttachChildCmd;
struct DetachChildCmd;

template <typename T>
struct DestroyPartCmd {
    Thing thing;
    T part;

    InsertPartCmd<T> inverse() const noexcept {
        return {thing, part};
    }

    static DestroyPartCmd nil() noexcept
        requires std::is_nothrow_default_constructible_v<T>
    {
        return {Thing::nil(), T{}};
    }

    bool is_nil() const noexcept {
        return thing.is_nil();
    }
};

template <typename T>
struct InsertPartCmd {
    Thing thing;
    T part;

    DestroyPartCmd<T> inverse() const noexcept {
        return {thing, part};
    }

    static InsertPartCmd nil() noexcept
        requires std::is_nothrow_default_constructible_v<T>
    {
        return {Thing::nil(), T{}};
    }

    bool is_nil() const noexcept {
        return thing.is_nil();
    }
};

template <typename T>
struct MutatePartCmd {
    Thing thing;
    T prev;
    T next;

    MutatePartCmd<T> inverse() const noexcept {
        return {thing, next, prev};
    }

    static MutatePartCmd nil() noexcept
        requires std::is_nothrow_default_constructible_v<T>
    {
        return {Thing::nil(), T{}, T{}};
    }

    bool is_nil() const noexcept {
        return thing.is_nil();
    }
};

struct AttachChildCmd {
    Thing parent;
    Thing child;

    DetachChildCmd inverse() const noexcept;

    static AttachChildCmd nil() noexcept {
        return {Thing::nil(), Thing::nil()};
    }

    bool is_nil() const noexcept {
        return parent.is_nil();
    }
};

struct DetachChildCmd {
    Thing parent;
    Thing child;

    AttachChildCmd inverse() const noexcept;

    static DetachChildCmd nil() noexcept {
        return {Thing::nil(), Thing::nil()};
    }

    bool is_nil() const noexcept {
        return parent.is_nil();
    }
};

class World;
class CmdSheaf;
} // namespace fr

// ================================================================ Raw Command Types

namespace fr::impl {

struct RawInsertPartCmd;
struct RawDestroyPartCmd;
struct RawMutatePartCmd;

struct RawInsertPartCmd {
    TypeIdx tidx;
    Thing thing;
    USize part_offset; ///< Byte offset of the part in the arena buffer.
    void (*commit_insert_move)(void *registry, Thing, void *part_ptr) noexcept;
    void (*commit_insert_copy)(void *registry, Thing, void *part_ptr) noexcept;
    void (*commit_destroy)(void *registry, Thing) noexcept;

    RawDestroyPartCmd invert() const noexcept;

    /// @brief Cast the part pointer from the arena. Returns nullptr on type mismatch.
    template <typename T>
    T *try_cast_part(Byte *arena_base) noexcept {
        if (TypeIdx::from_type<T>() != tidx) {
            return nullptr;
        }

        return static_cast<T *>(static_cast<void *>(arena_base + part_offset));
    }
};

struct RawDestroyPartCmd {
    TypeIdx tidx;
    Thing thing;
    USize part_offset;
    void (*commit_destroy)(void *registry, Thing) noexcept;
    void (*commit_insert_move)(void *registry, Thing, void *part_ptr) noexcept;
    void (*commit_insert_copy)(void *registry, Thing, void *part_ptr) noexcept;

    RawInsertPartCmd invert() const noexcept;

    /// @brief Cast the part pointer from the arena. Returns nullptr on type mismatch.
    template <typename T>
    T *try_cast_part(Byte *arena_base) noexcept {
        if (TypeIdx::from_type<T>() != tidx) {
            return nullptr;
        }

        return static_cast<T *>(static_cast<void *>(arena_base + part_offset));
    }
};

struct RawMutatePartCmd {
    TypeIdx tidx;
    Thing thing;
    USize prev_offset;
    USize next_offset;
    void (*commit_mutate)(void *registry, Thing, void *part_ptr) noexcept;

    RawMutatePartCmd invert() const noexcept;

    template <typename T>
    T *try_cast_prev(Byte *arena_base) noexcept {
        if (TypeIdx::from_type<T>() != tidx) {
            return nullptr;
        }

        return static_cast<T *>(static_cast<void *>(arena_base + prev_offset));
    }

    template <typename T>
    T *try_cast_next(Byte *arena_base) noexcept {
        if (TypeIdx::from_type<T>() != tidx) {
            return nullptr;
        }

        return static_cast<T *>(static_cast<void *>(arena_base + next_offset));
    }
};

struct RawCmd {
    CmdKind kind{CmdKind::Nil};
    union {
        RawInsertPartCmd insert_part;
        RawDestroyPartCmd destroy_part;
        RawMutatePartCmd mutate_part;
        AttachChildCmd attach_child;
        DetachChildCmd detach_child;
    };

    static RawCmd nil() noexcept {
        RawCmd cmd{};
        cmd.kind = CmdKind::Nil;
        return cmd;
    }

    bool is_nil() const noexcept {
        return kind == CmdKind::Nil;
    }
};

/// @brief Tracks which command indices belong to a given thing.
struct ThingCmdEntry {
    Array<CmdIdx, MAX_CMDS_PER_THING> indices{};
    U8 count{0};
};

// ================================================= Shared Commit Helpers (cmd.cpp)

/// @brief Commits all DestroyPart commands — removes each part from the registry.
void commit_raw_destroy(const DynamicArray<RawCmd> &cmds, Registry *registry) noexcept;

/// @brief Commits DestroyPart inverses in reverse order — re-inserts parts via copy.
void commit_raw_destroy_inverse(const DynamicArray<RawCmd> &cmds, Byte *base,
                                Registry *registry) noexcept;

/// @brief Commits InsertPart commands by moving each part from the arena into the registry.
void commit_raw_insert_move(const DynamicArray<RawCmd> &cmds, Byte *base,
                            Registry *registry) noexcept;

/// @brief Commits InsertPart inverses in reverse order — removes inserted parts.
void commit_raw_insert_inverse(const DynamicArray<RawCmd> &cmds, Registry *registry) noexcept;

/// @brief Commits InsertPart commands by copying each part from the arena into the registry.
void commit_raw_insert_copy(const DynamicArray<RawCmd> &cmds, Byte *base,
                            Registry *registry) noexcept;

/// @brief Commits MutatePart commands by writing the next-state part into the registry.
void commit_raw_mutate(const DynamicArray<RawCmd> &cmds, Byte *base, Registry *registry) noexcept;

/// @brief Commits MutatePart inverses in reverse order — restores the prev-state part.
void commit_raw_mutate_inverse(const DynamicArray<RawCmd> &cmds, Byte *base,
                               Registry *registry) noexcept;

/// @brief Commits AttachChild commands — attaches each child to its parent.
void commit_raw_attach_child(const DynamicArray<RawCmd> &cmds, World *world) noexcept;

/// @brief Commits AttachChild inverses in reverse order — detaches children.
void commit_raw_attach_child_inverse(const DynamicArray<RawCmd> &cmds, World *world) noexcept;

/// @brief Commits DetachChild commands — detaches each child from its parent.
void commit_raw_detach_child(const DynamicArray<RawCmd> &cmds, World *world) noexcept;

/// @brief Commits DetachChild inverses in reverse order — re-attaches children.
void commit_raw_detach_child_inverse(const DynamicArray<RawCmd> &cmds, World *world) noexcept;

/// @brief Destroys all arena-stored parts and resets the arena. Does NOT clear the thing map.
void reset_raw_cmds(DynamicArray<RawCmd> &cmds, Byte *base, ArenaAlloc &arena) noexcept;

} // namespace fr::impl

// ================================================================ CmdBatch

namespace fr {

/// @brief Self-contained unit of deferred commands.
class CmdBatch {
public:
    // ------------------------------------- Typedefs & Constructors & Operators
    static constexpr USize DEFAULT_ARENA_SIZE = 64 * 1024;

    CmdBatch() noexcept;
    explicit CmdBatch(Alloc *alloc, USize arena_size = DEFAULT_ARENA_SIZE) noexcept;
    ~CmdBatch() noexcept;

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

        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(*current);

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::DestroyPart;
        cmd.destroy_part = impl::RawDestroyPartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .part_offset = offset,
            .commit_destroy = &CmdBatch::do_commit_destroy<T>,
            .commit_insert_move = &CmdBatch::do_commit_insert_move<T>,
            .commit_insert_copy = &CmdBatch::do_commit_insert_copy<T>,
        };

        do_push_cmd(thing, cmd);
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

        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(part);

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = impl::RawInsertPartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .part_offset = offset,
            .commit_insert_move = &CmdBatch::do_commit_insert_move<T>,
            .commit_insert_copy = &CmdBatch::do_commit_insert_copy<T>,
            .commit_destroy = &CmdBatch::do_commit_destroy<T>,
        };

        do_push_cmd(thing, cmd);
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

        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(std::move(part));

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = impl::RawInsertPartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .part_offset = offset,
            .commit_insert_move = &CmdBatch::do_commit_insert_move<T>,
            .commit_insert_copy = &CmdBatch::do_commit_insert_copy<T>,
            .commit_destroy = &CmdBatch::do_commit_destroy<T>,
        };

        do_push_cmd(thing, cmd);
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
        USize prev_offset =
            static_cast<USize>(static_cast<Byte *>(prev_ptr) - m_arena_buffer.data());
        new (prev_ptr) T(prev);

        void *next_ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize next_offset =
            static_cast<USize>(static_cast<Byte *>(next_ptr) - m_arena_buffer.data());
        new (next_ptr) T(next);

        impl::RawCmd cmd{};
        cmd.kind = CmdKind::MutatePart;
        cmd.mutate_part = impl::RawMutatePartCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .prev_offset = prev_offset,
            .next_offset = next_offset,
            .commit_mutate = &CmdBatch::do_commit_mutate<T>,
        };

        do_push_cmd(thing, cmd);
    }

    /// @brief Records an attach-child relation command.
    void record_attach_child(Thing parent, Thing child) noexcept {
        impl::RawCmd cmd{};
        cmd.kind = CmdKind::AttachChild;
        cmd.attach_child = AttachChildCmd{.parent = parent, .child = child};
        do_push_cmd(parent, cmd);
    }

    /// @brief Records a detach-child relation command.
    void record_detach_child(Thing parent, Thing child) noexcept {
        impl::RawCmd cmd{};
        cmd.kind = CmdKind::DetachChild;
        cmd.detach_child = DetachChildCmd{.parent = parent, .child = child};
        do_push_cmd(parent, cmd);
    }

    // ------------------------------------------------------------------ Commit

    /// @brief Commits destroy commands.
    void commit_destroy(impl::Registry *registry) noexcept;

    /// @brief Commits inverses of destroy commands in reverse order (re-inserts parts).
    void commit_destroy_inverse(impl::Registry *registry) noexcept;

    /// @brief Commits insert commands by moving arena data. Do not commit again after this.
    void commit_insert_move(impl::Registry *registry) noexcept;

    /// @brief Commits inverses of insert commands in reverse order (destroys parts).
    void commit_insert_inverse(impl::Registry *registry) noexcept;

    void commit_insert_copy(impl::Registry *registry) noexcept;

    void commit_mutate(impl::Registry *registry) noexcept;

    /// @brief Commits inverses of mutate commands in reverse order (applies prev state).
    void commit_mutate_inverse(impl::Registry *registry) noexcept;

    /// @brief Commits attatch child commands.
    void commit_attach_child(World *world) noexcept;

    /// @brief Commits inverses of attach child commands in reverse order (detaches children).
    void commit_attach_child_inverse(World *world) noexcept;

    /// @brief Commits detach child commands.
    void commit_detach_child(World *world) noexcept;

    /// @brief Commits inverses of detach child commands in reverse order (attaches children).
    void commit_detach_child_inverse(World *world) noexcept;

    /**
     * @brief Destroys all arena-allocated part objects and clears the command list.
     * @note Safe to call with uncommitted commands - parts are properly destroyed.
     */
    void reset() noexcept;

    // ----------------------------------------------------------------- Collect

    /// @brief Returns the base of the arena buffer for pointer reconstruction.
    Byte *arena_data() noexcept {
        return m_arena_buffer.data();
    }

    const Byte *arena_data() const noexcept {
        return m_arena_buffer.data();
    }

    /// @brief Collects all raw insert commands into a new dynamic array.
    DynamicArray<impl::RawInsertPartCmd> collect_insert_part_cmds() const noexcept;

    /// @brief Collects all raw destroy commands into a new dynamic array.
    DynamicArray<impl::RawDestroyPartCmd> collect_destroy_part_cmds() const noexcept;

    /// @brief Collects all raw mutate commands into a new dynamic array.
    DynamicArray<impl::RawMutatePartCmd> collect_mutate_part_cmds() const noexcept;

    /// @brief Collects all attach-child commands into a new dynamic array.
    DynamicArray<AttachChildCmd> collect_attach_child_cmds() const noexcept;

    /// @brief Collects all detach-child commands into a new dynamic array.
    DynamicArray<DetachChildCmd> collect_detach_child_cmds() const noexcept;

private:
    // --------------------------------------------------------------- Internals

    /// @brief Push a command and register it in the per-thing lookup table.
    void do_push_cmd(Thing thing, impl::RawCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        CmdIdx idx = static_cast<CmdIdx>(m_cmds.size());
        m_cmds.push_back(cmd);

        if (!thing.is_nil()) {
            impl::ThingCmdEntry &entry = m_thing_cmds[thing];
            FR_ASSERT(entry.count < MAX_CMDS_PER_THING, "too many commands for one thing");
            entry.indices[entry.count++] = idx;
        }
    }

    template <typename T>
    static void do_commit_insert_move(void *registry_ptr, Thing thing, void *part_ptr) noexcept {
        impl::Registry *registry = static_cast<impl::Registry *>(registry_ptr);
        registry->emplace_checked<T>(thing, std::move(*static_cast<T *>(part_ptr)));
    }

    template <typename T>
    static void do_commit_insert_copy(void *registry_ptr, Thing thing, void *part_ptr) noexcept {
        impl::Registry *registry = static_cast<impl::Registry *>(registry_ptr);
        registry->emplace_checked<T>(thing, *static_cast<T *>(part_ptr));
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

    // ----------------------------------------------------------------- Members
    Alloc *m_alloc{nullptr};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<impl::RawCmd> m_cmds{};
    HashMap<Thing, impl::ThingCmdEntry> m_thing_cmds{};

    friend class CmdSheaf;
};

// ================================================================ CmdSheaf

/// @brief Command container scoped to a single thing.
class CmdSheaf {
public:
    static constexpr USize DEFAULT_ARENA_SIZE = 1024;

    /**
     * @brief Construct an empty `CmdSheaf` for a specific thing.
     * @param alloc Allocator to use.
     * @param thing The thing this sheaf belongs to.
     * @param arena_size Size of the arena in bytes.
     */
    CmdSheaf(Alloc *alloc, Thing thing, USize arena_size = DEFAULT_ARENA_SIZE) noexcept;

    /**
     * @brief Extract and copy the commands for a specific thing from a CmdBatch.
     *
     * @param alloc Allocator to use.
     * @param batch Source batch to extract from.
     * @param thing Thing whose commands are extracted.
     *
     * @note Computes a fitted arena from the batch's type metadata and copies each part.
     * The resulting sheaf is fully self-contained; the batch may be reset or destroyed freely.
     */
    CmdSheaf(Alloc *alloc, const CmdBatch &batch, Thing thing) noexcept;

    ~CmdSheaf() noexcept;

    CmdSheaf(const CmdSheaf &) = delete;
    CmdSheaf(CmdSheaf &&) = delete;
    CmdSheaf &operator=(const CmdSheaf &) = delete;
    CmdSheaf &operator=(CmdSheaf &&) = delete;

    // ------------------------------------------------------------------ Access

    /// @brief Returns the thing this sheaf belongs to.
    Thing thing() const noexcept {
        return m_thing;
    }

    /// @brief Returns the base of the arena buffer.
    Byte *arena() noexcept {
        return m_arena_buffer.data();
    }

    /// @brief Returns the base of the arena buffer.
    const Byte *arena() const noexcept {
        return m_arena_buffer.data();
    }

    // ------------------------------------------------------------------ Commit

    void commit_destroy(impl::Registry *registry) noexcept;
    void commit_destroy_inverse(impl::Registry *registry) noexcept;

    void commit_insert_move(impl::Registry *registry) noexcept;
    void commit_insert_inverse(impl::Registry *registry) noexcept;
    void commit_insert_copy(impl::Registry *registry) noexcept;

    void commit_mutate(impl::Registry *registry) noexcept;
    void commit_mutate_inverse(impl::Registry *registry) noexcept;

    void commit_attach_child(World *world) noexcept;
    void commit_attach_child_inverse(World *world) noexcept;

    void commit_detach_child(World *world) noexcept;
    void commit_detach_child_inverse(World *world) noexcept;

    /// @brief Destroys all arena-allocated part objects and clears the command list.
    void reset() noexcept;

private:
    Alloc *m_alloc{nullptr};
    Thing m_thing{};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<impl::RawCmd> m_cmds{};
};
} // namespace fr
