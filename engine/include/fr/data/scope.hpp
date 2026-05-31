/**
 * @file scope.hpp
 * @author Kiju
 *
 * @brief Scope is a lightweight handle to a World, safe for use inside systems.
 * @note Method implementations are provided in world.hpp, which must be included for use.
 */

#pragma once

#include "fr/data/query.hpp"
#include "fr/data/thing.hpp"

namespace fr {

class World;

/**
 * @brief Scope is a mini version of the World that forwards thing and part operations.
 * It is safe to pass around inside systems for world mutation.
 */
class Scope {
public:
    // -------------------------------------------------- Constructors

    Scope() noexcept;
    Scope(World *world) noexcept;

    Scope(const Scope &) noexcept = default;
    Scope(Scope &&) noexcept = default;
    Scope &operator=(const Scope &) noexcept = default;
    Scope &operator=(Scope &&) noexcept = default;

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

    // ----------------------------------------------------------------- Scripts

    /**
     * @brief Removes a script from a thing, calling on_destroy if defined.
     * @note Does nothing if thing is nil, dead, or does not own script S.
     */
    template <typename S>
    void destroy_script(Thing thing) noexcept;

private:
    // -------------------------------------------------------- Member Variables
    World *m_world{nullptr};
};

} // namespace fr
