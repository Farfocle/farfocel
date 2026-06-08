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
    Thing thing() const noexcept;

    /// @brief Returns the inverse command (for undo).
    Cmd inverse() const noexcept;
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

    explicit CmdBatch(USize arena_size) noexcept;

    /// @brief Construct with an explicit allocator and arena size.
    explicit CmdBatch(Alloc *alloc, USize arena_size = DEFAULT_CMD_BATCH_ARENA_SIZE) noexcept;

    ~CmdBatch() noexcept;

    CmdBatch(const CmdBatch &other) noexcept;
    CmdBatch(CmdBatch &&other) noexcept;

    CmdBatch &operator=(const CmdBatch &other) noexcept;
    CmdBatch &operator=(CmdBatch &&other) noexcept;

    // -------------------------------------------------------------- Record Raw

    /// @brief Copies `part` into the arena and records a deferred insert command.
    void record_insert_raw(Thing thing, TypeIdx tidx, const void *part) noexcept;

    /// @brief Moves `part` into the arena and records a deferred insert command.
    void record_insert_raw(Thing thing, TypeIdx tidx, void *part) noexcept;

    /// @brief Copies `current` into the arena and records a deferred destroy command (snapshot).
    void record_destroy_raw(Thing thing, TypeIdx tidx, const void *current) noexcept;

    /// @brief Copies `prev` and `next` into the arena and records a deferred mutate command.
    void record_mutate_raw(Thing thing, TypeIdx tidx, const void *prev, const void *next) noexcept;

    // ------------------------------------------------------------------ Record

    /**
     * @brief Records a deferred insert for part `T` (copy).
     * @note Does NOT check for duplicate parts - the commit step handles that.
     */
    template <typename T>
    void record_insert(Thing thing, const T &part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(part);

        do_record_insert({
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

        do_record_insert({
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

        do_record_destroy({
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

        do_record_mutate({.tidx = TypeIdx::from_type<T>(),
                          .thing = thing,
                          .prev_offset = do_get_offset(prev_ptr),
                          .next_offset = do_get_offset(next_ptr)});
    }

    /// @brief Records a deferred attach-child relation command.
    void record_attach_child(Thing parent, Thing child) noexcept;

    /// @brief Records a deferred detach-child relation command.
    void record_detach_child(Thing parent, Thing child) noexcept;

    /**
     * @brief Records that `thing` was handed out (for undo tracking).
     * @note The thing is already alive at this point; commit_handout() is a no-op.
     */
    void record_handout(Thing thing) noexcept;

    /**
     * @brief Records a deferred kill — `thing` is killed at commit time.
     * @note Kills are applied after all part mutations and destructions in `commit()`.
     */
    void record_kill(Thing thing) noexcept;

    // ------------------------------------------------------------------ Access

    /// @brief Returns a read-only view of all recorded commands.
    Slice<const Cmd> cmds() const noexcept;

    /// @brief Returns the base of the arena buffer for pointer reconstruction.
    Byte *arena() noexcept;
    const Byte *arena() const noexcept;

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

    // ------------------------------------------------------------------ Merge

    /**
     * @brief Merge `a` (older) and `b` (newer) into one batch.
     * @note Adjacent `MutateCmd` for the same `(thing, type)` pair are coalesced:
     *       oldest `prev` is kept, newest `next` wins.
     */
    static CmdBatch merge(Alloc *alloc, const CmdBatch &a, const CmdBatch &b) noexcept;

    /**
     * @brief Merge all `batches` (oldest first) into one, in a single O(n) pass.
     * @note For each `(thing, type)` pair the first `prev` and last `next` are kept.
     */
    static CmdBatch merge_all(Alloc *alloc, Slice<const CmdBatch *> batches) noexcept;

private:
    void do_record_insert(InsertCmd cmd) noexcept;
    void do_record_destroy(DestroyCmd cmd) noexcept;
    void do_record_mutate(MutateCmd cmd) noexcept;
    void do_record_attach_child(AttachChildCmd cmd) noexcept;
    void do_record_detach_child(DetachChildCmd cmd) noexcept;
    void do_record_handout(HandoutCmd cmd) noexcept;
    void do_record_kill(KillCmd cmd) noexcept;

    USize do_get_offset(void *ptr) const noexcept;
    void do_init_storage(USize size, const char *tag) noexcept;
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

    CmdSheaf &operator=(const CmdSheaf &other) noexcept;
    CmdSheaf &operator=(CmdSheaf &&other) noexcept;

    // -------------------------------------------------------------- Record Raw

    /// @brief Copies `part` into the arena and records a deferred insert command.
    void record_insert_raw(TypeIdx tidx, const void *part) noexcept;

    /// @brief Moves `part` into the arena and records a deferred insert command.
    void record_insert_raw(TypeIdx tidx, void *part) noexcept;

    /// @brief Copies `current` into the arena and records a deferred destroy command (snapshot).
    void record_destroy_raw(TypeIdx tidx, const void *current) noexcept;

    /// @brief Copies `prev` and `next` into the arena and records a deferred mutate command.
    void record_mutate_raw(TypeIdx tidx, const void *prev, const void *next) noexcept;

    // ------------------------------------------------------------------ Record

    /// @brief Records a deferred insert for part `T` (copy). Thing is implicit.
    template <typename T>
    void record_insert(const T &part) noexcept {
        void *ptr = m_arena.allocate(sizeof(T), alignof(T));
        new (ptr) T(part);

        do_record_insert({
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

        do_record_insert({
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

        do_record_destroy({
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

        do_record_mutate({.tidx = TypeIdx::from_type<T>(),
                          .thing = m_thing,
                          .prev_offset = do_get_offset(prev_ptr),
                          .next_offset = do_get_offset(next_ptr)});
    }

    /// @brief Records a deferred attach-child relation (explicit parent and child).
    void record_attach_child(Thing parent, Thing child) noexcept;

    /// @brief Records a deferred detach-child relation (explicit parent and child).
    void record_detach_child(Thing parent, Thing child) noexcept;

    /// @brief Records a deferred kill for `m_thing`.
    void record_kill() noexcept;

    // ------------------------------------------------------------------ Access

    /// @brief Returns the thing this sheaf belongs to.
    Thing thing() const noexcept;

    /// @brief Returns a read-only view of all recorded commands.
    Slice<const Cmd> cmds() const noexcept;

    /// @brief Returns the base of the arena buffer for pointer reconstruction.
    Byte *arena() noexcept;
    const Byte *arena() const noexcept;

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

    /**
     * @brief Merge `a` (older) and `b` (newer) into one sheaf.
     * @note Adjacent `MutateCmd` for the same part type are coalesced:
     * oldest `prev` is kept, newest `next` wins.
     */
    static CmdSheaf merge(Alloc *alloc, const CmdSheaf &a, const CmdSheaf &b) noexcept;

    /**
     * @brief Merge all `sheaves` (oldest first) into one, in a single O(n) pass.
     * @note For each part type the first `prev` and last `next` are kept.
     */
    static CmdSheaf merge_all(Alloc *alloc, Slice<const CmdSheaf *> sheaves) noexcept;

private:
    void do_record_insert(InsertCmd cmd) noexcept;
    void do_record_destroy(DestroyCmd cmd) noexcept;
    void do_record_mutate(MutateCmd cmd) noexcept;
    void do_record_attach_child(AttachChildCmd cmd) noexcept;
    void do_record_detach_child(DetachChildCmd cmd) noexcept;
    void do_record_kill(KillCmd cmd) noexcept;

    USize do_get_offset(void *ptr) const noexcept;
    void do_init_storage(USize size, const char *tag) noexcept;
    void do_copy_cmds(Slice<const Cmd> src_cmds, const Byte *src_base,
                      Thing filter = Thing::nil()) noexcept;

    Alloc *m_alloc{nullptr};
    Thing m_thing{};
    Slice<Byte> m_arena_buffer{};
    ArenaAlloc m_arena{};
    DynamicArray<Cmd> m_cmds{};
};

// ============================================================ CmdBatchTimeline

class CmdBatchTimeline {
    // ----------------------------------------------------------------- Members

    static constexpr USize DEFAULT_BATCH_RING_SIZE = 1 << 8;

    Alloc *m_alloc{get_ambient_ctx().alloc};
    DynamicArray<CmdBatch> m_batches{};
    DynamicArray<USize> m_calendar{};
    USize m_ring_size{0};
    USize m_head{0};
    USize m_cursor{0};
    USize m_time{0};
    USize m_count{0};

public:
    // ----------------------------------------------- Constructors & Destructor

    CmdBatchTimeline() noexcept;
    explicit CmdBatchTimeline(Alloc *alloc, USize ring_size = DEFAULT_BATCH_RING_SIZE) noexcept;

    CmdBatchTimeline(const CmdBatchTimeline &) noexcept = default;
    CmdBatchTimeline(CmdBatchTimeline &&) noexcept = default;
    CmdBatchTimeline &operator=(const CmdBatchTimeline &) noexcept = default;
    CmdBatchTimeline &operator=(CmdBatchTimeline &&) noexcept = default;
    ~CmdBatchTimeline() noexcept = default;

    // --------------------------------------------------------------------- API

    /// @brief Returns the current logical time.
    USize time() const noexcept;

    /// @brief Returns the number of pushed batches.
    USize count() const noexcept;

    /// @brief Returns the batch at the current cursor.
    CmdBatch &batch() noexcept;
    const CmdBatch &batch() const noexcept;

    /// @brief Push a batch onto the timeline (copy).
    void push(const CmdBatch &b, USize dt) noexcept;

    /// @brief Push a batch onto the timeline (move).
    void push(CmdBatch &&b, USize dt) noexcept;

    /// @brief Step cursor toward the present. Returns false if already there.
    bool go_future() noexcept;

    /// @brief Step cursor into the past. Returns false if already at the oldest entry.
    bool go_past() noexcept;

    /// @brief Go to the present, i.e. the most recently pushed batch.
    void go_present() noexcept;

    /// @brief Number of batches ahead of the cursor (toward the present).
    USize count_future() const noexcept;

    /// @brief Number of batches behind the cursor (toward the past).
    USize count_past() const noexcept;

    /**
     * @brief Merge the two most recent batches into one (goes to present first).
     * @note Adjacent `MutateCmd` for the same `(thing, type)` are coalesced.
     * @return False if fewer than two batches exist.
     */
    bool compress() noexcept;

    /**
     * @brief Collapse the entire timeline into a single batch in one O(n) pass.
     * @note For each `(thing, type)` pair the oldest `prev` and newest `next` are kept.
     * @return False if fewer than two batches exist.
     */
    bool compress_all() noexcept;

    /// @brief Resets all active batches and clears the entire timeline to empty.
    void clear_all() noexcept;

    /// @brief Resets all batches ahead of the cursor, making the cursor the new present.
    void clear_future() noexcept;

    /// @brief Resets all batches behind the cursor, making the cursor the new oldest entry.
    void clear_past() noexcept;

private:
    // --------------------------------------------------------------- Internals

    USize do_next(USize idx) const noexcept {
        return (idx + 1) % m_ring_size;
    }
    USize do_prev(USize idx) const noexcept {
        return (idx + m_ring_size - 1) % m_ring_size;
    }

    void do_push_advance() noexcept;
};

// ============================================================ CmdSheafTimeline

/**
 * @brief Ring-buffered timeline of CmdSheafs for a single thing.
 * @note Supports navigation and merge-compression of adjacent sheaves with `MutatePart` coalescing.
 */
class CmdSheafTimeline {
    // ----------------------------------------------------------------- Members

    static constexpr USize DEFAULT_SHEAF_RING_SIZE = 1 << 5;

    Alloc *m_alloc{get_ambient_ctx().alloc};
    Thing m_thing{Thing::nil()};
    Byte *m_storage{nullptr};
    DynamicArray<USize> m_calendar{};
    USize m_ring_size{0};
    USize m_head{0};
    USize m_cursor{0};
    USize m_time{0};
    USize m_count{0};

public:
    // ----------------------------------------------- Constructors & Destructor

    CmdSheafTimeline() noexcept = default;
    explicit CmdSheafTimeline(Alloc *alloc, Thing thing,
                              USize ring_size = DEFAULT_SHEAF_RING_SIZE) noexcept;
    CmdSheafTimeline(const CmdSheafTimeline &other) noexcept;
    CmdSheafTimeline(CmdSheafTimeline &&other) noexcept;
    CmdSheafTimeline &operator=(const CmdSheafTimeline &other) noexcept;
    CmdSheafTimeline &operator=(CmdSheafTimeline &&other) noexcept;
    ~CmdSheafTimeline() noexcept;

    // --------------------------------------------------------------------- API

    /// @brief Returns the current logical time.
    USize time() const noexcept;

    /// @brief Returns the number of live sheaves.
    USize count() const noexcept;

    /// @brief Returns the thing all sheaves belong to.
    Thing thing() const noexcept;

    /// @brief Returns the sheaf at the current cursor. @pre count() > 0.
    CmdSheaf &sheaf() noexcept;
    const CmdSheaf &sheaf() const noexcept;

    /// @brief Push a sheaf onto the timeline (copy).
    void push(const CmdSheaf &s, USize dt) noexcept;

    /// @brief Push a sheaf onto the timeline (move).
    void push(CmdSheaf &&s, USize dt) noexcept;

    /// @brief Step cursor toward the present. Returns false if already there.
    bool go_future() noexcept;

    /// @brief Step cursor into the past. Returns false if already at the oldest entry.
    bool go_past() noexcept;

    /// @brief Move the cursor to the most recently pushed sheaf.
    void go_present() noexcept;

    /// @brief Number of sheaves ahead of the cursor (toward the present).
    USize count_future() const noexcept;

    /// @brief Number of sheaves behind the cursor (toward the past).
    USize count_past() const noexcept;

    /**
     * @brief Merge the two most recent sheaves into one (goes to present first).
     * @note Adjacent `MutateCmd` for the same type are coalesced: oldest `prev`, newest `next`.
     * @return False if fewer than two sheaves exist.
     */
    bool compress() noexcept;

    /**
     * @brief Collapse the entire timeline into a single sheaf in one O(n) pass.
     * @note For each part type the oldest `prev` and newest `next` are kept.
     * @return False if fewer than two sheaves exist.
     */
    bool compress_all() noexcept;

    /// @brief Destroys all active sheaves and clears the entire timeline to empty.
    void clear_all() noexcept;

    /// @brief Destroys all sheaves ahead of the cursor, making the cursor the new present.
    void clear_future() noexcept;

    /// @brief Destroys all sheaves behind the cursor, making the cursor the new oldest entry.
    void clear_past() noexcept;

private:
    // --------------------------------------------------------------- Internals

    USize do_next(USize idx) const noexcept {
        return (idx + 1) % m_ring_size;
    }

    USize do_prev(USize idx) const noexcept {
        return (idx + m_ring_size - 1) % m_ring_size;
    }

    CmdSheaf *do_sheaf_at(USize idx) noexcept {
        return reinterpret_cast<CmdSheaf *>(m_storage + idx * sizeof(CmdSheaf));
    }

    const CmdSheaf *do_sheaf_at(USize idx) const noexcept {
        return reinterpret_cast<const CmdSheaf *>(m_storage + idx * sizeof(CmdSheaf));
    }

    void do_destroy_all() noexcept;
    void do_push_advance() noexcept;
};
} // namespace fr
