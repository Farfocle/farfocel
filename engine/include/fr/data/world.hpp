/**
 * @file world.hpp
 * @author Kiju
 *
 * @brief World is the heart of all data operations in Farfocel.
 */

#pragma once

#include <utility>

#include "fr/core/algo.hpp"
#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/bitset.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/inline_function.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/data/cmd.hpp"
#include "fr/data/part.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/relations.hpp"
#include "fr/data/resource.hpp"
#include "fr/data/thing.hpp"

namespace fr {
// ======================================================================= Scope

class World;

/**
 * @brief Scope is a mini version of the World that forwards thing and part operations.
 * It is safe to pass around inside systems for world mutation.
 */
class Scope {
public:
    // ------------------------------------------------------------ Constructors

    Scope() noexcept;
    Scope(World *world) noexcept;

    Scope(const Scope &) noexcept = default;
    Scope(Scope &&) noexcept = default;
    Scope &operator=(const Scope &) noexcept = default;
    Scope &operator=(Scope &&) noexcept = default;

    // -------------------------------------------------------- Thing Operations

    /// @brief Returns a fresh, non-nil thing immediately.
    Thing spawn() noexcept;

    /// @brief Hands out a new thing immediately and records it.
    Thing spawn_deferred() noexcept;

    /**
     * @brief Kills a thing and destroys all of its parts immediately.
     * @note If `thing` is nil or dead; does nothing.
     */
    void kill(Thing thing) noexcept;

    /// @brief Records a deferred kill.
    void kill_deferred(Thing thing) noexcept;

    /**
     * @brief Returns true if a thing is alive.
     * @note The nil thing is alive.
     */
    bool is_alive(Thing thing) const noexcept;

    /**
     * @brief Returns true if a thing is dead.
     * @note The nil thing is not dead.
     */
    bool is_dead(Thing thing) const noexcept;

    // --------------------------------------------------------- Part Operations

    /// @brief Returns true if the thing has part `T`.
    template <typename T>
    bool has(Thing thing) const noexcept;

    /**
     * @brief Inserts or overrides part `T` on a thing immediately.
     * @note If thing is nil; returns the stub pointer.
     * @note If thing is dead; returns nullptr.
     * @note If thing is alive; inserts part `T` or ovverides it if alread present.
     */
    template <typename T, typename... Args>
    T *try_emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Inserts part `T` immediately without any checks.
     * @pre Caller must ensure: thing is alive, thing does NOT yet have part `T`.
     */
    template <typename T, typename... Args>
    T &emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Inserts or overrides part `T` on a thing immediately (copy).
     * @note If thing is nil; does nothing.
     * @note If thing is dead; does nothing.
     * @note If thing is alive; inserts part `T` or overrides it if already present.
     */
    template <typename T>
    void insert_now(Thing thing, const T &part) noexcept;

    /**
     * @brief Inserts or overrides part `T` on a thing immediately (move).
     * @note If thing is nil; does nothing.
     * @note If thing is dead; does nothing.
     * @note If thing is alive; inserts part `T` or overrides it if already present.
     */
    template <typename T>
    void insert_now(Thing thing, T &&part) noexcept;

    /**
     * @brief Destroys part `T` on a thing immediately.
     * @return false if thing is nil, dead, or does NOT have part `T`.
     */
    template <typename T>
    bool try_destroy_now(Thing thing) noexcept;

    /**
     * @brief Destroys part `T` on a thing immediately.
     * @pre Caller must ensure: thing is alive and has `T`.
     */
    template <typename T>
    void destroy_now(Thing thing) noexcept;

    /// @brief Returns a pointer to part `T` owned by the thing, or nullptr if not found.
    template <typename T>
    T *try_get(Thing thing) noexcept;

    /**
     * @brief Returns a reference to part `T` owned by the thing.
     * @pre Caller must ensure: thing is alive and has `T`.
     */
    template <typename T>
    T &get(Thing thing) noexcept;

    // --------------------------------------------------------------- Relations

    /**
     * @brief Attaches a child to the parent immediately. Updates hierarchy.
     * @return false if either thing is nil.
     * @pre Both things must have `Relations` part.
     */
    bool attach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a deferred `AttachChild` command.
     * @pre Both things must have `Relations` part.
     * @note If either thing is nil; does nothing.
     */
    void attach_child(Thing parent, Thing child) noexcept;

    /**
     * @brief Detaches a child from a parent immediately. Updates hierarchy.
     * @return false if either thing is nil, or child is not actually a child of parent.
     * @pre Both things must have `Relations` part.
     */
    bool detach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a deferred `DetachChild` command.
     * @pre Both things must have `Relations` part.
     * @note If either thing is nil; does nothing.
     */
    void detach_child(Thing parent, Thing child) noexcept;

    // ------------------------------------------------- Command Part Operations

    /**
     * @brief Records a deferred insert-or-override command for part `T` on a thing.
     * @note Does nothing if thing is nil or dead.
     * @note If thing already has part `T`, the existing value is overridden at commit time.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept;

    /**
     * @brief Records a deferred insert-or-override command for part `T` on a thing.
     * @note Does nothing if thing is nil or dead.
     * @note If thing already has part `T`, the existing value is overridden at commit time.
     */
    template <typename T>
    void insert(Thing thing, T &&part) noexcept;

    /**
     * @brief Records a deferred destroy command for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or does NOT have part `T`.
     */
    template <typename T>
    void destroy(Thing thing) noexcept;

    // ------------------------------------------------------------------- Query

    /// @brief Creates a forward query for things owning all parts in the Include list.
    template <typename... Include>
    auto query(QueryOptions options = {}) noexcept;

    /// @brief Creates a reverse query for things owning all parts in the Include list.
    template <typename... Include>
    auto reverse_query(QueryOptions options = {}) noexcept;

    /// @brief Creates a query over the direct children of a thing.
    template <typename... Include>
    auto shallow_query(Thing thing, QueryOptions options = {}) noexcept;

    /// @brief Creates a depth-first query over all descendants of a thing.
    template <typename... Include>
    auto deep_query(Thing thing, QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a top-down (parents first) forward query.
     * @note This works on any part pools - not only the sorted ones.
     */
    template <typename... Include>
    auto top_down_query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a bottom-up (leaves first) forward query.
     * @note This works on any part pools - not only the sorted ones.
     */
    template <typename... Include>
    auto bottom_up_query(QueryOptions options = {}) noexcept;

    // ---------------------------------------------------------------- Resources

    /**
     * @brief Tries to get a resource of type `T` from the pool.
     * @tparam T The type of the resource to get.
     * @return A pointer to the resource if it exists, `nullptr` otherwise.
     */
    template <typename T>
    T *try_get_resource() noexcept;

    /**
     * @brief Gets a resource of type `T` from the pool.
     * @tparam T The type of the resource to get.
     * @return A reference to the resource.
     */
    template <typename T>
    T &get_resource() noexcept;

    /**
     * @brief Checks if a resource of type `T` exists in the pool.
     * @tparam T The type of the resource to check.
     * @return `true` if the resource exists, `false` otherwise.
     */
    template <typename T>
    bool check_resource() noexcept;

    /// @brief Returns the underlying world. Useful when a system needs full World access.
    World &world() noexcept {
        return *m_world;
    }

private:
    // -------------------------------------------------------- Member Variables
    World *m_world{nullptr};
};

// ====================================================================== Script

template <typename T>
concept ScriptHasOnPreUpdate = requires(T script) {
    { script.on_pre_update() } -> std::same_as<void>;
};

template <typename T>
concept ScriptHasOnUpdate = requires(T script) {
    { script.on_update() } -> std::same_as<void>;
};

template <typename T>
concept ScriptHasOnPostUpdate = requires(T script) {
    { script.on_post_update() } -> std::same_as<void>;
};

template <typename T>
concept ScriptHasOnInit = requires(T script) {
    { script.on_init() } -> std::same_as<void>;
};

template <typename T>
concept ScriptHasOnDestroy = requires(T script) {
    { script.on_destroy() } -> std::same_as<void>;
};

class Script {
public:
    // ----------------------------------------------- Constructors & Destructor
    Script() noexcept = default;
    Script(const Script &) noexcept = default;
    Script(Script &&) noexcept = default;
    Script &operator=(const Script &) noexcept = default;
    Script &operator=(Script &&) noexcept = default;
    virtual ~Script() noexcept = default;

    // ----------------------------------------------------------------- Members

    /// @brief The scope this script operates in. Set automatically at insert time.
    Scope scope{};

    /// @brief The thing this script is attached to. Set automatically at insert time.
    Thing self{Thing::nil()};

    // ------------------------------------------------------------------ Things

    /// @brief Spawns a new thing immediately.
    Thing spawn() noexcept;

    /// @brief Kills a thing immediately.
    void kill(Thing thing) noexcept;

    // --------------------------------------------------------------- Hierarchy

    /// @brief Attaches `child` to self as a parent immediately. Updates hierarchy.
    /// @return false if either thing is nil.
    bool attach_child_now(Thing child) noexcept;

    /// @brief Detaches `child` from self immediately. Updates hierarchy.
    /// @return false if `child` is not actually a child of self.
    bool detach_child_now(Thing child) noexcept;

    // ------------------------------------------------------------------- Parts

    /// @brief Returns true if self has part `T`.
    template <typename T>
    bool has() const noexcept;

    /**
     * @brief Returns a reference to part `T` on self.
     * @pre self must have part `T`.
     */
    template <typename T>
    T &get() noexcept;

    /// @brief Inserts or overrides part `T` on self immediately (checked). Returns pointer.
    template <typename T, typename... Args>
    T *try_emplace_now(Args &&...args) noexcept;

    /**
     * @brief Emplaces part `T` on self immediately (unchecked). Returns reference.
     * @pre self must NOT yet have part `T`.
     */
    template <typename T, typename... Args>
    T &emplace_now(Args &&...args) noexcept;

    /// @brief Inserts or overrides part `T` on self immediately (copy).
    template <typename T>
    void insert_now(const T &part) noexcept;

    /// @brief Inserts or overrides part `T` on self immediately (move).
    template <typename T>
    void insert_now(T &&part) noexcept;

    /// @brief Destroys part `T` on self immediately. Returns false if not present.
    template <typename T>
    bool try_destroy_now() noexcept;

    /**
     * @brief Destroys part `T` on self immediately.
     * @pre self must have part `T`.
     */
    template <typename T>
    void destroy_now() noexcept;

    /// @brief Records a deferred insert for part `T` on self (copy).
    template <typename T>
    void insert(const T &part) noexcept;

    /// @brief Records a deferred insert for part `T` on self (move).
    template <typename T>
    void insert(T &&part) noexcept;

    /// @brief Records a deferred destroy for part `T` on self.
    template <typename T>
    void destroy() noexcept;
};

template <typename T>
concept IsScript = std::derived_from<T, Script>;

// ================================================================== SystemPool

using StageStorageType = U8;

enum class Stage : StageStorageType {
    PreUpdate,
    PreUpdateScript,
    Update,
    UpdateScript,
    PostUpdate,
    PostUpdateScript
};

constexpr U8 STAGE_COUNT = 6;

using System = Fn128<void(Scope)>;

namespace impl {
class SystemPool {
public:
    SystemPool() noexcept;
    explicit SystemPool(Alloc *alloc) noexcept;

    SystemPool(const SystemPool &) = delete;
    SystemPool(SystemPool &&) = delete;
    SystemPool &operator=(const SystemPool &) = delete;
    SystemPool &operator=(SystemPool &&) = delete;

    /// @brief Schedules a system for synchronous execution in the given stage.
    void schedule(Stage stage, const System &system) noexcept;

    /// @brief Runs all systems scheduled for the given stage synchronously.
    void run_stage(Stage stage, Scope scope);

private:
    /// @brief Executes all systems for a given stage index.
    void do_run_stage(U8 stage_idx, Scope scope) noexcept;

    const Alloc *m_alloc{nullptr};
    Array<DynamicArray<System>, STAGE_COUNT> m_stages{};
};
} // namespace impl

// ======================================================================= World

class World {
public:
    // -------------------------------------------------- Options & Constructors

    struct Options {
        Alloc *registry_alloc{get_ambient_ctx().alloc};
        Alloc *system_pool_alloc{get_ambient_ctx().alloc};
        Alloc *resource_pool_alloc{get_ambient_ctx().alloc};
        USize cmd_batch_arena_size{DEFAULT_CMD_BATCH_ARENA_SIZE};
    };

    World() noexcept;
    explicit World(const Options &opt) noexcept;

    World(const World &) = delete;
    World(World &&) = delete;
    World &operator=(const World &) = delete;
    World &operator=(World &&) = delete;

    // -------------------------------------------------------- Thing Operations

    /// @brief Returns a fresh, non-nil thing immediately.
    Thing spawn() noexcept;

    /**
     * @brief Hands out a new thing immediately, then records it in the batch for tracking.
     * @return The newly created thing (already alive).
     * @note The thing is created now so the handle is usable right away.
     */
    Thing spawn_deferred() noexcept;

    /**
     * @brief Kills a thing and destroys all of its parts immediately.
     * @note If the thing is nil or dead; does nothing.
     */
    void kill(Thing thing) noexcept;

    /**
     * @brief Records a deferred kill.
     * @note Safe to call while iterating over part pools; the actual kill is postponed.
     * @note If `thing` is nil; does nothing.
     */
    void kill_deferred(Thing thing) noexcept;

    /**
     * @brief Returns true if a thing is alive.
     * @note The nil thing is alive.
     */
    bool is_alive(Thing thing) const noexcept;

    /**
     * @brief Returns true if a thing is dead.
     * @note The nil thing is not dead.
     */
    bool is_dead(Thing thing) const noexcept;

    // --------------------------------------------------------- Part Operations

    /// @brief Returns true if the thing has part `T`.
    template <typename T>
    bool has(Thing thing) const noexcept;

    /**
     * @brief Inserts or overrides part `T` on a thing immediately.
     * @note If thing is nil; returns the stub pointer.
     * @note If thing is dead; returns nullptr.
     * @note If thing is alive; inserts part `T` or ovverides it if alread present.
     */
    template <typename T, typename... Args>
    T *try_emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Inserts part `T` immediately without any checks.
     * @pre Caller must ensure: thing is alive, thing does NOT yet have part `T`.
     */
    template <typename T, typename... Args>
    T &emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Inserts or overrides part `T` on a thing immediately (copy).
     * @note If thing is nil; does nothing.
     * @note If thing is dead; does nothing.
     * @note If thing is alive; inserts part `T` or overrides it if already present.
     */
    template <typename T>
    void insert_now(Thing thing, const T &part) noexcept;

    /**
     * @brief Inserts or overrides part `T` on a thing immediately (move).
     * @note If thing is nil; does nothing.
     * @note If thing is dead; does nothing.
     * @note If thing is alive; inserts part `T` or overrides it if already present.
     */
    template <typename T>
    void insert_now(Thing thing, T &&part) noexcept;

    /**
     * @brief Destroys part `T` on a thing immediately.
     * @return false if thing is nil, dead, or does NOT have part `T`.
     */
    template <typename T>
    bool try_destroy_now(Thing thing) noexcept;

    /**
     * @brief Destroys part `T` on a thing immediately.
     * @pre Caller must ensure: thing is alive and has `T`.
     */
    template <typename T>
    void destroy_now(Thing thing) noexcept;

    /// @brief Returns a pointer to part `T` owned by the thing, or nullptr if not found.
    template <typename T>
    T *try_get(Thing thing) noexcept;

    /**
     * @brief Returns a reference to part `T` owned by the thing.
     * @pre Caller must ensure: thing is alive and has `T`.
     */
    template <typename T>
    T &get(Thing thing) noexcept;

    // -------------------------------------------------- Raw Part Operations

    /// @brief Returns true if `thing` owns the part for `tidx`.
    bool has_raw(TypeIdx tidx, Thing thing) const noexcept;

    /**
     * @brief Inserts or overrides the part for `tidx` on `thing` immediately (move, checked).
     * @note Returns nullptr if thing is dead. Returns stub pointer for nil thing.
     * @pre `ensure<T>()` (or equivalent) must have been called for this type.
     */
    void *try_emplace_now_raw(TypeIdx tidx, Thing thing, void *part) noexcept;

    /**
     * @brief Inserts or overrides the part for `tidx` on `thing` immediately (copy, checked).
     * @note Returns nullptr if thing is dead. Returns stub pointer for nil thing.
     * @pre `ensure<T>()` (or equivalent) must have been called for this type.
     */
    void *try_emplace_now_raw(TypeIdx tidx, Thing thing, const void *part) noexcept;

    /**
     * @brief Inserts the part for `tidx` on `thing` immediately (move, unchecked).
     * @pre Caller guarantees: thing is alive and does NOT yet own this part.
     */
    void *emplace_now_raw(TypeIdx tidx, Thing thing, void *part) noexcept;

    /**
     * @brief Inserts the part for `tidx` on `thing` immediately (copy, unchecked).
     * @pre Caller guarantees: thing is alive and does NOT yet own this part.
     */
    void *emplace_now_raw(TypeIdx tidx, Thing thing, const void *part) noexcept;

    /**
     * @brief Inserts or overrides the part for `tidx` on `thing` immediately (move, checked).
     * @note Does nothing if thing is nil or dead.
     */
    void insert_now_raw(TypeIdx tidx, Thing thing, void *part) noexcept;

    /**
     * @brief Inserts or overrides the part for `tidx` on `thing` immediately (copy, checked).
     * @note Does nothing if thing is nil or dead.
     */
    void insert_now_raw(TypeIdx tidx, Thing thing, const void *part) noexcept;

    /**
     * @brief Destroys the part for `tidx` on `thing` immediately (checked).
     * @return false if thing is nil, dead, or does NOT own this part.
     */
    bool destroy_now_raw(TypeIdx tidx, Thing thing) noexcept;

    /**
     * @brief Destroys the part for `tidx` on `thing` immediately (unchecked).
     * @pre Caller guarantees: thing is alive and owns this part.
     */
    void destroy_now_unchecked_raw(TypeIdx tidx, Thing thing) noexcept;

    /**
     * @brief Returns a raw pointer to the part for `tidx`, or nullptr if absent (checked).
     * @note Returns stub pointer for nil thing.
     */
    void *try_get_raw(TypeIdx tidx, Thing thing) noexcept;

    /**
     * @brief Returns a raw pointer to the part for `tidx` (unchecked).
     * @pre Caller guarantees: thing is alive and owns this part.
     */
    void *get_raw(TypeIdx tidx, Thing thing) noexcept;

    /**
     * @brief Sorts the entire part pool `T` using the hierarchy depth.
     * @note This allows for BFS-like traversals with a normal forward and reverse queries for
     * sorted pools.
     */
    template <typename T>
    void sort_by_hierarchy_depth() noexcept;

    // --------------------------------------------------------------- Relations

    /**
     * @brief Attaches a child to the parent immediately. Updates hierarchy.
     * @return false if either thing is nil.
     * @pre Both things must have `Relations` part.
     */
    bool attach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a deferred `AttachChild` command.
     * @pre Both things must have `Relations` part.
     * @note If either thing is nil; does nothing.
     */
    void attach_child(Thing parent, Thing child) noexcept;

    /**
     * @brief Detaches a child from a parent immediately. Updates hierarchy.
     * @return false if either thing is nil, or child is not actually a child of parent.
     * @pre Both things must have `Relations` part.
     */
    bool detach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a deferred `DetachChild` command.
     * @pre Both things must have `Relations` part.
     * @note If either thing is nil; does nothing.
     */
    void detach_child(Thing parent, Thing child) noexcept;

    /**
     * @brief Updates hierarchy.
     *
     * @param root from which root to update the hierarchy from.
     * @pre `root` must have `Relations` part.
     *
     * @note If `root` is nil; does nothing.
     * @details This only updates the `depth` field of `Relations` part.
     */
    void update_hierarchy(Thing root) noexcept;

    // ------------------------------------------------- Command Part Operations

    /**
     * @brief Apply all insert commands from the provided batch across all part pools.
     * @note If `invert` is true, applies inverse insert commands (destroy).
     */
    void commit_insert_from(CmdBatch &batch, bool invert = false) noexcept;

    /**
     * @brief Apply all destroy commands from the provided batch across all part pools.
     * @note If `invert` is true, applies inverse destroy commands (insert).
     */
    void commit_destroy_from(CmdBatch &batch, bool invert = false) noexcept;

    /**
     * @brief Apply all mutate commands from the provided batch across all part pools.
     * @note If `invert` is true, applies inverse mutate commands.
     */
    void commit_mutate_from(CmdBatch &batch, bool invert = false) noexcept;

    /**
     * @brief Apply all attach-child commands from the provided batch.
     * @note If `invert` is true, applies inverse attach-child commands (detach).
     */
    void commit_attach_child_from(CmdBatch &batch, bool invert = false) noexcept;

    /**
     * @brief Apply all detach-child commands from the provided batch.
     * @note If `invert` is true, applies inverse detach-child commands (attach).
     */
    void commit_detach_child_from(CmdBatch &batch, bool invert = false) noexcept;

    /**
     * @brief Apply all handout commands from the provided batch.
     * @note No-op; exists for symmetry and future hooks.
     */
    void commit_spawn_from(CmdBatch &batch, bool invert = false) noexcept;

    /**
     * @brief Apply all kill commands from the provided batch.
     * @note If `invert` is true, applies inverse kill commands (handout).
     */
    void commit_kill_from(CmdBatch &batch, bool invert = false) noexcept;

    /**
     * @brief Apply all commands in the provided batch:
     * mutate -> destroy -> insert -> attach -> detach -> kill.
     */
    void commit_from(CmdBatch &batch) noexcept;

    /**
     * @brief Apply all inverse commands from the provided batch:
     * kill' -> detach' -> attach' -> insert' -> destroy' -> mutate'.
     */
    void commit_inverse_from(CmdBatch &batch) noexcept;

    /// @brief Apply all recorded insert commands across all part pools.
    void commit_insert(bool invert = false) noexcept {
        commit_insert_from(m_cmd_batch, invert);
    }

    /// @brief Apply all recorded destroy commands across all part pools.
    void commit_destroy(bool invert = false) noexcept {
        commit_destroy_from(m_cmd_batch, invert);
    }

    /// @brief Apply all recorded mutate commands across all part pools.
    void commit_mutate(bool invert = false) noexcept {
        commit_mutate_from(m_cmd_batch, invert);
    }

    /// @brief Apply all recorded attach-child commands.
    void commit_attach_child(bool invert = false) noexcept {
        commit_attach_child_from(m_cmd_batch, invert);
    }

    /// @brief Apply all recorded detach-child commands.
    void commit_detach_child(bool invert = false) noexcept {
        commit_detach_child_from(m_cmd_batch, invert);
    }

    /// @brief No-op (things are already alive). Exists for symmetry and future use.
    void commit_spawn(bool invert = false) noexcept {
        commit_spawn_from(m_cmd_batch, invert);
    }

    /// @brief Kill all things recorded via `kill_deferred()`.
    void commit_kill(bool invert = false) noexcept {
        commit_kill_from(m_cmd_batch, invert);
    }

    /// @brief Apply all commands: mutate -> destroy -> insert -> attach -> detach -> kill.
    void commit() noexcept {
        commit_from(m_cmd_batch);
    }

    /**
     * @brief Apply all inverse commands:
     * kill' -> detach' -> attach' -> insert' -> destroy' -> mutate'
     */
    void commit_inverse() noexcept {
        commit_inverse_from(m_cmd_batch);
    }

    /**
     * @brief Records a deferred insert-or-override command for part `T` on a thing.
     * @note Does nothing if thing is nil or dead.
     * @note If thing already has part `T`, the existing value is overridden at commit time.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept;

    /**
     * @brief Records a deferred insert-or-override command for part `T` on a thing.
     * @note Does nothing if thing is nil or dead.
     * @note If thing already has part `T`, the existing value is overridden at commit time.
     */
    template <typename T>
    void insert(Thing thing, T &&part) noexcept;

    /**
     * @brief Records a deferred destroy command for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or does NOT have part `T`.
     */
    template <typename T>
    void destroy(Thing thing) noexcept;

    // ---------------------------------------------- Raw Deferred Part Operations

    /// @brief Records a deferred insert for `tidx` on `thing` (copy).
    void insert_raw(TypeIdx tidx, Thing thing, const void *part) noexcept;

    /// @brief Records a deferred insert for `tidx` on `thing` (move).
    void insert_raw(TypeIdx tidx, Thing thing, void *part) noexcept;

    /// @brief Records a deferred destroy for `tidx` on `thing`, snapshotting `current`.
    void destroy_raw(TypeIdx tidx, Thing thing, const void *current) noexcept;

    /// @brief Records a deferred mutate for `tidx` on `thing`, capturing `prev` and `next`.
    void mutate_raw(TypeIdx tidx, Thing thing, const void *prev, const void *next) noexcept;

    // ------------------------------------------------------------------- Query

    /// @brief Creates a forward query for things owning all parts in the Include list.
    template <typename... Include>
    auto query(QueryOptions options = {}) noexcept;

    /// @brief Creates a reverse query for things owning all parts in the Include list.
    template <typename... Include>
    auto reverse_query(QueryOptions options = {}) noexcept;

    /// @brief Creates a query over the direct children of a thing.
    template <typename... Include>
    auto shallow_query(Thing thing, QueryOptions options = {}) noexcept;

    /// @brief Creates a depth-first query over all descendants of a thing.
    template <typename... Include>
    auto deep_query(Thing thing, QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a top-down (parents first) forward query.
     * @note This works on any part pools - not only the sorted ones.
     */
    template <typename... Include>
    auto top_down_query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a bottom-up (leaves first) forward query.
     * @note This works on any part pools - not only the sorted ones.
     */
    template <typename... Include>
    auto bottom_up_query(QueryOptions options = {}) noexcept;

    /// @brief Returns a reference to the world's active command batch.
    CmdBatch &cmd_batch() noexcept {
        return m_cmd_batch;
    }

    // ------------------------------------------------------------------- Shape

    /// @brief Ensures the part pool for the given type exists so it can be deserialized.
    template <typename T>
    void ensure() noexcept {
        m_registry.ensure<T>();
    }

    /// @brief Serializes the world (things, parts, resources) to JSON.
    void shape(JsonWriterArchive &archive) noexcept;

    /// @brief Deserializes the world from JSON. Call ensure<T>() for each part type first.
    void shape(JsonReaderArchive &archive) noexcept;

    // ----------------------------------------------------------------- Systems

    /// @brief Schedules a system for synchronous execution in the given stage.
    void schedule(Stage stage, const System &system) noexcept;

    /// @brief Runs all systems scheduled for the given stage synchronously.
    void run_stage(Stage stage) noexcept;

    /**
     * @brief Runs all cheduled systems.
     * @note The order of execution:
     * 1. `PreUpdate`
     * 2. `PreUpdateScript`
     * 3. `Update`
     * 4. `UpdateScript`
     * 5. `PostUpdate`
     * 6. `PostUpdateScript`
     */
    void run() noexcept;

    // --------------------------------------------------------------- Resources

    /**
     * @brief Emplaces a resource `T` with the given arguments.
     *
     * @tparam T The type of the resource to emplace.
     * @tparam Args The types of the arguments to pass to the constructor of `T`.
     * @param args The arguments to pass to the constructor of `T`.
     * @return A reference to the emplaced resource.
     *
     * @note If a resource `T` already exists, it will be destroyed and replaced with the
     * new resource.
     */
    template <typename T, typename... Args>
    T &emplace_resource(Args &&...args) noexcept {
        return m_resource_pool.emplace<T>(std::forward<Args>(args)...);
    };

    /**
     * @brief Inserts a resource `T` into the pool.
     *
     * @tparam T The type of the resource to insert.
     * @param t The resource to insert.
     *
     * @note If a resource of type `T` already exists, it will be destroyed and replaced with the
     * new resource.
     */
    template <typename T>
    void insert_resource(T &&t) noexcept {
        m_resource_pool.insert<T>(std::move(t));
    }

    /**
     * @brief Inserts a resource `T` into the pool.
     *
     * @tparam T The type of the resource to insert.
     * @param t The resource to insert.
     *
     * @note If a resource of type `T` already exists, it will be destroyed and replaced with the
     * new resource.
     */
    template <typename T>
    void insert_resource(const T &t) noexcept {
        m_resource_pool.insert<T>(t);
    }

    /**
     * @brief Destroys a resource of type `T` from the pool.
     * @tparam T The type of the resource to destroy.
     * @return `true` if the resource was destroyed, `false` if it was not found.
     */
    template <typename T>
    bool destroy() {
        return m_resource_pool.destroy<T>();
    }

    /**
     * @brief Checks if a resource of type `T` exists in the pool.
     * @tparam T The type of the resource to check.
     * @return `true` if the resource exists, `false` otherwise.
     */
    template <typename T>
    bool check_resource() {
        return m_resource_pool.check<T>();
    }

    /**
     * @brief Tries to get a resource of type `T` from the pool.
     * @tparam T The type of the resource to get.
     * @return A pointer to the resource if it exists, `nullptr` otherwise.
     */
    template <typename T>
    T *try_get_resource() {
        return m_resource_pool.try_get<T>();
    }

    /**
     * @brief Gets a resource of type `T` from the pool.
     * @tparam T The type of the resource to get.
     * @return A reference to the resource.
     */
    template <typename T>
    T &get_resource() {
        return m_resource_pool.get<T>();
    }

private:
    // --------------------------------------------------------------- Internals
    void do_update_hierarchy(HierarchyDepth parent_depth, Thing child) noexcept;
    void do_detach_from_hierarchy_unchecked(Thing thing) noexcept;

    /// @brief Registers lifecycle systems for script type `S` if not already registered.
    template <IsScript S>
    void do_register_script_type() noexcept;

    // ----------------------------------------------------------------- Members
    Options m_options{};
    impl::Registry m_registry;
    impl::SystemPool m_system_pool{};
    Bitset<MAX_PARTS> m_script_registry{};
    CmdBatch m_cmd_batch;
    impl::ResourcePool m_resource_pool{};
};

// =========================================== SystemPool Method Implementations

inline impl::SystemPool::SystemPool() noexcept
    : SystemPool(get_ambient_ctx().alloc) {
}

inline impl::SystemPool::SystemPool(Alloc *alloc) noexcept {
    m_alloc = alloc;
    for (auto &systems : m_stages) {
        systems = DynamicArray<System>::with_alloc(alloc);
    }
}

inline void impl::SystemPool::schedule(Stage stage, const System &system) noexcept {
    m_stages[static_cast<U8>(stage)].push_back(system);
}

inline void impl::SystemPool::run_stage(Stage stage, Scope scope) {
    do_run_stage(static_cast<U8>(stage), scope);
}

inline void impl::SystemPool::do_run_stage(U8 stage_idx, Scope scope) noexcept {
    for (const auto &system : m_stages[stage_idx]) {
        system(scope);
    }
}

// ================================================ World Method Implementations

inline World::World() noexcept
    : World(Options{}) {
}

inline World::World(const Options &opt) noexcept
    : m_options(opt),
      m_registry(opt.registry_alloc),
      m_system_pool(opt.system_pool_alloc),
      m_script_registry(Bitset<MAX_PARTS>::with_zeros()),
      m_cmd_batch(opt.registry_alloc, opt.cmd_batch_arena_size),
      m_resource_pool(opt.resource_pool_alloc) {
}

inline Thing World::spawn() noexcept {
    return m_registry.spawn();
}

inline Thing World::spawn_deferred() noexcept {
    Thing thing = m_registry.spawn();
    m_cmd_batch.record_spawn(thing);
    return thing;
}

inline void World::kill(Thing thing) noexcept {
    m_registry.kill(thing);
}

inline void World::kill_deferred(Thing thing) noexcept {
    if (thing.is_nil()) [[unlikely]]
        return;
    m_cmd_batch.record_kill(thing);
}

inline bool World::is_alive(Thing thing) const noexcept {
    return m_registry.is_alive(thing);
}

inline bool World::is_dead(Thing thing) const noexcept {
    return m_registry.is_dead(thing);
}

template <typename T>
inline bool World::has(Thing thing) const noexcept {
    return m_registry.has<T>(thing);
}

template <typename T, typename... Args>
inline T *World::try_emplace_now(Thing thing, Args &&...args) noexcept {
    if constexpr (IsScript<T>) {
        T script(std::forward<Args>(args)...);
        script.self = thing;
        script.scope = Scope(this);

        if constexpr (ScriptHasOnInit<T>) {
            script.on_init();
        }

        do_register_script_type<T>();

        return m_registry.emplace_checked<T>(thing, script);
    } else {
        return m_registry.emplace_checked<T>(thing, std::forward<Args>(args)...);
    }
}

template <typename T, typename... Args>
inline T &World::emplace_now(Thing thing, Args &&...args) noexcept {
    if constexpr (IsScript<T>) {
        T script(std::forward<Args>(args)...);
        script.self = thing;
        script.scope = Scope(this);

        if constexpr (ScriptHasOnInit<T>) {
            script.on_init();
        }

        do_register_script_type<T>();

        return m_registry.emplace_unchecked<T>(thing, script);
    } else {
        return m_registry.emplace_unchecked<T>(thing, std::forward<Args>(args)...);
    }
}

template <typename T>
inline void World::insert_now(Thing thing, const T &part) noexcept {
    if constexpr (IsScript<T>) {
        T script = part;
        script.self = thing;
        script.scope = Scope(this);

        if constexpr (ScriptHasOnInit<T>) {
            script.on_init();
        }

        do_register_script_type<T>();

        m_registry.emplace_checked<T>(thing, script);
    } else {
        m_registry.emplace_checked<T>(thing, part);
    }
}

template <typename T>
inline void World::insert_now(Thing thing, T &&part) noexcept {
    if constexpr (IsScript<T>) {
        T script = std::move(part);
        script.self = thing;
        script.scope = Scope(this);
        if constexpr (ScriptHasOnInit<T>) {
            script.on_init();
        }
        do_register_script_type<T>();
        m_registry.emplace_checked<T>(thing, script);
    } else {
        m_registry.emplace_checked<T>(thing, std::forward<T>(part));
    }
}

template <typename T>
inline bool World::try_destroy_now(Thing thing) noexcept {
    if constexpr (IsScript<T>) {
        if (!m_registry.has<T>(thing)) {
            return false;
        }
        if constexpr (ScriptHasOnDestroy<T>) {
            m_registry.get_unchecked<T>(thing).on_destroy();
        }
        m_registry.destroy_unchecked<T>(thing);
        return true;
    } else {
        return m_registry.destroy_checked<T>(thing);
    }
}

template <typename T>
inline void World::destroy_now(Thing thing) noexcept {
    if constexpr (IsScript<T>) {
        FR_ASSERT(m_registry.has<T>(thing), "destroy_now: thing does not have the script");
        if constexpr (ScriptHasOnDestroy<T>) {
            m_registry.get_unchecked<T>(thing).on_destroy();
        }
        m_registry.destroy_unchecked<T>(thing);
    } else {
        [[maybe_unused]] bool ok = m_registry.destroy_checked<T>(thing);
        FR_ASSERT(ok, "destroy_now: thing is nil, dead, or does not have the part");
    }
}

template <typename T>
inline T *World::try_get(Thing thing) noexcept {
    return m_registry.get_checked<T>(thing);
}

template <typename T>
inline T &World::get(Thing thing) noexcept {
    return m_registry.get_unchecked<T>(thing);
}

template <typename T>
inline void World::sort_by_hierarchy_depth() noexcept {
    impl::PartPool<T> *pool = m_registry.try_part_pool_mut<T>();
    if (!pool) {
        return;
    }

    Slice<T> parts = pool->parts_mut();
    Slice<Thing> part_to_thing = pool->part_to_thing_mut();
    USize n = parts.size();

    if (n == 0) {
        return;
    }

    // Build depth key for each part's owning thing (0 if no Relations part).
    DynamicArray<HierarchyDepth> depths;
    depths.reserve(n);
    for (USize i = 0; i < n; ++i) {
        Relations *rel = m_registry.get_checked<Relations>(part_to_thing[i]);
        depths.push_back(rel ? rel->depth : ROOT_HIERARCHY_DEPTH);
    }

    // Argsort: perm[i] = old index of the part that ends up at position i.
    auto perm = DynamicArray<USize>::from_repeated(n, 0);
    radix_argsort(depths.slice_mut(), perm.slice_mut());

    // Reorder both dense arrays by the same permutation.
    apply_permutation(parts, perm.slice_mut());
    apply_permutation(part_to_thing, perm.slice_mut());

    // Rebuild thing -> part reverse mapping (dense index i == pool index i+1, stub at 0).
    Slice<USize> t2p = pool->thing_to_part_with_stub_mut();
    for (USize i = 0; i < n; ++i) {
        t2p[part_to_thing[i].idx()] = i + 1;
    }
}

inline void World::commit_insert_from(CmdBatch &batch, bool invert) noexcept {
    auto cmds = batch.cmds();

    if (invert) {
        for (USize i = cmds.size(); i-- > 0;) {
            const Cmd &cmd = cmds[i];
            if (cmd.kind != CmdKind::Insert) {
                continue;
            }

            Cmd inverse = cmd.inverse();
            const InsertCmdData &c = inverse.insert_part;
            m_registry.destroy_raw(c.tidx, c.thing);
        }

        return;
    }

    Byte *base = batch.arena();
    for (const Cmd &cmd : cmds) {
        if (cmd.kind != CmdKind::Insert) {
            continue;
        }

        const InsertCmdData &c = cmd.insert_part;
        m_registry.insert_raw(c.tidx, c.thing, base + c.offset);
    }
}

inline void World::commit_destroy_from(CmdBatch &batch, bool invert) noexcept {
    auto cmds = batch.cmds();
    if (invert) {
        Byte *base = batch.arena();
        for (USize i = cmds.size(); i-- > 0;) {
            const Cmd &cmd = cmds[i];
            if (cmd.kind != CmdKind::Destroy) {
                continue;
            }

            Cmd inverse = cmd.inverse();
            const InsertCmdData &c = inverse.insert_part;
            m_registry.insert_raw(c.tidx, c.thing, base + c.offset);
        }

        return;
    }

    for (const Cmd &cmd : cmds) {
        if (cmd.kind != CmdKind::Destroy) {
            continue;
        }

        const DestroyCmdData &c = cmd.destroy_part;
        m_registry.destroy_raw(c.tidx, c.thing);
    }
}

inline void World::commit_mutate_from(CmdBatch &batch, bool invert) noexcept {
    auto cmds = batch.cmds();
    Byte *base = batch.arena();
    if (invert) {
        for (USize i = cmds.size(); i-- > 0;) {
            const Cmd &cmd = cmds[i];
            if (cmd.kind != CmdKind::Mutate) {
                continue;
            }

            const MutateCmdData &c = cmd.mutate_part.inverse();
            if (m_registry.part_meta().has(c.tidx)) {
                m_registry.part_meta().get(c.tidx).commit_mutate(static_cast<void *>(&m_registry),
                                                                 c.thing, base + c.next_offset);
            }
        }
        return;
    }

    for (const Cmd &cmd : cmds) {
        if (cmd.kind != CmdKind::Mutate) {
            continue;
        }

        const MutateCmdData &c = cmd.mutate_part;
        if (m_registry.part_meta().has(c.tidx)) {
            m_registry.part_meta().get(c.tidx).commit_mutate(static_cast<void *>(&m_registry),
                                                             c.thing, base + c.next_offset);
        }
    }
}

inline void World::commit_attach_child_from(CmdBatch &batch, bool invert) noexcept {
    auto cmds = batch.cmds();
    if (invert) {
        for (USize i = cmds.size(); i-- > 0;) {
            const Cmd &cmd = cmds[i];
            if (cmd.kind != CmdKind::AttachChild) {
                continue;
            }

            Cmd inverse = cmd.inverse();
            detach_child_now(inverse.attach_child.parent, inverse.attach_child.child);
        }

        return;
    }

    for (const Cmd &cmd : cmds) {
        if (cmd.kind != CmdKind::AttachChild) {
            continue;
        }

        attach_child_now(cmd.attach_child.parent, cmd.attach_child.child);
    }
}

inline void World::commit_detach_child_from(CmdBatch &batch, bool invert) noexcept {
    auto cmds = batch.cmds();
    if (invert) {
        for (USize i = cmds.size(); i-- > 0;) {
            const Cmd &cmd = cmds[i];
            if (cmd.kind != CmdKind::DetachChild) {
                continue;
            }

            Cmd inverse = cmd.inverse();
            detach_child_now(inverse.detach_child.parent, inverse.detach_child.child);
        }

        return;
    }

    for (const Cmd &cmd : cmds) {
        if (cmd.kind != CmdKind::DetachChild) {
            continue;
        }

        detach_child_now(cmd.detach_child.parent, cmd.detach_child.child);
    }
}

inline void World::commit_spawn_from(CmdBatch & /*batch*/, bool /* invert */) noexcept {
    // No-op: things handed out via `handout_deferred()` are already alive.
    // This method exists for symmetry and potential future hooks.
}

inline void World::commit_kill_from(CmdBatch &batch, bool invert) noexcept {
    auto cmds = batch.cmds();
    if (invert) {
        for (USize i = cmds.size(); i-- > 0;) {
            const Cmd &cmd = cmds[i];
            if (cmd.kind != CmdKind::Kill) {
                continue;
            }

            Cmd inverse = cmd.inverse();
            m_registry.spawn();
            (void)inverse;
        }

        return;
    }

    for (const Cmd &cmd : cmds) {
        if (cmd.kind != CmdKind::Kill) {
            continue;
        }

        m_registry.kill(cmd.kill.thing);
    }
}

inline void World::commit_from(CmdBatch &batch) noexcept {
    commit_mutate_from(batch);
    commit_destroy_from(batch);
    commit_insert_from(batch);
    commit_attach_child_from(batch);
    commit_detach_child_from(batch);
    commit_kill_from(batch);
    batch.reset();
}

inline void World::commit_inverse_from(CmdBatch &batch) noexcept {
    commit_kill_from(batch, true);
    commit_detach_child_from(batch, true);
    commit_attach_child_from(batch, true);
    commit_insert_from(batch, true);
    commit_destroy_from(batch, true);
    commit_mutate_from(batch, true);
    batch.reset();
}

template <typename T>
inline void World::insert(Thing thing, const T &part) noexcept {
    if constexpr (IsScript<T>) {
        T script = part;
        script.self = thing;
        script.scope = Scope(this);

        if constexpr (ScriptHasOnInit<T>) {
            script.on_init();
        }

        do_register_script_type<T>();

        m_registry.ensure<T>();
        m_cmd_batch.record_insert<T>(thing, script);
    } else {
        m_registry.ensure<T>();
        m_cmd_batch.record_insert<T>(thing, part);
    }
}

template <typename T>
inline void World::insert(Thing thing, T &&part) noexcept {
    if constexpr (IsScript<T>) {
        T script = std::move(part);
        script.self = thing;
        script.scope = Scope(this);

        if constexpr (ScriptHasOnInit<T>) {
            script.on_init();
        }

        do_register_script_type<T>();

        m_registry.ensure<T>();
        m_cmd_batch.record_insert<T>(thing, script);
    } else {
        m_registry.ensure<T>();
        m_cmd_batch.record_insert<T>(thing, std::forward<T>(part));
    }
}

template <typename T>
inline void World::destroy(Thing thing) noexcept {
    T *current = m_registry.get_checked<T>(thing);
    if (!current) [[unlikely]] {
        return;
    }

    if constexpr (IsScript<T>) {
        if constexpr (ScriptHasOnDestroy<T>) {
            if (!thing.is_nil()) {
                current->on_destroy();
            }
        }
    }

    m_cmd_batch.record_destroy<T>(thing, *current);
}

// -------------------------------------------------- Raw Part Operations

inline bool World::has_raw(TypeIdx tidx, Thing thing) const noexcept {
    return m_registry.has_raw(tidx, thing);
}

inline void *World::try_emplace_now_raw(TypeIdx tidx, Thing thing, void *part) noexcept {
    m_registry.insert_raw(tidx, thing, part);
    return m_registry.get_raw(tidx, thing);
}

inline void *World::try_emplace_now_raw(TypeIdx tidx, Thing thing, const void *part) noexcept {
    m_registry.insert_raw(tidx, thing, part);
    return m_registry.get_raw(tidx, thing);
}

inline void *World::emplace_now_raw(TypeIdx tidx, Thing thing, void *part) noexcept {
    m_registry.emplace_unchecked_raw(tidx, thing, part);
    return m_registry.get_unchecked_raw(tidx, thing);
}

inline void *World::emplace_now_raw(TypeIdx tidx, Thing thing, const void *part) noexcept {
    m_registry.emplace_unchecked_raw(tidx, thing, part);
    return m_registry.get_unchecked_raw(tidx, thing);
}

inline void World::insert_now_raw(TypeIdx tidx, Thing thing, void *part) noexcept {
    m_registry.insert_raw(tidx, thing, part);
}

inline void World::insert_now_raw(TypeIdx tidx, Thing thing, const void *part) noexcept {
    m_registry.insert_raw(tidx, thing, part);
}

inline bool World::destroy_now_raw(TypeIdx tidx, Thing thing) noexcept {
    bool had = m_registry.has_raw(tidx, thing);
    m_registry.destroy_raw(tidx, thing);
    return had;
}

inline void World::destroy_now_unchecked_raw(TypeIdx tidx, Thing thing) noexcept {
    m_registry.destroy_unchecked_raw(tidx, thing);
}

inline void *World::try_get_raw(TypeIdx tidx, Thing thing) noexcept {
    return m_registry.get_raw(tidx, thing);
}

inline void *World::get_raw(TypeIdx tidx, Thing thing) noexcept {
    return m_registry.get_unchecked_raw(tidx, thing);
}

// ------------------------------------------ Raw Deferred Part Operations

inline void World::insert_raw(TypeIdx tidx, Thing thing, const void *part) noexcept {
    m_cmd_batch.record_insert_raw(thing, tidx, part);
}

inline void World::insert_raw(TypeIdx tidx, Thing thing, void *part) noexcept {
    m_cmd_batch.record_insert_raw(thing, tidx, part);
}

inline void World::destroy_raw(TypeIdx tidx, Thing thing, const void *current) noexcept {
    m_cmd_batch.record_destroy_raw(thing, tidx, current);
}

inline void World::mutate_raw(TypeIdx tidx, Thing thing, const void *prev,
                              const void *next) noexcept {
    m_cmd_batch.record_mutate_raw(thing, tidx, prev, next);
}

inline bool World::attach_child_now(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return false;
    }

    Relations &parent_rel = m_registry.get_unchecked<Relations>(parent);
    Relations &child_rel = m_registry.get_unchecked<Relations>(child);

    if (child_rel.parent == parent) {
        return true;
    }

    if (!child_rel.parent.is_nil()) {
        do_detach_from_hierarchy_unchecked(child);
    }

    child_rel.parent = parent;
    child_rel.next_sibling = parent_rel.first_child;

    if (!child_rel.next_sibling.is_nil()) {
        Relations &next_rel = m_registry.get_unchecked<Relations>(child_rel.next_sibling);
        next_rel.prev_sibling = child;
    }

    parent_rel.first_child = child;
    do_update_hierarchy(parent_rel.depth, child);
    return true;
}

inline void World::attach_child(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return;
    }
    m_cmd_batch.record_attach_child(parent, child);
}

inline bool World::detach_child_now(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return false;
    }

    Relations &child_rel = m_registry.get_unchecked<Relations>(child);
    if (child_rel.parent != parent) {
        return false;
    }

    do_detach_from_hierarchy_unchecked(child);
    return true;
}

inline void World::detach_child(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return;
    }

    m_cmd_batch.record_detach_child(parent, child);
}

inline void World::update_hierarchy(Thing root) noexcept {
    if (root.is_nil()) [[unlikely]] {
        return;
    }

    Relations &root_rel = m_registry.get_unchecked<Relations>(root);
    Thing curr = root_rel.first_child;

    while (!curr.is_nil()) {
        Relations &rel = m_registry.get_unchecked<Relations>(curr);
        do_update_hierarchy(root_rel.depth, curr);
        curr = rel.next_sibling;
    }
}

inline void World::do_update_hierarchy(HierarchyDepth parent_depth, Thing child) noexcept {
    Thing curr_thing = child;
    HierarchyDepth curr_depth = parent_depth + 1;

    while (curr_depth > parent_depth) {
        Relations &rel = m_registry.get_unchecked<Relations>(curr_thing);
        if (rel.depth == curr_depth) {
            curr_thing = rel.parent;
            --curr_depth;
            continue;
        }

        rel.depth = curr_depth;

        if (!rel.first_child.is_nil()) {
            curr_thing = rel.first_child;
            ++curr_depth;
            continue;
        }

        if (!rel.next_sibling.is_nil()) {
            curr_thing = rel.next_sibling;
            continue;
        }

        FR_ASSERT(!rel.parent.is_nil(), "malformed hierarchy");
        FR_ASSERT(curr_depth > 0, "malformed hierarchy");
        curr_thing = rel.parent;
        --curr_depth;
    }
}

inline void World::do_detach_from_hierarchy_unchecked(Thing thing) noexcept {
    Relations &thing_rel = m_registry.get_unchecked<Relations>(thing);

    if (thing_rel.parent.is_nil()) {
        return;
    }

    Relations &parent_rel = m_registry.get_unchecked<Relations>(thing_rel.parent);
    if (thing == parent_rel.first_child) {
        FR_ASSERT(thing_rel.prev_sibling.is_nil(), "previous sibling of a first child must be nil");

        parent_rel.first_child = thing_rel.next_sibling;
        if (!parent_rel.first_child.is_nil()) {
            Relations &next_rel = m_registry.get_unchecked<Relations>(thing_rel.next_sibling);
            next_rel.prev_sibling = Thing::nil();
            thing_rel.next_sibling = Thing::nil();
        }
        thing_rel.parent = Thing::nil();

        return;
    }

    if (thing_rel.next_sibling.is_nil()) {
        Relations &prev_rel = m_registry.get_unchecked<Relations>(thing_rel.prev_sibling);
        prev_rel.next_sibling = Thing::nil();
        thing_rel.prev_sibling = Thing::nil();
        thing_rel.parent = Thing::nil();
        return;
    }

    FR_ASSERT(!thing_rel.next_sibling.is_nil(),
              "in the final case the next sibling must be non-nil");
    FR_ASSERT(!thing_rel.prev_sibling.is_nil(),
              "in the final case the previous sibling must be non-nil");

    Relations &prev_rel = m_registry.get_unchecked<Relations>(thing_rel.prev_sibling);
    Relations &next_rel = m_registry.get_unchecked<Relations>(thing_rel.next_sibling);

    prev_rel.next_sibling = thing_rel.next_sibling;
    next_rel.prev_sibling = thing_rel.prev_sibling;
    thing_rel.prev_sibling = Thing::nil();
    thing_rel.next_sibling = Thing::nil();
    thing_rel.parent = Thing::nil();
}

template <typename... Include>
inline auto World::query(QueryOptions options) noexcept {
    return Query<Include...>(&m_registry, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto World::reverse_query(QueryOptions options) noexcept {
    return ReverseQuery<Include...>(&m_registry, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto World::shallow_query(Thing thing, QueryOptions options) noexcept {
    return ShallowQuery<Include...>(&m_registry, thing, Signature::from_parts<Include...>(),
                                    options);
}

template <typename... Include>
inline auto World::deep_query(Thing thing, QueryOptions options) noexcept {
    return DeepQuery<Include...>(&m_registry, thing, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto World::top_down_query(QueryOptions options) noexcept {
    return TopDownQuery<Include...>(&m_registry, Signature::from_parts<Include...>(), options);
}

template <typename... Include>
inline auto World::bottom_up_query(QueryOptions options) noexcept {
    return BottomUpQuery<Include...>(&m_registry, Signature::from_parts<Include...>(), options);
}

inline void World::schedule(Stage stage, const System &system) noexcept {
    m_system_pool.schedule(stage, system);
}

inline void World::run_stage(Stage stage) noexcept {
    Scope scope{this};
    m_system_pool.run_stage(stage, scope);
}

inline void World::shape(JsonWriterArchive &archive) noexcept {
    archive.prop("registry", m_registry);
    archive.prop("resources", m_resource_pool);
}

inline void World::shape(JsonReaderArchive &archive) noexcept {
    archive.prop("registry", m_registry);
    archive.prop("resources", m_resource_pool);
}

inline void World::run() noexcept {
    run_stage(Stage::PreUpdate);
    run_stage(Stage::PreUpdateScript);
    run_stage(Stage::Update);
    run_stage(Stage::UpdateScript);
    run_stage(Stage::PostUpdate);
    run_stage(Stage::PostUpdateScript);
}

template <IsScript S>
inline void World::do_register_script_type() noexcept {
    TypeIdx tidx = TypeIdx::from_type<S>();
    if (m_script_registry.check_bit(tidx.idx())) {
        return;
    }

    if constexpr (ScriptHasOnPreUpdate<S>) {
        schedule(Stage::PreUpdateScript, [](Scope scope) {
            for (auto [t, s] : scope.query<S>()) {
                s.on_pre_update();
            }
        });
    }

    if constexpr (ScriptHasOnUpdate<S>) {
        schedule(Stage::UpdateScript, [](Scope scope) {
            for (auto [t, s] : scope.query<S>()) {
                s.on_update();
            }
        });
    }

    if constexpr (ScriptHasOnPostUpdate<S>) {
        schedule(Stage::PostUpdateScript, [](Scope scope) {
            for (auto [t, s] : scope.query<S>()) {
                s.on_post_update();
            }
        });
    }

    m_script_registry.one_bit(tidx.idx());
}

// ================================================ Scope Method Implementations

inline Scope::Scope() noexcept
    : m_world(nullptr) {
}

inline Scope::Scope(World *world) noexcept
    : m_world(world) {
}

inline Thing Scope::spawn() noexcept {
    return m_world->spawn();
}

inline Thing Scope::spawn_deferred() noexcept {
    return m_world->spawn_deferred();
}

inline void Scope::kill(Thing thing) noexcept {
    m_world->kill(thing);
}

inline void Scope::kill_deferred(Thing thing) noexcept {
    m_world->kill_deferred(thing);
}

inline bool Scope::is_alive(Thing thing) const noexcept {
    return m_world->is_alive(thing);
}

inline bool Scope::is_dead(Thing thing) const noexcept {
    return m_world->is_dead(thing);
}

template <typename T>
inline bool Scope::has(Thing thing) const noexcept {
    return m_world->has<T>(thing);
}

template <typename T, typename... Args>
inline T *Scope::try_emplace_now(Thing thing, Args &&...args) noexcept {
    return m_world->try_emplace_now<T>(thing, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline T &Scope::emplace_now(Thing thing, Args &&...args) noexcept {
    return m_world->emplace_now<T>(thing, std::forward<Args>(args)...);
}

template <typename T>
inline void Scope::insert_now(Thing thing, const T &part) noexcept {
    m_world->insert_now(thing, part);
}

template <typename T>
inline void Scope::insert_now(Thing thing, T &&part) noexcept {
    m_world->insert_now(thing, std::forward<T>(part));
}

template <typename T>
inline bool Scope::try_destroy_now(Thing thing) noexcept {
    return m_world->try_destroy_now<T>(thing);
}

template <typename T>
inline void Scope::destroy_now(Thing thing) noexcept {
    m_world->destroy_now<T>(thing);
}

template <typename T>
inline T *Scope::try_get(Thing thing) noexcept {
    return m_world->try_get<T>(thing);
}

template <typename T>
inline T &Scope::get(Thing thing) noexcept {
    return m_world->get<T>(thing);
}

inline bool Scope::attach_child_now(Thing parent, Thing child) noexcept {
    return m_world->attach_child_now(parent, child);
}

inline void Scope::attach_child(Thing parent, Thing child) noexcept {
    m_world->attach_child(parent, child);
}

inline bool Scope::detach_child_now(Thing parent, Thing child) noexcept {
    return m_world->detach_child_now(parent, child);
}

inline void Scope::detach_child(Thing parent, Thing child) noexcept {
    m_world->detach_child(parent, child);
}

template <typename T>
inline void Scope::insert(Thing thing, const T &part) noexcept {
    m_world->insert(thing, part);
}

template <typename T>
inline void Scope::insert(Thing thing, T &&part) noexcept {
    m_world->insert(thing, std::move(part));
}

template <typename T>
inline void Scope::destroy(Thing thing) noexcept {
    m_world->destroy<T>(thing);
}

template <typename... Include>
inline auto Scope::query(QueryOptions options) noexcept {
    return m_world->query<Include...>(options);
}

template <typename... Include>
inline auto Scope::reverse_query(QueryOptions options) noexcept {
    return m_world->reverse_query<Include...>(options);
}

template <typename... Include>
inline auto Scope::shallow_query(Thing thing, QueryOptions options) noexcept {
    return m_world->shallow_query<Include...>(thing, options);
}

template <typename... Include>
inline auto Scope::deep_query(Thing thing, QueryOptions options) noexcept {
    return m_world->deep_query<Include...>(thing, options);
}

template <typename... Include>
inline auto Scope::top_down_query(QueryOptions options) noexcept {
    return m_world->top_down_query<Include...>(options);
}

template <typename... Include>
inline auto Scope::bottom_up_query(QueryOptions options) noexcept {
    return m_world->bottom_up_query<Include...>(options);
}

template <typename T>
inline T *Scope::try_get_resource() noexcept {
    return m_world->try_get_resource<T>();
}

template <typename T>
inline T &Scope::get_resource() noexcept {
    return m_world->get_resource<T>();
}

template <typename T>
inline bool Scope::check_resource() noexcept {
    return m_world->check_resource<T>();
}

inline Thing Script::spawn() noexcept {
    return scope.spawn();
}

inline void Script::kill(Thing thing) noexcept {
    scope.kill(thing);
}

inline bool Script::attach_child_now(Thing child) noexcept {
    return scope.attach_child_now(self, child);
}

inline bool Script::detach_child_now(Thing child) noexcept {
    return scope.detach_child_now(self, child);
}

template <typename T>
inline bool Script::has() const noexcept {
    return scope.has<T>(self);
}

template <typename T>
inline T &Script::get() noexcept {
    return scope.get<T>(self);
}

template <typename T, typename... Args>
inline T *Script::try_emplace_now(Args &&...args) noexcept {
    return scope.try_emplace_now<T>(self, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline T &Script::emplace_now(Args &&...args) noexcept {
    return scope.emplace_now<T>(self, std::forward<Args>(args)...);
}

template <typename T>
inline void Script::insert_now(const T &part) noexcept {
    scope.insert_now(self, part);
}

template <typename T>
inline void Script::insert_now(T &&part) noexcept {
    scope.insert_now(self, std::move(part));
}

template <typename T>
inline bool Script::try_destroy_now() noexcept {
    return scope.try_destroy_now<T>(self);
}

template <typename T>
inline void Script::destroy_now() noexcept {
    scope.destroy_now<T>(self);
}

template <typename T>
inline void Script::insert(const T &part) noexcept {
    scope.insert(self, part);
}

template <typename T>
inline void Script::insert(T &&part) noexcept {
    scope.insert(self, std::move(part));
}

template <typename T>
inline void Script::destroy() noexcept {
    scope.destroy<T>(self);
}
} // namespace fr
