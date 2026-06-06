/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Command types and deferred command buffers for the data layer.
 * @details `CmdBatch` and `CmdSheaf` are pure data holders. They record typed operations into an
 * arena-backed list. World (or any caller) drives the actual commit by iterating over
 * the command list.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/arena_alloc.hpp"
#include "fr/core/array.hpp"
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
    Destroy,
    Insert,
    Mutate,
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
    DestroyCmd inverse() const noexcept;

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
    InsertCmd inverse() const noexcept;

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
    MutateCmd inverse() const noexcept;

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

    Cmd inverse() const noexcept {
        switch (kind) {
        case CmdKind::Insert:
            return {
                .kind = CmdKind::Destroy,
                .destroy_part = insert_part.inverse(),
            };
        case CmdKind::Destroy:
            return {
                .kind = CmdKind::Insert,
                .insert_part = destroy_part.inverse(),
            };
        case CmdKind::Mutate:
            return {
                .kind = CmdKind::Mutate,
                .mutate_part = mutate_part.inverse(),
            };
        case CmdKind::AttachChild:
            return {
                .kind = CmdKind::DetachChild,
                .detach_child = attach_child.inverse(),
            };
        case CmdKind::DetachChild:
            return {
                .kind = CmdKind::AttachChild,
                .attach_child = detach_child.inverse(),
            };
        case CmdKind::Handout:
            return {
                .kind = CmdKind::Kill,
                .kill = handout.inverse(),
            };
        case CmdKind::Kill:
            return {
                .kind = CmdKind::Handout,
                .handout = kill.inverse(),
            };
        default:
            return nil();
        }
    };
};

// Forward declarations needed by CmdSheaf constructor.
class World;
class CmdBatch;

// ==================================================================== CmdBatch

/**
 * @brief Self-contained, arena-backed deferred command buffer.
 *
 * @note CmdBatch is a pure data holder. It records typed part and relation commands into an
 * internal arena without needing a registry reference. The caller (typically World) is
 * responsible for iterating `cmds()` and applying each command via Registry's type-erased
 * operations (`insert_raw`, `destroy_raw`, `get_raw`).
 */
class CmdBatch {
public:
    // ----------------------------------------------- Constructors & Destructor

    /// @brief Construct using the ambient allocator with the default arena size.
    CmdBatch() noexcept;

    /// @brief Construct with an explicit allocator and arena size.
    explicit CmdBatch(Alloc *alloc, USize arena_size = DEFAULT_CMD_BATCH_ARENA_SIZE) noexcept;

    ~CmdBatch() noexcept;

    CmdBatch(const CmdBatch &other) noexcept;
    CmdBatch(CmdBatch &&other) noexcept;

    CmdBatch &operator=(const CmdBatch &) = delete;
    CmdBatch &operator=(CmdBatch &&) = delete;

    // -------------------------------------------------------------- Record Raw

    /// @brief Pushes a pre-built InsertCmd. The offset must already point into this batch's arena.
    void record_insert_raw(InsertCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        Cmd c{};
        c.kind = CmdKind::Insert;
        c.insert_part = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built DestroyCmd. The offset must already point into this batch's arena.
    void record_destroy_raw(DestroyCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        Cmd c{};
        c.kind = CmdKind::Destroy;
        c.destroy_part = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built MutateCmd. Both offsets must already point into this batch's
    /// arena.
    void record_mutate_raw(MutateCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        Cmd c{};
        c.kind = CmdKind::Mutate;
        c.mutate_part = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built AttachChildCmd.
    void record_attach_child_raw(AttachChildCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        Cmd c{};
        c.kind = CmdKind::AttachChild;
        c.attach_child = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built DetachChildCmd.
    void record_detach_child_raw(DetachChildCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        Cmd c{};
        c.kind = CmdKind::DetachChild;
        c.detach_child = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built HandoutCmd.
    void record_handout_raw(HandoutCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        Cmd c{};
        c.kind = CmdKind::Handout;
        c.handout = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built KillCmd.
    void record_kill_raw(KillCmd cmd) noexcept {
        FR_ASSERT(m_cmds.size() < MAX_CMDS, "CmdBatch is full");
        Cmd c{};
        c.kind = CmdKind::Kill;
        c.kill = cmd;
        m_cmds.push_back(c);
    }

    // ------------------------------------------------------------------ Record

    /**
     * @brief Records a deferred insert for part `T` (copy).
     * @note Does NOT check for duplicate parts - the commit step handles that.
     */
    template <typename T>
    void record_insert(Thing thing, const T &part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(part);

        record_insert_raw({
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .offset = do_get_offset(ptr),
        });
    }

    /**
     * @brief Records a deferred insert for part `T` (move).
     * @note Does NOT check for duplicate parts - the commit step handles that.
     */
    template <typename T>
    void record_insert(Thing thing, T &&part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(std::move(part));

        record_insert_raw({
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .offset = do_get_offset(ptr),
        });
    }

    /**
     * @brief Records a deferred destroy for part `T`, snapshotting `current` into the arena.
     * @note The snapshot enables inverse commits (undo).
     */
    template <typename T>
    void record_destroy(Thing thing, const T &current) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(current);
        record_destroy_raw({
            .tidx = TypeIdx::from_type<T>(),
            .thing = thing,
            .offset = do_get_offset(ptr),
        });
    }

    /**
     * @brief Records a deferred mutate for part `T`, capturing both `prev` and `next` values.
     * @note Both values are copy-constructed into the arena. `prev` enables undo.
     */
    template <typename T>
    void record_mutate(Thing thing, const T &prev, const T &next) noexcept {
        void *prev_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (prev_ptr) T(prev);

        void *next_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (next_ptr) T(next);

        record_mutate_raw({.tidx = TypeIdx::from_type<T>(),
                           .thing = thing,
                           .prev_offset = do_get_offset(prev_ptr),
                           .next_offset = do_get_offset(next_ptr)});
    }

    /// @brief Records a deferred attach-child relation command.
    void record_attach_child(Thing parent, Thing child) noexcept {
        record_attach_child_raw({.parent = parent, .child = child});
    }

    /// @brief Records a deferred detach-child relation command.
    void record_detach_child(Thing parent, Thing child) noexcept {
        record_detach_child_raw({.parent = parent, .child = child});
    }

    /**
     * @brief Records that `thing` was handed out (for undo tracking).
     * @note The thing is already alive at this point; commit_handout() is a no-op.
     */
    void record_handout(Thing thing) noexcept {
        record_handout_raw({.thing = thing});
    }

    /**
     * @brief Records a deferred kill — `thing` is killed at commit time.
     * @note Kills are applied after all part mutations and destructions in `commit()`.
     */
    void record_kill(Thing thing) noexcept {
        record_kill_raw({.thing = thing});
    }

    // ------------------------------------------------------------------ Access

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

    /**
     * @brief Resets the command list and grows the arena buffer to `size` bytes if necessary.
     * @note Safe to call with uncommitted commands.
     */
    void reset_reserve(USize size) noexcept;

private:
    /// @brief Returns the byte offset of `ptr` from the arena base.
    USize do_get_offset(void *ptr) const noexcept {
        return static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
    }

    /**
     * @brief Allocates the arena buffer and initialises `m_arena_buffer`, `m_arena`, `m_cmds`.
     * @pre `m_alloc` must already be set.
     */
    void do_init_storage(USize size, const char *tag) noexcept;

    /**
     * @brief Copy-constructs part snapshots from `src_cmds` into this batch's arena.
     * @pre `do_init_storage` must have been called first.
     */
    void do_copy_cmds(Slice<const Cmd> src_cmds, const Byte *src_base,
                      Thing filter = Thing::nil()) noexcept;

    Alloc *m_alloc{nullptr};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<Cmd> m_cmds{};
};

// ==================================================================== CmdSheaf

/**
 * @brief Command buffer scoped to a single thing.
 *
 * @note Like `CmdBatch` but constrained to one thing. Provides a convenient record API where
 * the thing is implicit. Can be extracted from a `CmdBatch` via the
 * `CmdSheaf(Alloc*, const CmdBatch&, Thing)` constructor, which scans the batch for
 * commands belonging to the given thing and copies all part snapshots into a fitted arena.
 */
class CmdSheaf {
public:
    static constexpr USize DEFAULT_ARENA_SIZE = 1024;

    // ----------------------------------------------- Constructors & Destructor

    /// @brief Construct an empty sheaf for a specific thing.
    CmdSheaf(Alloc *alloc, Thing thing, USize arena_size = DEFAULT_ARENA_SIZE) noexcept;

    /// @brief Extract and copy all commands for `thing` from a CmdBatch.
    CmdSheaf(Alloc *alloc, const CmdBatch &batch, Thing thing) noexcept;

    ~CmdSheaf() noexcept;

    CmdSheaf(const CmdSheaf &other) noexcept;
    CmdSheaf(CmdSheaf &&other) noexcept;

    CmdSheaf &operator=(const CmdSheaf &) = delete;
    CmdSheaf &operator=(CmdSheaf &&) = delete;

    // -------------------------------------------------------------- Record Raw

    /// @brief Pushes a pre-built InsertCmd. The offset must already point into this sheaf's arena.
    void record_insert_raw(InsertCmd cmd) noexcept {
        Cmd c{};
        c.kind = CmdKind::Insert;
        c.insert_part = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built DestroyCmd. The offset must already point into this sheaf's arena.
    void record_destroy_raw(DestroyCmd cmd) noexcept {
        Cmd c{};
        c.kind = CmdKind::Destroy;
        c.destroy_part = cmd;
        m_cmds.push_back(c);
    }

    /**
     * @brief Pushes a pre-built `MutateCmd`. Both offsets must already point into this sheaf's
     * arena.
     */
    void record_mutate_raw(MutateCmd cmd) noexcept {
        Cmd c{};
        c.kind = CmdKind::Mutate;
        c.mutate_part = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built `AttachChildCmd`.
    void record_attach_child_raw(AttachChildCmd cmd) noexcept {
        Cmd c{};
        c.kind = CmdKind::AttachChild;
        c.attach_child = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built `DetachChildCmd`.
    void record_detach_child_raw(DetachChildCmd cmd) noexcept {
        Cmd c{};
        c.kind = CmdKind::DetachChild;
        c.detach_child = cmd;
        m_cmds.push_back(c);
    }

    /// @brief Pushes a pre-built `KillCmd`.
    void record_kill_raw(KillCmd cmd) noexcept {
        Cmd c{};
        c.kind = CmdKind::Kill;
        c.kill = cmd;
        m_cmds.push_back(c);
    }

    // ------------------------------------------------------------------ Record

    /// @brief Records a deferred insert for part `T` (copy). Thing is implicit.
    template <typename T>
    void record_insert(const T &part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(part);

        record_insert_raw({
            .tidx = TypeIdx::from_type<T>(),
            .thing = m_thing,
            .offset = do_get_offset(ptr),
        });
    }

    /// @brief Records a deferred insert for part `T` (move). Thing is implicit.
    template <typename T>
    void record_insert(T &&part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(std::move(part));

        record_insert_raw({
            .tidx = TypeIdx::from_type<T>(),
            .thing = m_thing,
            .offset = do_get_offset(ptr),
        });
    }

    /// @brief Records a deferred destroy, snapshotting `current`. Thing is implicit.
    template <typename T>
    void record_destroy(const T &current) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(current);

        record_destroy_raw({
            .tidx = TypeIdx::from_type<T>(),
            .thing = m_thing,
            .offset = do_get_offset(ptr),
        });
    }

    /// @brief Records a deferred mutate with both old and new values. Thing is implicit.
    template <typename T>
    void record_mutate(const T &prev, const T &next) noexcept {
        void *prev_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (prev_ptr) T(prev);

        void *next_ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (next_ptr) T(next);

        record_mutate_raw({.tidx = TypeIdx::from_type<T>(),
                           .thing = m_thing,
                           .prev_offset = do_get_offset(prev_ptr),
                           .next_offset = do_get_offset(next_ptr)});
    }

    /// @brief Records a deferred attach-child relation (explicit parent and child).
    void record_attach_child(Thing parent, Thing child) noexcept {
        record_attach_child_raw({
            .parent = parent,
            .child = child,
        });
    }

    /// @brief Records a deferred detach-child relation (explicit parent and child).
    void record_detach_child(Thing parent, Thing child) noexcept {
        record_detach_child_raw({
            .parent = parent,
            .child = child,
        });
    }

    /// @brief Records a deferred kill for `m_thing`.
    void record_kill() noexcept {
        record_kill_raw({.thing = m_thing});
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

    /**
     * @brief Resets the command list and grows the arena buffer to `size` bytes if necessary.
     * @note Safe to call with uncommitted commands.
     */
    void reset_reserve(USize size) noexcept;

    // --------------------------------------------------------------- Factory

    /// @brief Merge `a` (older) and `b` (newer) into one sheaf with MutatePart coalescing.
    static CmdSheaf merge_compressed(Alloc *alloc, const CmdSheaf &a, const CmdSheaf &b) noexcept;

private:
    // ----------------------------------------------------------------- Helpers

    /// @brief Returns the byte offset of `ptr` from the arena base.
    USize do_get_offset(void *ptr) const noexcept {
        return static_cast<USize>(static_cast<Byte *>(ptr) - m_arena_buffer.data());
    }

    /// @brief Allocates the arena buffer and initialises `m_arena_buffer`, `m_arena`, `m_cmds`.
    void do_init_storage(USize size, const char *tag) noexcept;

    /// @brief Copy-constructs part snapshots from `src_cmds` into this sheaf's arena,
    void do_copy_cmds(Slice<const Cmd> src_cmds, const Byte *src_base,
                      Thing filter = Thing::nil()) noexcept;

    // ----------------------------------------------------------------- Members
    Alloc *m_alloc{nullptr};
    Thing m_thing{};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<Cmd> m_cmds{};
};

// ============================================================ CmdBatchTimeline

class CmdBatchTimeline {
    // ----------------------------------------------------------------- Members

    static constexpr USize BATCH_RING_SIZE = 1 << 8;
    Alloc *m_alloc{get_ambient_ctx().alloc};
    Array<CmdBatch, BATCH_RING_SIZE> m_batches{};
    Array<USize, BATCH_RING_SIZE> m_calendar{};
    USize m_cursor{0};
    USize m_time{0};

public:
    // ----------------------------------------------- Constructors & Destructor

    CmdBatchTimeline() noexcept = default;
    explicit CmdBatchTimeline(Alloc *alloc) noexcept
        : m_alloc(alloc) {
        (void)m_alloc;
        m_calendar = Array<USize, BATCH_RING_SIZE>::from_repeated(0);
    }

    CmdBatchTimeline(const CmdBatchTimeline &) = delete;
    CmdBatchTimeline(CmdBatchTimeline &&) = delete;
    CmdBatchTimeline &operator=(const CmdBatchTimeline &) = delete;
    CmdBatchTimeline &operator=(CmdBatchTimeline &&) = delete;

    ~CmdBatchTimeline() noexcept = default;

    // --------------------------------------------------------------------- API

    /// @brief Returns the current cursor position.
    USize cursor() const noexcept {
        return m_cursor;
    }

    /// @brief Returns the current time.
    USize time() const noexcept {
        return m_time;
    }

    /// @brief Returns the current batch.
    CmdBatch &batch() noexcept {
        return m_batches[m_cursor];
    }

    /// @brief Push a batch onto the timeline (copy).
    void push(const CmdBatch &batch, USize dt) noexcept {
        m_cursor = do_next_cursor();
        m_batches[m_cursor].~CmdBatch();
        new (&m_batches[m_cursor]) CmdBatch(batch);

        m_time += dt;
        m_calendar[m_cursor] = m_time;
    }

    /// @brief Push a batch onto the timeline (move).
    void push(CmdBatch &&batch, USize dt) noexcept {
        m_cursor = do_next_cursor();
        m_batches[m_cursor].~CmdBatch();
        new (&m_batches[m_cursor]) CmdBatch(std::move(batch));

        m_time += dt;
        m_calendar[m_cursor] = m_time;
    }

    /// @brief Step cursor toward the present. Returns false if already there.
    bool go_future() noexcept {
        if (m_calendar[m_cursor] >= m_calendar[do_next_cursor()]) {
            return false;
        }

        m_cursor = do_next_cursor();
        return true;
    }

    /// @brief Step cursor into the past. Returns false if already at the oldest entry.
    bool go_past() noexcept {
        if (m_calendar[m_cursor] <= m_calendar[do_prev_cursor()]) {
            return false;
        }

        m_cursor = do_prev_cursor();
        return true;
    }

    /// @brief Go to the present, i.e. the most recent batch.
    void go_present() noexcept {
        while (go_future()) {
        }
    }

private:
    // --------------------------------------------------------------- Internals
    USize do_next_cursor() const noexcept {
        return do_next_cursor(1);
    }

    USize do_next_cursor(USize offset) const noexcept {
        return (m_cursor + offset) % BATCH_RING_SIZE;
    }

    USize do_prev_cursor() const noexcept {
        return do_prev_cursor(1);
    }

    USize do_prev_cursor(USize offset) const noexcept {
        return (m_cursor + BATCH_RING_SIZE - offset) % BATCH_RING_SIZE;
    }

    USize do_prev_time() const noexcept {
        return m_calendar[do_prev_cursor(1)];
    }
};

// ============================================================ CmdSheafTimeline

/**
 * @brief Ring-buffered timeline of CmdSheafs for a single thing.
 * @note Supports navigation and merge-compression of adjacent sheaves with `MutatePart` coalescing.
 */
class CmdSheafTimeline {
    // ----------------------------------------------------------------- Members

    static constexpr USize SHEAF_RING_SIZE = 1 << 5; // 32

    Alloc *m_alloc{get_ambient_ctx().alloc};
    Thing m_thing{Thing::nil()};
    alignas(CmdSheaf) Byte m_storage[SHEAF_RING_SIZE * sizeof(CmdSheaf)]{};
    Array<USize, SHEAF_RING_SIZE> m_calendar{};
    USize m_head{0};
    USize m_cursor{0};
    USize m_time{0};
    USize m_count{0};

public:
    // ----------------------------------------------- Constructors & Destructor

    CmdSheafTimeline() noexcept = default;

    explicit CmdSheafTimeline(Alloc *alloc, Thing thing) noexcept
        : m_alloc(alloc),
          m_thing(thing) {
        m_calendar = Array<USize, SHEAF_RING_SIZE>::from_repeated(0);
    }

    CmdSheafTimeline(const CmdSheafTimeline &) = delete;
    CmdSheafTimeline(CmdSheafTimeline &&) = delete;
    CmdSheafTimeline &operator=(const CmdSheafTimeline &) = delete;
    CmdSheafTimeline &operator=(CmdSheafTimeline &&) = delete;

    ~CmdSheafTimeline() noexcept {
        for (USize i = 0; i < m_count; ++i) {
            USize idx = (m_head + SHEAF_RING_SIZE - i) % SHEAF_RING_SIZE;
            reinterpret_cast<CmdSheaf *>(m_storage + idx * sizeof(CmdSheaf))->~CmdSheaf();
        }
    }

    // --------------------------------------------------------------------- API

    /// @brief Returns the current logical time.
    USize time() const noexcept {
        return m_time;
    }
    /// @brief Returns the number of live sheaves.
    USize count() const noexcept {
        return m_count;
    }
    /// @brief Returns the thing all sheaves belong to.
    Thing thing() const noexcept {
        return m_thing;
    }

    /// @brief Returns the sheaf at the current cursor. @pre count() > 0.
    CmdSheaf &sheaf() noexcept {
        return *reinterpret_cast<CmdSheaf *>(m_storage + m_cursor * sizeof(CmdSheaf));
    }

    const CmdSheaf &sheaf() const noexcept {
        return *reinterpret_cast<const CmdSheaf *>(m_storage + m_cursor * sizeof(CmdSheaf));
    }

    /// @brief Push a sheaf onto the timeline (copy).
    void push(const CmdSheaf &s, USize dt) noexcept {
        do_push_advance();
        new (m_storage + m_head * sizeof(CmdSheaf)) CmdSheaf(s);
        m_cursor = m_head;
        m_time += dt;
        m_calendar[m_head] = m_time;
    }

    /// @brief Push a sheaf onto the timeline (move).
    void push(CmdSheaf &&s, USize dt) noexcept {
        do_push_advance();
        new (m_storage + m_head * sizeof(CmdSheaf)) CmdSheaf(std::move(s));
        m_cursor = m_head;
        m_time += dt;
        m_calendar[m_head] = m_time;
    }

    /// @brief Step cursor toward the present. Returns false if already there.
    bool go_future() noexcept {
        if (m_count == 0 || m_cursor == m_head)
            return false;
        m_cursor = do_next_cursor();
        return true;
    }

    /// @brief Step cursor into the past. Returns false if already at the oldest entry.
    bool go_past() noexcept {
        if (m_count == 0)
            return false;
        USize oldest = (m_head + SHEAF_RING_SIZE - (m_count - 1)) % SHEAF_RING_SIZE;
        if (m_cursor == oldest)
            return false;
        m_cursor = do_prev_cursor();
        return true;
    }

    /// @brief Move the cursor to the most recently pushed sheaf.
    void go_present() noexcept {
        if (m_count > 0)
            m_cursor = m_head;
    }

    /**
     * @brief Merge the two most recent sheaves into one (goes to present first).
     * @note Adjacent `MutateCmd` for the same type are coalesced: oldest `prev`, newest `next`.
     * @return False if fewer than two sheaves exist.
     */
    bool compress() noexcept;

private:
    // --------------------------------------------------------------- Internals

    USize do_next_cursor() const noexcept {
        return do_next_cursor(1);
    }
    USize do_next_cursor(USize n) const noexcept {
        return (m_cursor + n) % SHEAF_RING_SIZE;
    }
    USize do_prev_cursor() const noexcept {
        return do_prev_cursor(1);
    }
    USize do_prev_cursor(USize n) const noexcept {
        return (m_cursor + SHEAF_RING_SIZE - n) % SHEAF_RING_SIZE;
    }

    void do_push_advance() noexcept {
        USize next = (m_head + 1) % SHEAF_RING_SIZE;
        if (m_count == SHEAF_RING_SIZE) {
            reinterpret_cast<CmdSheaf *>(m_storage + next * sizeof(CmdSheaf))->~CmdSheaf();
        } else {
            ++m_count;
        }

        m_head = next;
    }
};
} // namespace fr
