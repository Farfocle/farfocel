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
    Thing handout() noexcept;

    /// @brief Hands out a new thing immediately and records it in the batch for tracking.
    Thing handout_deferred() noexcept;

    /**
     * @brief Kills a thing and destroys all of its parts immediately.
     * @note If the thing is nil or dead; does nothing.
     */
    void kill(Thing thing) noexcept;

    /// @brief Records a deferred kill — applied during the next `commit()`.
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
    bool destroy_now(Thing thing) noexcept;

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
     * @brief Attaches a child to the parent. Updates hierarchy.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
     */
    void attach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a `AttachChild` command.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
     */
    void attach_child(Thing parent, Thing child) noexcept;

    /**
     * @brief Detaches a child from a parent. Updates hierarchy.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `child` is not a real child of the `parent`; does nothing.
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
     */
    void detach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a `DetachChild` command.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `child` is not a real child of the `parent`; does nothing.
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
     */
    void detach_child(Thing parent, Thing child) noexcept;

    // ------------------------------------------------- Command Part Operations

    /**
     * @brief Records a deferred insert command for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept;

    /**
     * @brief Records a deferred insert command for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
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

    // ----------------------------------------------------------------- Scripts

    /// @brief Attaches a script to a thing.
    template <typename S>
    void insert_script(Thing thing, S script) noexcept;

    /**
     * @brief Removes a script from a thing..
     * @note If the thing is dead; does nothing.
     * @note If the thing is nil; does nothing.
     * @note If the thing does have a script `S`; does nothing.
     */
    template <typename S>
    void destroy_script(Thing thing) noexcept;

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

    // --------------------------------------------------------------------- API

    /// @brief Sets the thing this script is attached to.
    void set_self(Thing thing) noexcept;

    /// @brief Sets the scope this script operates in.
    void set_scope(Scope scope) noexcept;

    /// @brief Returns the thing this script is attached to.
    [[nodiscard]] Thing self() const noexcept;

    /// @brief Returns the scope this script operates in.
    [[nodiscard]] Scope &scope() noexcept;

    /// @brief Checks if self has part `T`.
    template <typename T>
    bool has() const noexcept;

    /**
     * @brief Returns a reference to part `T` attached to self.
     * @warning Asserts if self does not have part `T`.
     */
    template <typename T>
    T &get() noexcept;

    /// @brief Records an insert command for part `T` on self.
    template <typename T>
    void insert(const T &part) noexcept;

    /// @brief Records an insert command for part `T` on self.
    template <typename T>
    void insert(T &&part) noexcept;

    /// @brief Records a destroy command for part `T` on self.
    template <typename T>
    void destroy() noexcept;

private:
    // -------------------------------------------------------- Member Variables
    Scope m_scope{};
    Thing m_self{Thing::nil()};
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
    Thing handout() noexcept;

    /**
     * @brief Hands out a new thing immediately, then records it in the batch for tracking.
     * @return The newly created thing (already alive).
     * @note The thing is created now so the handle is usable right away. The batch entry
     * enables undo (its inverse is a deferred kill) and batch introspection.
     */
    Thing handout_deferred() noexcept;

    /**
     * @brief Kills a thing and destroys all of its parts immediately.
     * @note If the thing is nil or dead; does nothing.
     */
    void kill(Thing thing) noexcept;

    /**
     * @brief Records a deferred kill — the thing is killed during the next `commit()`.
     * @note Safe to call while iterating over part pools; the actual kill is postponed.
     * @note If thing is nil; does nothing.
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
    bool destroy_now(Thing thing) noexcept;

    /// @brief Returns a pointer to part `T` owned by the thing, or nullptr if not found.
    template <typename T>
    T *try_get(Thing thing) noexcept;

    /**
     * @brief Returns a reference to part `T` owned by the thing.
     * @pre Caller must ensure: thing is alive and has `T`.
     */
    template <typename T>
    T &get(Thing thing) noexcept;

    /**
     * @brief Sorts the entire part pool `T` using the hierarchy depth.
     * @note This allows for BFS-like traversals with a normal forward and reverse queries for
     * sorted pools.
     */
    template <typename T>
    void sort_by_hierarchy_depth() noexcept;

    // --------------------------------------------------------------- Relations

    /**
     * @brief Attaches a child to the parent. Updates hierarchy.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
     */
    void attach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a `AttachChild` command.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
     */
    void attach_child(Thing parent, Thing child) noexcept;

    /**
     * @brief Detaches a child from a parent. Updates hierarchy.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `child` is not a real child of the `parent`; does nothing.
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
     */
    void detach_child_now(Thing parent, Thing child) noexcept;

    /**
     * @brief Emits a `DetachChild` command.
     * @param parent The parent thing.
     * @pre `parent` must have `Relations` part.
     * @param child The child thing.
     * @pre `child` must have `Relations` part.
     *
     * @note If `child` is not a real child of the `parent`; does nothing.
     * @note If `parent` is nil; does nothing.
     * @note If `child` is nil; does nothing.
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

    /// @brief Apply all recorded insert commands across all part pools.
    void commit_insert() noexcept;

    /// @brief Apply all recorded mutate commands across all part pools.
    void commit_mutate() noexcept;

    /// @brief Apply all recorded destroy commands across all part pools.
    void commit_destroy() noexcept;

    /// @brief Apply all recorded attach-child commands.
    void commit_attach_child() noexcept;

    /// @brief Apply all recorded detach-child commands.
    void commit_detach_child() noexcept;

    /// @brief No-op (things are already alive). Exists for symmetry and future use.
    void commit_handout() noexcept;

    /// @brief Kill all things recorded via `kill_deferred()`.
    void commit_kill() noexcept;

    /// @brief Apply all commands: mutate -> destroy -> insert -> attach -> detach -> kill.
    void commit() noexcept;

    /**
     * @brief Records a deferred insert command for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept;

    /**
     * @brief Records a deferred insert command for part `T` on a thing.
     * @note Does nothing if thing is nil, dead, or already has part `T`.
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

    // ------------------------------------------------------------------- Shape

    /// @brief Ensures the part pool for the given type exists so it can be deserialized.
    template <typename T>
    void ensure() noexcept {
        m_registry.ensure<T>();
    }

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

    // ----------------------------------------------------------------- Scripts

    /// @brief Attaches a script to a thing.
    template <IsScript S>
    void insert_script(Thing thing, S script) noexcept;

    /**
     * @brief Removes a script from a thing..
     * @note If the thing is dead; does nothing.
     * @note If the thing is nil; does nothing.
     * @note If the thing does have a script `S`; does nothing.
     */
    template <IsScript S>
    void destroy_script(Thing thing) noexcept;

private:
    // --------------------------------------------------------------- Internals
    void do_update_hierarchy(HierarchyDepth parent_depth, Thing child) noexcept;
    void do_detach_from_hierarchy_unchecked(Thing thing) noexcept;

    // ----------------------------------------------------------------- Members
    Options m_options{};
    impl::Registry m_registry;
    impl::SystemPool m_system_pool{};
    Bitset<MAX_PARTS> m_script_registry{};
    CmdBatch m_cmd_batch;
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
      m_cmd_batch(opt.registry_alloc, opt.cmd_batch_arena_size) {
}

inline Thing World::handout() noexcept {
    return m_registry.handout();
}

inline Thing World::handout_deferred() noexcept {
    Thing thing = m_registry.handout();
    m_cmd_batch.record_handout(thing);
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
    return m_registry.emplace_checked<T>(thing, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline T &World::emplace_now(Thing thing, Args &&...args) noexcept {
    return m_registry.emplace_unchecked<T>(thing, std::forward<Args>(args)...);
}

template <typename T>
inline void World::insert_now(Thing thing, const T &part) noexcept {
    m_registry.emplace_checked<T>(thing, part);
}

template <typename T>
inline void World::insert_now(Thing thing, T &&part) noexcept {
    m_registry.emplace_checked<T>(thing, std::forward<T>(part));
}

template <typename T>
inline bool World::destroy_now(Thing thing) noexcept {
    return m_registry.destroy_checked<T>(thing);
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

inline void World::commit_insert() noexcept {
    Byte *base = m_cmd_batch.arena();
    for (const Cmd &cmd : m_cmd_batch.cmds()) {
        if (cmd.kind != CmdKind::InsertPart) {
            continue;
        }

        const InsertCmd &c = cmd.insert_part;
        m_registry.insert_raw(c.tidx, c.thing, base + c.offset);
    }
}

inline void World::commit_mutate() noexcept {
    Byte *base = m_cmd_batch.arena();
    for (const Cmd &cmd : m_cmd_batch.cmds()) {
        if (cmd.kind != CmdKind::MutatePart) {
            continue;
        }

        const MutateCmd &c = cmd.mutate_part;
        if (m_registry.part_meta().has(c.tidx)) {
            m_registry.part_meta().get(c.tidx).commit_mutate(static_cast<void *>(&m_registry),
                                                             c.thing, base + c.next_offset);
        }
    }
}

inline void World::commit_destroy() noexcept {
    for (const Cmd &cmd : m_cmd_batch.cmds()) {
        if (cmd.kind != CmdKind::DestroyPart) {
            continue;
        }

        const DestroyCmd &c = cmd.destroy_part;
        m_registry.destroy_raw(c.tidx, c.thing);
    }
}

inline void World::commit_attach_child() noexcept {
    for (const Cmd &cmd : m_cmd_batch.cmds()) {
        if (cmd.kind != CmdKind::AttachChild) {
            continue;
        }

        attach_child_now(cmd.attach_child.parent, cmd.attach_child.child);
    }
}

inline void World::commit_detach_child() noexcept {
    for (const Cmd &cmd : m_cmd_batch.cmds()) {
        if (cmd.kind != CmdKind::DetachChild) {
            continue;
        }

        detach_child_now(cmd.detach_child.parent, cmd.detach_child.child);
    }
}

inline void World::commit_handout() noexcept {
    // No-op: things handed out via `handout_deferred()` are already alive.
    // This method exists for symmetry and potential future hooks.
}

inline void World::commit_kill() noexcept {
    for (const Cmd &cmd : m_cmd_batch.cmds()) {
        if (cmd.kind != CmdKind::Kill) {
            continue;
        }

        m_registry.kill(cmd.kill.thing);
    }
}

inline void World::commit() noexcept {
    commit_mutate();
    commit_destroy();
    commit_insert();
    commit_attach_child();
    commit_detach_child();
    commit_kill();
    m_cmd_batch.reset();
}

template <typename T>
inline void World::insert(Thing thing, const T &part) noexcept {
    m_registry.ensure<T>(); // lazily register pool + PartMeta on first use
    m_cmd_batch.record_insert<T>(thing, part);
}

template <typename T>
inline void World::insert(Thing thing, T &&part) noexcept {
    m_registry.ensure<T>();
    m_cmd_batch.record_insert<T>(thing, std::forward<T>(part));
}

template <typename T>
inline void World::destroy(Thing thing) noexcept {
    T *current = m_registry.get_checked<T>(thing);
    if (current) [[likely]] {
        m_cmd_batch.record_destroy<T>(thing, *current);
    }
}

inline void World::attach_child_now(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return;
    }

    Relations &parent_rel = m_registry.get_unchecked<Relations>(parent);
    Relations &child_rel = m_registry.get_unchecked<Relations>(child);

    if (child_rel.parent == parent) {
        return;
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
}

inline void World::attach_child(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return;
    }
    m_cmd_batch.record_attach_child(parent, child);
}

inline void World::detach_child_now(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return;
    }

    Relations &child_rel = m_registry.get_unchecked<Relations>(child);
    if (child_rel.parent != parent) {
        return;
    }

    do_detach_from_hierarchy_unchecked(child);
}

inline void World::detach_child(Thing parent, Thing child) noexcept {
    if (parent.is_nil() || child.is_nil()) {
        return;
    }

    m_cmd_batch.record_detach_child(parent, child);
}

inline void World::update_hierarchy(Thing root) noexcept {
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

inline void World::run() noexcept {
    run_stage(Stage::PreUpdate);
    run_stage(Stage::PreUpdateScript);
    run_stage(Stage::Update);
    run_stage(Stage::UpdateScript);
    run_stage(Stage::PostUpdate);
    run_stage(Stage::PostUpdateScript);
}

template <IsScript S>
inline void World::insert_script(Thing thing, S script) noexcept {
    TypeIdx tidx = TypeIdx::from_type<S>();
    script.set_self(thing);
    script.set_scope(this);

    if constexpr (ScriptHasOnInit<S>) {
        script.on_init();
    }

    if (!m_script_registry.check_bit(tidx.idx())) {
        if constexpr (ScriptHasOnPostUpdate<S>) {
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

    insert<S>(thing, script);
}

template <IsScript S>
inline void World::destroy_script(Thing thing) noexcept {
    if (thing.is_nil()) [[unlikely]] {
        return;
    }
    if (m_registry.is_dead(thing)) [[unlikely]] {
        return;
    }
    if (!m_registry.has<S>(thing)) [[unlikely]] {
        return;
    }

    if constexpr (ScriptHasOnDestroy<S>) {
        m_registry.get_unchecked<S>(thing).on_destroy();
    }

    m_registry.destroy_checked<S>(thing);
}

// ================================================ Scope Method Implementations

inline Scope::Scope() noexcept
    : m_world(nullptr) {
}

inline Scope::Scope(World *world) noexcept
    : m_world(world) {
}

inline Thing Scope::handout() noexcept {
    return m_world->handout();
}

inline Thing Scope::handout_deferred() noexcept {
    return m_world->handout_deferred();
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
inline bool Scope::destroy_now(Thing thing) noexcept {
    return m_world->destroy_now<T>(thing);
}

template <typename T>
inline T *Scope::try_get(Thing thing) noexcept {
    return m_world->try_get<T>(thing);
}

template <typename T>
inline T &Scope::get(Thing thing) noexcept {
    return m_world->get<T>(thing);
}

inline void Scope::attach_child_now(Thing parent, Thing child) noexcept {
    m_world->attach_child_now(parent, child);
}

inline void Scope::attach_child(Thing parent, Thing child) noexcept {
    m_world->attach_child(parent, child);
}

inline void Scope::detach_child_now(Thing parent, Thing child) noexcept {
    m_world->detach_child_now(parent, child);
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

template <typename S>
inline void Scope::insert_script(Thing thing, const S script) noexcept {
    m_world->insert_script(thing, script);
}

template <typename S>
inline void Scope::destroy_script(Thing thing) noexcept {
    m_world->destroy_script<S>(thing);
}

inline void Script::set_self(Thing thing) noexcept {
    m_self = thing;
}

inline void Script::set_scope(Scope scope) noexcept {
    m_scope = scope;
}

inline Thing Script::self() const noexcept {
    return m_self;
}

inline Scope &Script::scope() noexcept {
    return m_scope;
}

template <typename T>
inline bool Script::has() const noexcept {
    return m_scope.has<T>(m_self);
}

template <typename T>
inline T &Script::get() noexcept {
    return m_scope.get<T>(m_self);
}

template <typename T>
inline void Script::insert(const T &part) noexcept {
    m_scope.insert(m_self, part);
}

template <typename T>
inline void Script::insert(T &&part) noexcept {
    m_scope.insert(m_self, std::move(part));
}

template <typename T>
inline void Script::destroy() noexcept {
    m_scope.destroy<T>(m_self);
}
} // namespace fr
