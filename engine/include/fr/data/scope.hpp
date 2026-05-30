/**
 * @file scope.hpp
 * @author Kiju
 *
 * @brief Scope is a lightweight handle to a World, safe for use inside systems.
 * @note Method implementations are provided in world.hpp, which must be included for use.
 */

#pragma once

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
     * @brief Kills a thing. Nil thing is immortal.
     * @note If a thing is dead, its signature is still reset.
     */
    void kill(Thing thing) noexcept;

    /**
     * @brief Checks if a thing is alive. Nil thing is always alive.
     */
    bool is_alive(Thing thing) const noexcept;

    /**
     * @brief Checks if a thing is dead. Nil thing is never dead.
     */
    bool is_dead(Thing thing) const noexcept;

    // --------------------------------------------------------- Part Operations

    /**
     * @brief Checks if a thing has part T.
     * @note Returns false if the pool is missing or the thing is dead.
     * @note Returns true for nil thing if the pool exists.
     */
    template <typename T>
    bool has(Thing thing) const noexcept;

    /**
     * @brief Tries to emplace part T on a thing.
     * @note Creates the part pool if missing. Returns stub for nil thing.
     * @note Returns nullptr if the thing is dead or already has T.
     */
    template <typename T, typename... Args>
    T *try_emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Tries to insert part T on a thing by const reference.
     */
    template <typename T>
    T *try_insert_now(Thing thing, const T &part) noexcept;

    /**
     * @brief Tries to insert part T on a thing by rvalue reference.
     */
    template <typename T>
    T *try_insert_now(Thing thing, T &&part) noexcept;

    /**
     * @brief Emplaces part T on a thing.
     * @note Creates the part pool if missing. Returns stub for nil thing.
     * @warning Asserts if the thing is dead or already owns T.
     */
    template <typename T, typename... Args>
    T &emplace_now(Thing thing, Args &&...args) noexcept;

    /**
     * @brief Inserts part T on a thing by const reference.
     */
    template <typename T>
    T &insert_now(Thing thing, const T &part) noexcept;

    /**
     * @brief Inserts part T on a thing by rvalue reference.
     */
    template <typename T>
    T &insert_now(Thing thing, T &&part) noexcept;

    /**
     * @brief Tries to destroy part T on a thing.
     * @note Returns false if the pool is missing or the thing does not have T.
     */
    template <typename T>
    bool try_destroy_now(Thing thing) noexcept;

    /**
     * @brief Destroys part T on a thing.
     * @note Returns false if thing is nil.
     * @warning Asserts if pool missing or the thing does not have T.
     */
    template <typename T>
    bool destroy_now(Thing thing) noexcept;

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
     * @brief Records an insert command for part T on a thing.
     * @note Does nothing if thing is nil, dead, or already owns T.
     */
    template <typename T>
    void insert(Thing thing, const T &part) noexcept;

    /**
     * @brief Records an insert command for part T on a thing.
     * @note Does nothing if thing is nil, dead, or already owns T.
     */
    template <typename T>
    void insert(Thing thing, T &&part) noexcept;

    /**
     * @brief Records a destroy command for part T on a thing.
     * @note Does nothing if thing is nil, dead, or does not own T.
     */
    template <typename T>
    void destroy(Thing thing) noexcept;

    // ------------------------------------------------------------------- Query

    /**
     * @brief Tries to get part T owned by the thing.
     * @note Returns nullptr if pool is missing, thing is dead, or does not own T.
     * @note Returns stub pointer for nil thing if the pool exists.
     */
    template <typename T>
    T *try_get(Thing thing) noexcept;

    /**
     * @brief Returns part T owned by the thing.
     * @warning Asserts if thing is dead or does not have T.
     */
    template <typename T>
    T &get(Thing thing) noexcept;

    /**
     * @brief Creates a query for a set of parts.
     */
    template <typename... Include>
    auto query() noexcept;

private:
    // -------------------------------------------------------- Member Variables
    World *m_world{nullptr};
};

} // namespace fr
