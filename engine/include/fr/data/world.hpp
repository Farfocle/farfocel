/**
 * @file world.hpp
 * @author Kiju
 *
 * @brief World is the heart of all data operations in the Farfocel.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/inline_function.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// ==================================================================== Typedefs
class World;
class Scope;
using System = Fn128<void(Scope &)>;
enum class Stage : U8 { PreUpdate, Update, PostUpdate };
constexpr U8 STAGE_COUNT = 3;

// ================================================================== SystemPool
namespace impl {
class SystemPool {
public:
    // ----------------------------------------------- Constructors & Destructor

    SystemPool() noexcept
        : SystemPool(get_ambient_ctx().alloc) {};

    explicit SystemPool(Alloc *alloc) noexcept {
        m_alloc = alloc;

        for (auto &systems : m_stages) {
            systems = DynamicArray<System>::with_alloc(alloc);
        }
    }

    SystemPool(const SystemPool &) = delete;
    SystemPool(SystemPool &&) = delete;
    SystemPool &operator=(const SystemPool &) = delete;
    SystemPool &operator=(SystemPool &&) = delete;

    // --------------------------------------------------------------------- API

    void schedule_sync(Stage stage, const System &system) noexcept {
        U8 stage_idx = static_cast<U8>(stage);
        m_stages[stage_idx].push_back(system);
    }

    void run_stage_sync(Stage stage, Scope &scope) {
        U8 stage_idx = static_cast<U8>(stage);
        do_run_stage(stage_idx, scope);
    }

    void run_all_sync(Scope &scope) {
        for (U8 i = 0; i < m_stages.size(); ++i) {
            do_run_stage(i, scope);
        }
    }

    // ---------------------------------------------- Helpers & Member Variables

private:
    void do_run_stage(U8 stage_idx, Scope &scope) noexcept {
        for (const auto &system : m_stages[stage_idx]) {
            system(scope);
        }
    }

    const Alloc *m_alloc{nullptr};
    Array<DynamicArray<System>, STAGE_COUNT> m_stages{};
};
} // namespace impl

// ======================================================================= World

class World {
public:
    // -------------------------------------------------- Typedefs & Contructors

    struct Options {
        Alloc *registry_alloc{get_ambient_ctx().alloc};
        Alloc *system_pool_alloc{get_ambient_ctx().alloc};
    };

    World() noexcept
        : World(Options{}) {
    }

    World(const Options &opt) noexcept
        : m_options(opt),
          m_registry(opt.registry_alloc),
          m_system_pool(opt.system_pool_alloc) {
    }

    World(const World &) = delete;
    World(World &&) = delete;
    World &operator=(const World &) = delete;
    World &operator=(World &&) = delete;

    // -------------------------------------------------------- Thing Operations

    /**
     * @brief Returns a fresh, non-nil thing.
     */
    Thing handout() noexcept {
        return m_registry.handout();
    }

    /**
     * @brief Kills a thing, adding its slot into a free list.
     * @note If a thing is nil, does nothing, nil thing is immortal.
     * @note If a thing is dead, the pool does nothing but the signature is reset.
     */
    void kill(Thing thing) noexcept {
        m_registry.kill(thing);
    }

    /**
     * @brief Checks if a thing is alive.
     * @note The nil thing is alive and immortal.
     */
    bool is_alive(Thing thing) const noexcept {
        return m_registry.is_alive(thing);
    }

    /**
     * @brief Checks if a thing is dead.
     * @note The nil thing is alive and immortal.
     */
    bool is_dead(Thing thing) const noexcept {
        return !is_alive(thing);
    }

    // --------------------------------------------------------- Part Operations

    /**
     * @brief Checks if a thing has part `T`.
     * @note Returns false if the pool is missing or the thing is dead.
     * @note Returns true for nil thing if the pool exists.
     */
    template <typename T>
    bool has(Thing thing) const noexcept {
        return m_registry.owns<T>(thing);
    }

    /**
     * @brief Tries to emplace part `T` on a thing.
     * @note Creates the part pool if missing.
     * @note Returns the stub for nil thing.
     * @note Returns nullptr if the thing is dead or already has `T`.
     */
    template <typename T, typename... Args>
    T *try_emplace_now(Thing thing, Args &&...args) noexcept {
        return m_registry.try_emplace<T>(thing, std::forward<Args>(args)...);
    }

    /**
     * @brief Tries to insert part `T` on a thing by const reference.
     * @note Same behavior as `try_emplace`.
     */
    template <typename T>
    T *try_insert_now(Thing thing, const T &part) noexcept {
        return m_registry.try_insert<T>(thing, part);
    }

    /**
     * @brief Tries to insert part `T` on a thing by rvalue reference.
     * @note Same behavior as `try_emplace`.
     */
    template <typename T>
    T *try_insert_now(Thing thing, T &&part) noexcept {
        return m_registry.try_insert<T>(thing, std::forward<T>(part));
    }

    /**
     * @brief Emplaces part `T` on a thing.
     * @note Creates the part pool if missing.
     * @note Returns the stub for nil thing.
     * @warning Asserts if the thing is dead or already owns `T`.
     */
    template <typename T, typename... Args>
    T &emplace_now(Thing thing, Args &&...args) noexcept {
        return m_registry.emplace<T>(thing, std::forward<Args>(args)...);
    }

    /**
     * @brief Inserts part `T` on a thing by const reference.
     * @note Same behavior as `emplace`.
     */
    template <typename T>
    T &insert_now(Thing thing, const T &part) noexcept {
        return m_registry.insert<T>(thing, part);
    }

    /**
     * @brief Inserts part `T` on a thing by rvalue reference.
     * @note Same behavior as `emplace`.
     */
    template <typename T>
    T &insert_now(Thing thing, T &&part) noexcept {
        return m_registry.insert<T>(thing, std::forward<T>(part));
    }

    /**
     * @brief Tries to destroy part `T` on a thing.
     * @note Returns false if thing is nil, pool is missing, or the thing does not have part `T`.
     */
    template <typename T>
    bool try_destroy_now(Thing thing) noexcept {
        return m_registry.try_destroy<T>(thing);
    }

    /**
     * @brief Destroys part `T` on a thing.
     * @note Returns false if thing is nil.
     * @warning Asserts if the pool is missing or the thing does not have part `T`.
     */
    template <typename T>
    bool destroy_now(Thing thing) noexcept {
        return m_registry.destroy<T>(thing);
    }

    // ------------------------------------------------- Command Part Operations

    /**
     * @brief Apply all recorded insert commands across all part pools.
     */
    void commit_insert_part_cmds() noexcept {
        m_registry.commit_insert_all();
    }

    /**
     * @brief Apply all recorded mutate commands across all part pools.
     */
    void commit_mutate_part_cmds() noexcept {
        m_registry.commit_mutate_all();
    }

    /**
     * @brief Apply all recorded destroy commands across all part pools.
     */
    void commit_destroy_part_cmds() noexcept {
        m_registry.commit_destroy_all();
    }

    /**
     * @brief Applies all recorded part commands across all part pools.
     * @note The order of operations is as following:
     * 1. mutate commands
     * 2. destroy commands
     * 3. insert commands
     */
    void commit_part_cmds() noexcept {
        commit_mutate_part_cmds();
        commit_destroy_part_cmds();
        commit_insert_part_cmds();
    }

    /**
     * @brief Records an insert command for part `T` on a thing.
     * @note If thing is nil or dead; does nothing.
     * @note If the does own a part `T`; does nothing.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept {
        m_registry.record_insert(thing, part);
    }

    /**
     * @brief Records an insert command for part `T` on a thing.
     * @note If thing is nil or dead; does nothing.
     * @note If the does own a part `T`; does nothing.
     */
    template <typename T>
    void insert(Thing thing, T &&part) noexcept {
        m_registry.record_insert(thing, std::move(part));
    }

    /**
     * @brief Record a destroy command for part `T` on a thing.
     * @note If thing is nil or dead; does nothing.
     * @note If the thing does not own the part `T`; does nothing.
     */
    template <typename T>
    void destroy(Thing thing) noexcept {
        m_registry.record_destroy<T>(thing);
    }

    // ------------------------------------------------------------------- Query

    /**
     * @brief Tries to get the part `T` owned by the thing.
     * @note Returns nullptr if the pool is missing, the thing is dead, or the thing does not own
     * `T`.
     * @note Returns the stub pointer for nil thing if the pool exists.
     */
    template <typename T>
    T *try_get(Thing thing) noexcept {
        return m_registry.try_get<T>(thing);
    }

    /**
     * @brief Returns a reference to the part `T` owned by the thing.
     * @warning Asserts if the thing is dead or does not have a part `T`.
     */
    template <typename T>
    T &get(Thing thing) noexcept {
        return m_registry.get<T>(thing);
    }

    /**
     * @brief Creates a query for a set of parts.
     */
    template <typename... Include>
    auto query() noexcept {
        return m_registry.query<Include...>();
    }

    // ----------------------------------------------------------------- Systems

    /**
     * @brief Schedules a system for synchronous execution in the given stage.
     */
    void schedule_sync(Stage stage, const System &system) noexcept {
        m_system_pool.schedule_sync(stage, system);
    }

    /**
     * @brief Runs all systems scheduled for the given stage synchronously.
     */
    void run_stage_sync(Stage stage) noexcept;

    /**
     * @brief Runs all systems scheduled for all stages synchronously.
     */
    void run_all_sync() noexcept;

    // --------------------------------------------------------------- Internals

private:
    Options m_options{};
    impl::Registry m_registry{};
    impl::SystemPool m_system_pool{};
};

// ======================================================================= Scope

/**
 * @brief `Scope` is a mini version of the `World`. It forwards many thing and part operations,
 * without providing system management. Scope is safe to use in systems and to be passed around for
 * world mutation operations.
 */
class Scope {
public:
    // -------------------------------------------------- Typedefs & Contructors

    explicit Scope(World &world) noexcept
        : m_world(world) {
    }

    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    Scope &operator=(const Scope &) = delete;
    Scope &operator=(Scope &&) = delete;

    // -------------------------------------------------------- Thing Operations

    /**
     * @brief Returns a fresh, non-nil thing.
     */
    Thing handout() noexcept {
        return m_world.handout();
    }

    /**
     * @brief Kills a thing, adding its slot into a free list.
     * @note If a thing is nil, does nothing, nil thing is immortal.
     * @note If a thing is dead, the pool does nothing but the signature is reset.
     */
    void kill(Thing thing) noexcept {
        m_world.kill(thing);
    }

    /**
     * @brief Checks if a thing is alive.
     * @note The nil thing is alive and immortal.
     */
    bool is_alive(Thing thing) const noexcept {
        return m_world.is_alive(thing);
    }

    /**
     * @brief Checks if a thing is dead.
     * @note The nil thing is alive and immortal.
     */
    bool is_dead(Thing thing) const noexcept {
        return m_world.is_dead(thing);
    }

    // --------------------------------------------------------- Part Operations

    /**
     * @brief Checks if a thing has part `T`.
     * @note Returns false if the pool is missing or the thing is dead.
     * @note Returns true for nil thing if the pool exists.
     */
    template <typename T>
    bool has(Thing thing) const noexcept {
        return m_world.has<T>(thing);
    }

    /**
     * @brief Tries to emplace part `T` on a thing.
     * @note Creates the part pool if missing.
     * @note Returns the stub for nil thing.
     * @note Returns nullptr if the thing is dead or already has `T`.
     */
    template <typename T, typename... Args>
    T *try_emplace_now(Thing thing, Args &&...args) noexcept {
        return m_world.try_emplace_now<T>(thing, std::forward<Args>(args)...);
    }

    /**
     * @brief Tries to insert part `T` on a thing by const reference.
     * @note Same behavior as `try_emplace`.
     */
    template <typename T>
    T *try_insert_now(Thing thing, const T &part) noexcept {
        return m_world.try_insert_now<T>(thing, part);
    }

    /**
     * @brief Tries to insert part `T` on a thing by rvalue reference.
     * @note Same behavior as `try_emplace`.
     */
    template <typename T>
    T *try_insert_now(Thing thing, T &&part) noexcept {
        return m_world.try_insert_now<T>(thing, std::forward<T>(part));
    }

    /**
     * @brief Emplaces part `T` on a thing.
     * @note Creates the part pool if missing.
     * @note Returns the stub for nil thing.
     * @warning Asserts if the thing is dead or already owns `T`.
     */
    template <typename T, typename... Args>
    T &emplace_now(Thing thing, Args &&...args) noexcept {
        return m_world.emplace_now<T>(thing, std::forward<Args>(args)...);
    }

    /**
     * @brief Inserts part `T` on a thing by const reference.
     * @note Same behavior as `emplace`.
     */
    template <typename T>
    T &insert_now(Thing thing, const T &part) noexcept {
        return m_world.insert_now<T>(thing, part);
    }

    /**
     * @brief Inserts part `T` on a thing by rvalue reference.
     * @note Same behavior as `emplace`.
     */
    template <typename T>
    T &insert_now(Thing thing, T &&part) noexcept {
        return m_world.insert_now<T>(thing, std::forward<T>(part));
    }

    /**
     * @brief Tries to destroy part `T` on a thing.
     * @note Returns false if thing is nil, pool is missing, or the thing does not have part `T`.
     */
    template <typename T>
    bool try_destroy_now(Thing thing) noexcept {
        return m_world.try_destroy_now<T>(thing);
    }

    /**
     * @brief Destroys part `T` on a thing.
     * @note Returns false if thing is nil.
     * @warning Asserts if the pool is missing or the thing does not have part `T`.
     */
    template <typename T>
    bool destroy_now(Thing thing) noexcept {
        return m_world.destroy_now<T>(thing);
    }

    // ------------------------------------------------- Command Part Operations

    /**
     * @brief Apply all recorded insert commands across all part pools.
     */
    void commit_insert_part_cmds() noexcept {
        m_world.commit_insert_part_cmds();
    }

    /**
     * @brief Apply all recor
✗ Sorry, you've hit a rate limit that restricts the number of Copilot model requests you can make
within a specific time period. Please try again ided mutate commands across all part pools.
     */
    void commit_mutate_part_cmds() noexcept {
        m_world.commit_mutate_part_cmds();
    }

    /**
     * @brief Apply all recorded destroy commands across all part pools.
     */
    void commit_destroy_part_cmds() noexcept {
        m_world.commit_destroy_part_cmds();
    }

    /**
     * @brief Applies all recorded part commands across all part pools.
     * @note The order of operations is as following:
     * 1. mutate commands
     * 2. destroy commands
     * 3. insert commands
     */
    void commit_part_cmds() noexcept {
        m_world.commit_part_cmds();
    }

    /**
     * @brief Records an insert command for part `T` on a thing.
     * @note If thing is nil or dead; does nothing.
     * @note If the does own a part `T`; does nothing.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept {
        m_world.insert(thing, part);
    }

    /**
     * @brief Records an insert command for part `T` on a thing.
     * @note If thing is nil or dead; does nothing.
     * @note If the does own a part `T`; does nothing.
     */
    template <typename T>
    void insert(Thing thing, T &&part) noexcept {
        m_world.insert(thing, std::move(part));
    }

    /**
     * @brief Record a destroy command for part `T` on a thing.
     * @note If thing is nil or dead; does nothing.
     * @note If the thing does not own the part `T`; does nothing.
     */
    template <typename T>
    void destroy(Thing thing) noexcept {
        m_world.destroy<T>(thing);
    }

    // ------------------------------------------------------------------- Query

    /**
     * @brief Tries to get the part `T` owned by the thing.
     * @note Returns nullptr if the pool is missing, the thing is dead, or the thing does not own
     * `T`.
     * @note Returns the stub pointer for nil thing if the pool exists.
     */
    template <typename T>
    T *try_get(Thing thing) noexcept {
        return m_world.try_get<T>(thing);
    }

    /**
     * @brief Returns a reference to the part `T` owned by the thing.
     * @warning Asserts if the thing is dead or does not have a part `T`.
     */
    template <typename T>
    T &get(Thing thing) noexcept {
        return m_world.get<T>(thing);
    }

    /**
     * @brief Creates a query for a set of parts.
     */
    template <typename... Include>
    auto query() noexcept {
        return m_world.query<Include...>();
    }

private:
    World &m_world;
};

// ================================================= System Operations for World

inline void World::run_stage_sync(Stage stage) noexcept {
    Scope scope{*this};
    m_system_pool.run_stage_sync(stage, scope);
}

inline void World::run_all_sync() noexcept {
    Scope scope{*this};
    m_system_pool.run_all_sync(scope);
}
} // namespace fr
