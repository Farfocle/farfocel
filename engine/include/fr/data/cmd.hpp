/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Command types and deferred command buffers for the data layer.
 *
 * CmdBatch and CmdSheaf are pure data holders. They record typed operations into an
 * arena-backed list. World (or any caller) drives the actual commit by iterating over
 * the command list and calling Registry's type-erased insert_raw / destroy_raw / get_raw
 * methods backed by PartMeta function pointers.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// =================================================================== Constants

static constexpr USize MAX_CMDS = 1 << 16;
static constexpr USize MAX_CMDS_PER_THING = 16;
static constexpr USize DEFAULT_CMD_BATCH_ARENA_SIZE = 1 << 16;

/// @brief Index into the CmdBatch command list.
using CmdIdx = U16;

// ===================================================================== CmdKind

/// @brief Discriminant for the Cmd union.
enum class CmdKind : U8 {
    Nil,
    DestroyPart,
    InsertPart,
    MutatePart,
    AttachChild,
    DetachChild,
    Handout,
    Kill,
};

/// @brief Returns the nil command kind (ADL nil protocol).
inline CmdKind nil(CmdKind *) noexcept {
    return CmdKind::Nil;
}

/// @brief Returns true if the command kind is nil (ADL nil protocol).
inline bool is_nil(CmdKind v) noexcept {
    return v == CmdKind::Nil;
}

// ==================================================================== Commands

struct InsertCmd;
struct DestroyCmd;
struct MutateCmd;
struct AttachChildCmd;
struct DetachChildCmd;
struct HandoutCmd;
struct KillCmd;

/// @brief Payload for a deferred part insert.
struct InsertCmd {
    TypeIdx tidx;
    Thing thing;
    USize offset;

    /// Returns the equivalent destroy command (for undo / inverse commit).
    DestroyCmd invert() const noexcept;

    /// @brief Cast the part pointer from the arena. Returns nullptr on type mismatch.
    template <typename T>
    T *try_cast(Byte *arena_base) noexcept {
        if (TypeIdx::from_type<T>() != tidx) {
            return nullptr;
        }

        return static_cast<T *>(static_cast<void *>(arena_base + offset));
    }
};

/// @brief Payload for a deferred part destroy (snapshots the current part value).
struct DestroyCmd {
    TypeIdx tidx;
    Thing thing;
    USize offset;

    /// Returns the equivalent insert command (for undo / inverse commit).
    InsertCmd invert() const noexcept;

    /// @brief Cast the part pointer from the arena. Returns nullptr on type mismatch.
    template <typename T>
    T *try_cast(Byte *arena_base) noexcept {
        if (TypeIdx::from_type<T>() != tidx) {
            return nullptr;
        }

        return static_cast<T *>(static_cast<void *>(arena_base + offset));
    }
};

/// @brief Payload for a deferred part mutate (captures prev and next values).
struct MutateCmd {
    TypeIdx tidx;
    Thing thing;
    USize prev_offset;
    USize next_offset;

    /// Returns a mutate command with prev/next swapped (for undo / inverse commit).
    MutateCmd invert() const noexcept;

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

/// @brief Payload for a deferred attach-child relation command.
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

/// @brief Payload for a deferred detach-child relation command.
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

/**
 * @brief Records a thing that was handed out (created).
 *
 * @note `handout_deferred()` creates the thing immediately so the handle is usable right away,
 * then records this command in the batch for tracking and undo purposes.
 * The commit step (`commit_handout`) is a no-op since the thing is already alive.
 * @note The inverse is a `KillCmd` that destroys the thing.
 */
struct HandoutCmd {
    Thing thing;

    KillCmd inverse() const noexcept;

    static HandoutCmd nil() noexcept {
        return {Thing::nil()};
    }

    bool is_nil() const noexcept {
        return thing.is_nil();
    }
};

/**
 * @brief Defers the kill of a thing until commit time.
 *
 * @note `kill_deferred()` records this command in the batch. At commit, `kill()` is called on the
 * thing, destroying all its parts and recycling its slot. This avoids killing things while
 * iterating over part pools.
 * @note The inverse is a `HandoutCmd` (re-creation of the thing handle).
 */
struct KillCmd {
    Thing thing;

    HandoutCmd inverse() const noexcept;

    static KillCmd nil() noexcept {
        return {Thing::nil()};
    }

    bool is_nil() const noexcept {
        return thing.is_nil();
    }
};

// ========================================================================= Cmd

/// @brief Discriminated union of all command types.
struct Cmd {
    CmdKind kind{CmdKind::Nil};
    union {
        InsertCmd insert_part;
        DestroyCmd destroy_part;
        MutateCmd mutate_part;
        AttachChildCmd attach_child;
        DetachChildCmd detach_child;
        HandoutCmd handout;
        KillCmd kill;
    };

    static Cmd nil() noexcept {
        Cmd cmd{};
        cmd.kind = CmdKind::Nil;
        return cmd;
    }

    bool is_nil() const noexcept {
        return kind == CmdKind::Nil;
    }

    /// @brief Returns the primary thing associated with this command.
    Thing thing() const noexcept {
        switch (kind) {
        case CmdKind::InsertPart:
            return insert_part.thing;
        case CmdKind::DestroyPart:
            return destroy_part.thing;
        case CmdKind::MutatePart:
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
};

// Forward declarations needed by CmdSheaf constructor.
class World;
class CmdBatch;

// ==================================================================== CmdBatch

/**
 * @brief Self-contained, arena-backed deferred command buffer.
 *
 * CmdBatch is a pure data holder. It records typed part and relation commands into an
 * internal arena without needing a registry reference. The caller (typically World) is
 * responsible for iterating `cmds()` and applying each command via Registry's type-erased
 * operations (`insert_raw`, `destroy_raw`, `get_raw`).
 *
 * Part values are placement-new'd into the arena at record time and destroyed on `reset()`.
 */
class CmdBatch {
public:
    // ----------------------------------------------- Constructors & Destructor

    /// @brief Construct using the ambient allocator with the default arena size.
    CmdBatch() noexcept;

    /// @brief Construct with an explicit allocator and arena size.
    explicit CmdBatch(Alloc *alloc, USize arena_size = DEFAULT_CMD_BATCH_ARENA_SIZE) noexcept;

    ~CmdBatch() noexcept;

    /**
     * @brief Copy constructor.
     * Computes a fitted arena by scanning `other`'s commands, allocates it, then
     * copy-constructs every part snapshot into the new arena with patched offsets.
     * Non-part commands (attach/detach/handout/kill) are copied as they are.
     */
    CmdBatch(const CmdBatch &other) noexcept;

    /**
     * @brief Move constructor.
     * Steals the allocator, the arena buffer, and the command list from `other`.
     * The ArenaAlloc is reconstructed over the stolen buffer with the bump pointer
     * advanced to match the source's used bytes, so future record calls are safe.
     * Source is left in a valid, empty state (no buffer, no commands).
     */
    CmdBatch(CmdBatch &&other) noexcept;

    CmdBatch &operator=(const CmdBatch &) = delete;
    CmdBatch &operator=(CmdBatch &&) = delete;

    // ------------------------------------------------------------------ Record

    /**
     * @brief Records a deferred insert for part `T` (copy).
     * @note Does NOT check for duplicate parts - the commit step handles that.
     */
    template <typename T>
    void record_insert(Thing thing, const T &part) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(part);

        Cmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = InsertCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .offset = offset,
        };

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records a deferred insert for part `T` (move).
     * @note Does NOT check for duplicate parts - the commit step handles that.
     */
    template <typename T>
    void record_insert(Thing thing, T &&part) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(std::move(part));

        Cmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = InsertCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .offset = offset,
        };

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records a deferred destroy for part `T`, snapshotting `current` into the arena.
     * @note The snapshot enables inverse commits (undo).
     */
    template <typename T>
    void record_destroy(Thing thing, const T &current) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(current);

        Cmd cmd{};
        cmd.kind = CmdKind::DestroyPart;
        cmd.destroy_part = DestroyCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .offset = offset,
        };

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records a deferred mutate for part `T`, capturing both `prev` and `next` values.
     * @note Both values are copy-constructed into the arena. `prev` enables undo.
     */
    template <typename T>
    void record_mutate(Thing thing, const T &prev, const T &next) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        void *prev_ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize prev_offset =
            static_cast<USize>(static_cast<Byte *>(prev_ptr) - m_arena_buffer.data());
        new (prev_ptr) T(prev);

        void *next_ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize next_offset =
            static_cast<USize>(static_cast<Byte *>(next_ptr) - m_arena_buffer.data());
        new (next_ptr) T(next);

        Cmd cmd{};
        cmd.kind = CmdKind::MutatePart;
        cmd.mutate_part = MutateCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .prev_offset = prev_offset,
            .next_offset = next_offset,
        };

        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred attach-child relation command.
    void record_attach_child(Thing parent, Thing child) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        Cmd cmd{};
        cmd.kind = CmdKind::AttachChild;
        cmd.attach_child = AttachChildCmd{parent, child};

        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred detach-child relation command.
    void record_detach_child(Thing parent, Thing child) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        Cmd cmd{};
        cmd.kind = CmdKind::DetachChild;
        cmd.detach_child = DetachChildCmd{parent, child};

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records that `thing` was handed out (for undo tracking).
     * @note The thing is already alive at this point; commit_handout() is a no-op.
     */
    void record_handout(Thing thing) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        Cmd cmd{};
        cmd.kind = CmdKind::Handout;
        cmd.handout = HandoutCmd{thing};

        m_cmds.push_back(cmd);
    }

    /**
     * @brief Records a deferred kill - `thing` is killed at commit time.
     * @note Kills are applied after all part mutations and destructions in `commit()`.
     */
    void record_kill(Thing thing) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");

        Cmd cmd{};
        cmd.kind = CmdKind::Kill;
        cmd.kill = KillCmd{thing};

        m_cmds.push_back(cmd);
    }

    // --------------------------------------------------------------- Access

    /// @brief Returns a read-only view of all recorded commands.
    Slice<const Cmd> cmds() const noexcept {
        return m_cmds.slice();
    }

    /// @brief Returns the base of the arena buffer for pointer reconstruction.
    Byte *arena() noexcept {
        return m_arena_buffer.data();
    }

    const Byte *arena() const noexcept {
        return m_arena_buffer.data();
    }

    // ------------------------------------------------------------------- Reset

    /**
     * @brief Destroys all arena-stored part objects and clears the command list.
     * @note Safe to call with uncommitted commands — all snapshots are properly destroyed.
     */
    void reset() noexcept;

private:
    Alloc *m_alloc{nullptr};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<Cmd> m_cmds{};
};

// ================================================================ CmdSheaf

/**
 * @brief Command buffer scoped to a single thing.
 *
 * @note Like `CmdBatch` but constrained to one thing. Provides a convenient record API where
 * the thing is implicit. Can be extracted from a CmdBatch via the
 * `CmdSheaf(Alloc*, const CmdBatch&, Thing)` constructor, which scans the batch for
 * commands belonging to the given thing and copies all part snapshots into a fitted arena.
 */
class CmdSheaf {
public:
    static constexpr USize DEFAULT_ARENA_SIZE = 1024;

    // ----------------------------------------------- Constructors & Destructor

    /// @brief Construct an empty sheaf for a specific thing.
    CmdSheaf(Alloc *alloc, Thing thing, USize arena_size = DEFAULT_ARENA_SIZE) noexcept;

    /**
     * @brief Extract and copy all commands for `thing` from a CmdBatch.
     *
     * Computes a fitted arena from type metadata (via `TypeIdx::meta()`), then
     * copy-constructs each part snapshot into the local arena. The resulting sheaf is
     * fully self-contained; the source batch may be reset or destroyed freely afterwards.
     */
    CmdSheaf(Alloc *alloc, const CmdBatch &batch, Thing thing) noexcept;

    ~CmdSheaf() noexcept;

    /**
     * @brief Copy constructor.
     * Computes a fitted arena by scanning `other`'s commands, allocates it, then
     * copy-constructs every part snapshot into the new arena with patched offsets.
     * Non-part commands (attach/detach/handout/kill) are copied as they are.
     */
    CmdSheaf(const CmdSheaf &other) noexcept;

    /**
     * @brief Move constructor.
     * @note Steals `m_alloc`, `m_thing`, the arena buffer, and the command list from `other`.
     * Source is left in a valid, empty state (no buffer, no commands).
     */
    CmdSheaf(CmdSheaf &&other) noexcept;

    CmdSheaf &operator=(const CmdSheaf &) = delete;
    CmdSheaf &operator=(CmdSheaf &&) = delete;

    // ------------------------------------------------------------------ Record

    /// @brief Records a deferred insert for part `T` (copy). Thing is implicit (`this->thing()`).
    template <typename T>
    void record_insert(const T &part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(part);

        Cmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = InsertCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = m_thing,
            .offset = offset,
        };

        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred insert for part `T` (move). Thing is implicit.
    template <typename T>
    void record_insert(T &&part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(std::move(part));

        Cmd cmd{};
        cmd.kind = CmdKind::InsertPart;
        cmd.insert_part = InsertCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = m_thing,
            .offset = offset,
        };

        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred destroy, snapshotting `current`. Thing is implicit.
    template <typename T>
    void record_destroy(const T &current) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize offset = static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
        new (ptr) T(current);

        Cmd cmd{};
        cmd.kind = CmdKind::DestroyPart;
        cmd.destroy_part = DestroyCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = m_thing,
            .offset = offset,
        };

        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred mutate with both old and new values. Thing is implicit.
    template <typename T>
    void record_mutate(const T &prev, const T &next) noexcept {
        void *prev_ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize prev_offset =
            static_cast<USize>(static_cast<Byte *>(prev_ptr) - m_arena_buffer.data());
        new (prev_ptr) T(prev);

        void *next_ptr = m_arena.allocate(sizeof(T), alignof(T));
        USize next_offset =
            static_cast<USize>(static_cast<Byte *>(next_ptr) - m_arena_buffer.data());
        new (next_ptr) T(next);

        Cmd cmd{};
        cmd.kind = CmdKind::MutatePart;
        cmd.mutate_part = MutateCmd{
            .tidx = TypeIdx::from_type<T>(),
            .thing = m_thing,
            .prev_offset = prev_offset,
            .next_offset = next_offset,
        };

        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred attach-child relation (explicit parent and child).
    void record_attach_child(Thing parent, Thing child) noexcept {
        Cmd cmd{};
        cmd.kind = CmdKind::AttachChild;
        cmd.attach_child = AttachChildCmd{parent, child};
        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred detach-child relation (explicit parent and child).
    void record_detach_child(Thing parent, Thing child) noexcept {
        Cmd cmd{};
        cmd.kind = CmdKind::DetachChild;
        cmd.detach_child = DetachChildCmd{parent, child};
        m_cmds.push_back(cmd);
    }

    /// @brief Records a deferred kill for `m_thing` (applied at commit time).
    void record_kill() noexcept {
        Cmd cmd{};
        cmd.kind = CmdKind::Kill;
        cmd.kill = KillCmd{m_thing};
        m_cmds.push_back(cmd);
    }

    // ------------------------------------------------------------------ Access

    /// @brief Returns the thing this sheaf belongs to.
    Thing thing() const noexcept {
        return m_thing;
    }

    /// @brief Returns a read-only view of all recorded commands.
    Slice<const Cmd> cmds() const noexcept {
        return m_cmds.slice();
    }

    /// @brief Returns the base of the arena buffer for pointer reconstruction.
    Byte *arena() noexcept {
        return m_arena_buffer.data();
    }

    const Byte *arena() const noexcept {
        return m_arena_buffer.data();
    }

    // ------------------------------------------------------------------- Reset

    /**
     * @brief Destroys all arena-stored part objects and clears the command list.
     * @note Safe to call with uncommitted commands.
     */
    void reset() noexcept;

private:
    // ----------------------------------------------------------------- Members
    Alloc *m_alloc{nullptr};
    Thing m_thing{};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<Cmd> m_cmds{};
};

// ============================================================ CmdBatchTimeline

// ============================================================ CmdSheafTimeline

} // namespace fr
