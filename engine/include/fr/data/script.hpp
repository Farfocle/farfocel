/**
 * @file script.hpp
 * @author Kiju
 *
 * @brief Script offers an alternative view of the data stored in the hidden ECS.
 * @details Karol Szypula would despise that definition.
 */

#pragma once

#include <concepts>
#include <utility>

#include "fr/core/bitset.hpp"
#include "fr/core/meta.hpp"
#include "fr/data/part.hpp"

namespace fr {
// ==================================================================== Typedefs

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

// ====================================================================== Script

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

    void set_self(Thing thing) noexcept {
        m_self = thing;
    }

    void set_scope(Scope *scope) noexcept {
        m_scope = scope;
    }

    [[nodiscard]] Thing self() const noexcept {
        return m_self;
    }

    [[nodiscard]] Scope &scope() noexcept {
        return *m_scope;
    }

    /**
     * @brief Checks if self has part `T`.
     * @note Returns false if the pool is missing
     */
    template <typename T>
    bool has() const noexcept {
        return m_scope->has<T>(m_self);
    }

    /**
     * @brief Returns a reference to the part `T` attached to self.
     * @warning Asserts if self does not have a part `T`.
     */
    template <typename T>
    T &get() noexcept {
        return m_scope->get<T>(m_self);
    }

    template <typename T>
    void insert(const T &part) noexcept {
        m_scope->insert(m_self, part);
    }

    template <typename T>
    void insert(T &&part) noexcept {
        m_scope->insert(m_self, std::move(part));
    }

    template <typename T>
    void destroy() noexcept {
        m_scope->destroy<T>(m_self);
    }

private:
    // -------------------------------------------------------- Member Variables
    Scope *m_scope{nullptr};
    Thing m_self{Thing::nil()};
};

// ================================================================= Script Pool

namespace impl {

class ScriptPool {
public:
    // ----------------------------------------------- Constructors & Destructor
    ScriptPool() noexcept = delete;
    ScriptPool(World &world) noexcept
        : m_world(world),
          m_scope(world) {
    }

    ScriptPool(const ScriptPool &) noexcept = delete;
    ScriptPool(ScriptPool &&) noexcept = delete;
    ScriptPool &operator=(const ScriptPool &) = delete;
    ScriptPool &operator=(ScriptPool &&) = delete;

    // ------------------------------------------------------- Script Management

    template <typename T>
    void insert_script(Thing thing, T script) noexcept {
        if (m_world.has<T>(thing)) {
            return;
        }

        script.set_self(thing);
        script.set_scope(&m_scope);

        TypeIdx tidx = TypeIdx::from_type<T>();
        if (!m_schedules.check_bit(tidx.idx())) {
            m_schedules.one_bit(tidx.idx());

            if constexpr (ScriptHasOnPreUpdate<T>) {
                m_world.schedule_sync(Stage::PreUpdate, [](Scope &scope) {
                    for (auto [t, s] : scope.query<T>()) {
                        s.run_pre_update();
                    }
                });
            }

            if constexpr (ScriptHasOnUpdate<T>) {
                m_world.schedule_sync(Stage::Update, [](Scope &scope) {
                    for (auto [t, s] : scope.query<T>()) {
                        s.run_update();
                    }
                });
            }

            if constexpr (ScriptHasOnPostUpdate<T>) {
                m_world.schedule_sync(Stage::PostUpdate, [](Scope &scope) {
                    for (auto [t, s] : scope.query<T>()) {
                        s.run_post_update();
                    }
                });
            }
        }

        m_world.insert(thing, script);
    }

private:
    // -------------------------------------------------------- Member Variables
    Bitset<MAX_PARTS> m_schedules;
    World &m_world;
    Scope m_scope;
};
} // namespace impl
} // namespace fr
