/**
 * @file world.hpp
 * @author Kiju
 *
 * @brief World is the heart of all data operations in Farfocel.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/bitset.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/data/cmd.hpp"
#include "fr/data/part.hpp"
#include "fr/data/query.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/relations.hpp"
#include "fr/data/scope.hpp"
#include "fr/data/script.hpp"
#include "fr/data/stage.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// ================================================================== SystemPool

namespace impl {

class SystemPool {
public:
    // ----------------------------------------------- Constructors & Destructor

    SystemPool() noexcept;
    explicit SystemPool(Alloc *alloc) noexcept;

    SystemPool(const SystemPool &) = delete;
    SystemPool(SystemPool &&) = delete;
    SystemPool &operator=(const SystemPool &) = delete;
    SystemPool &operator=(SystemPool &&) = delete;

    // --------------------------------------------------------------------- API

    /**
     * @brief Schedules a system for synchronous execution in the given stage.
     */
    void schedule_sync(Stage stage, const System &system) noexcept;

    /**
     * @brief Runs all systems scheduled for the given stage synchronously.
     */
    void run_stage_sync(Stage stage, Scope scope);

    /**
     * @brief Runs all systems scheduled for all stages synchronously.
     */
    void run_all_sync(Scope &scope);

private:
    /**
     * @brief Executes all systems for a given stage index.
     */
    void do_run_stage(U8 stage_idx, Scope scope) noexcept;

    // -------------------------------------------------------- Member Variables
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
        USize cmd_arena_size{impl::CmdPool::DEFAULT_ARENA_SIZE};
    };

    World() noexcept;
    explicit World(const Options &opt) noexcept;

    World(const World &) = delete;
    World(World &&) = delete;
    World &operator=(const World &) = delete;
    World &operator=(World &&) = delete;

    // -------------------------------------------------------- Thing Operations

    /**
     * @brief Returns a fresh, non-nil thing.
     */
    Thing handout() noexcept;

    /**
     * @brief Kills a thing and destroys all of its parts.
     * @note Does nothing for nil or already-dead things.
     */
    void kill(Thing thing) noexcept;

    /**
     * @brief Returns true if the thing is alive. Nil thing is always alive.
     */
    bool is_alive(Thing thing) const noexcept;

    /**
     * @brief Returns true if the thing is dead. Nil thing is never dead.
     */
    bool is_dead(Thing thing) const noexcept;

    // --------------------------------------------------------- Part Operations

    /**
     * @brief Returns true if the thing owns part T.
     */
    template <typename T>
    bool has(Thing thing) const noexcept;

    /**
     * @brief Inserts or overrides part T on a thing immediately.
     * - Nil thing  : returns the stub pointer.
     * - Dead thing : returns nullptr.
     * - Alive thing: inserts T or overrides it if already present.
     */
    template <typename T, typename... Args>
    T *try_emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Inserts part T immediately without any checks.
     * @pre Caller must ensure: thing is alive, thing does NOT yet own T.
     */
    template <typename T, typename... Args>
    T &emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Destroys part T on a thing immediately.
     * @return false if thing is nil, dead, or does not own T.
     */
    template <typename T>
    bool destroy_now(Thing thing) noexcept;

    /**
     * @brief Returns a pointer to part T owned by the thing, or nullptr if not found.
     */
    template <typename T>
    T *try_get(Thing thing) noexcept;

    /**
     * @brief Returns a reference to part T owned by the thing.
     * @pre Caller must ensure: thing is alive and owns T.
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
     * @brief Apply all recorded insert commands across all part pools.
     */
    void commit_insert_part_cmds() noexcept;

    /**
     * @brief Apply all recorded mutate commands across all part pools.
     */
    void commit_mutate_part_cmds() noexcept;

    /**
     * @brief Apply all recorded destroy commands across all part pools.
     */
    void commit_destroy_part_cmds() noexcept;

    /**
     * @brief Apply all commands in order: mutate → destroy → insert.
     */
    void commit_part_cmds() noexcept;

    /**
     * @brief Apply all recorded attach-child commands.
     */
    void commit_attach_child_cmds() noexcept;

    /**
     * @brief Apply all recorded detach-child commands.
     */
    void commit_detach_child_cmds() noexcept;

    /**
     * @brief Records a deferred insert command for part T on a thing.
     * @note Does nothing if thing is nil, dead, or already owns T.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept;

    /**
     * @brief Records a deferred insert command for part T on a thing.
     * @note Does nothing if thing is nil, dead, or already owns T.
     */
    template <typename T>
    void insert(Thing thing, T &&part) noexcept;

    /**
     * @brief Records a deferred destroy command for part T on a thing.
     * @note Does nothing if thing is nil, dead, or does not own T.
     */
    template <typename T>
    void destroy(Thing thing) noexcept;

    // ------------------------------------------------------------------- Query

    /**
     * @brief Creates a forward query for things owning all parts in the Include list.
     */
    template <typename... Include>
    auto query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a reverse query for things owning all parts in the Include list.
     */
    template <typename... Include>
    auto reverse_query(QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a query over the direct children of a thing.
     */
    template <typename... Include>
    auto shallow_query(Thing thing, QueryOptions options = {}) noexcept;

    /**
     * @brief Creates a depth-first query over all descendants of a thing.
     */
    template <typename... Include>
    auto deep_query(Thing thing, QueryOptions options = {}) noexcept;

    // ----------------------------------------------------------------- Systems

    /**
     * @brief Schedules a system for synchronous execution in the given stage.
     */
    void schedule_sync(Stage stage, const System &system) noexcept;

    /**
     * @brief Runs all systems scheduled for the given stage synchronously.
     */
    void run_stage_sync(Stage stage) noexcept;

    /**
     * @brief Runs all systems scheduled for all stages synchronously.
     */
    void run_all_sync() noexcept;

    // ----------------------------------------------------------------- Scripts

    /**
     * @brief Attaches a script to a thing and registers its lifecycle systems.
     * @note Systems are only registered once per script type.
     */
    template <IsScript S>
    void insert_script(Thing thing, S script) noexcept;

    /**
     * @brief Removes a script from a thing, calling on_destroy if defined.
     * @note Does nothing if thing is nil, dead, or does not own script S.
     */
    template <IsScript S>
    void destroy_script(Thing thing) noexcept;

private:
    // --------------------------------------------------------------- Internals
    void do_update_hierarchy(HierarchyDepth parent_depth, Thing child) noexcept;
    void do_detach_from_hierarchy_unchecked(Thing thing) noexcept;

    // -------------------------------------------------------- Member Variables
    Options m_options{};
    impl::Registry m_registry{};
    impl::CmdPool m_cmd_pool{};
    impl::SystemPool m_system_pool{};
    Bitset<MAX_PARTS> m_script_registry{};
};

// ========================================= SystemPool Method Implementations

inline impl::SystemPool::SystemPool() noexcept
    : SystemPool(get_ambient_ctx().alloc) {
}

inline impl::SystemPool::SystemPool(Alloc *alloc) noexcept {
    m_alloc = alloc;
    for (auto &systems : m_stages) {
        systems = DynamicArray<System>::with_alloc(alloc);
    }
}

inline void impl::SystemPool::schedule_sync(Stage stage, const System &system) noexcept {
    m_stages[static_cast<U8>(stage)].push_back(system);
}

inline void impl::SystemPool::run_stage_sync(Stage stage, Scope scope) {
    do_run_stage(static_cast<U8>(stage), scope);
}

inline void impl::SystemPool::run_all_sync(Scope &scope) {
    for (U8 i = 0; i < m_stages.size(); ++i) {
        do_run_stage(i, scope);
    }
}

inline void impl::SystemPool::do_run_stage(U8 stage_idx, Scope scope) noexcept {
    for (const auto &system : m_stages[stage_idx]) {
        system(scope);
    }
}

// ============================================= World Method Implementations

inline World::World() noexcept
    : World(Options{}) {
}

inline World::World(const Options &opt) noexcept
    : m_options(opt),
      m_registry(opt.registry_alloc),
      m_cmd_pool(opt.registry_alloc, opt.cmd_arena_size),
      m_system_pool(opt.system_pool_alloc),
      m_script_registry(Bitset<MAX_PARTS>::with_zeros()) {
}

inline Thing World::handout() noexcept {
    return m_registry.handout();
}

inline void World::kill(Thing thing) noexcept {
    m_registry.kill(thing);
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

inline void World::commit_insert_part_cmds() noexcept {
    m_cmd_pool.commit_insert_all(&m_registry);
}

inline void World::commit_mutate_part_cmds() noexcept {
    m_cmd_pool.commit_mutate_all(&m_registry);
}

inline void World::commit_destroy_part_cmds() noexcept {
    m_cmd_pool.commit_destroy_all(&m_registry);
}

inline void World::commit_part_cmds() noexcept {
    commit_mutate_part_cmds();
    commit_destroy_part_cmds();
    commit_insert_part_cmds();
    m_cmd_pool.reset();
}

inline void World::commit_attach_child_cmds() noexcept {
    m_cmd_pool.commit_attach_child_all(this);
}

inline void World::commit_detach_child_cmds() noexcept {
    m_cmd_pool.commit_detach_child_all(this);
}

template <typename T>
inline void World::insert(Thing thing, const T &part) noexcept {
    m_cmd_pool.record_insert<T>(m_registry, thing, part);
}

template <typename T>
inline void World::insert(Thing thing, T &&part) noexcept {
    m_cmd_pool.record_insert<T>(m_registry, thing, std::forward<T>(part));
}

template <typename T>
inline void World::destroy(Thing thing) noexcept {
    m_cmd_pool.record_destroy<T>(m_registry, thing);
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

    m_cmd_pool.record_attach_child(parent, child);
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

    m_cmd_pool.record_detach_child(parent, child);
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

inline void World::schedule_sync(Stage stage, const System &system) noexcept {
    m_system_pool.schedule_sync(stage, system);
}

inline void World::run_stage_sync(Stage stage) noexcept {
    Scope scope{this};
    m_system_pool.run_stage_sync(stage, scope);
}

inline void World::run_all_sync() noexcept {
    Scope scope{this};
    m_system_pool.run_all_sync(scope);
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
            schedule_sync(Stage::PreUpdateScript, [](Scope scope) {
                for (auto [t, s] : scope.query<S>()) {
                    s.on_pre_update();
                }
            });
        }

        if constexpr (ScriptHasOnUpdate<S>) {
            schedule_sync(Stage::UpdateScript, [](Scope scope) {
                for (auto [t, s] : scope.query<S>()) {
                    s.on_update();
                }
            });
        }

        if constexpr (ScriptHasOnPostUpdate<S>) {
            schedule_sync(Stage::PostUpdateScript, [](Scope scope) {
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

// ============================================== Scope Method Implementations
// World must be complete before these can be provided.

inline Scope::Scope() noexcept
    : m_world(nullptr) {
}

inline Scope::Scope(World *world) noexcept
    : m_world(world) {
}

inline Thing Scope::handout() noexcept {
    return m_world->handout();
}

inline void Scope::kill(Thing thing) noexcept {
    m_world->kill(thing);
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

inline void Scope::commit_insert_part_cmds() noexcept {
    m_world->commit_insert_part_cmds();
}

inline void Scope::commit_mutate_part_cmds() noexcept {
    m_world->commit_mutate_part_cmds();
}

inline void Scope::commit_destroy_part_cmds() noexcept {
    m_world->commit_destroy_part_cmds();
}

inline void Scope::commit_part_cmds() noexcept {
    m_world->commit_part_cmds();
}

inline void Scope::commit_attach_child_cmds() noexcept {
    m_world->commit_attach_child_cmds();
}

inline void Scope::commit_detach_child_cmds() noexcept {
    m_world->commit_detach_child_cmds();
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

template <typename S>
inline void Scope::destroy_script(Thing thing) noexcept {
    m_world->destroy_script<S>(thing);
}

} // namespace fr
