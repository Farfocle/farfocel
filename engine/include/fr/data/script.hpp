/**
 * @file script.hpp
 * @author Kiju
 *
 * @brief Script is a per-thing behaviour unit in the Farfocel ECS.
 * @details Karol Szypula would despise that definition.
 */

#pragma once

#include <concepts>
#include <utility>

#include "fr/data/scope.hpp"

namespace fr {

// ================================================================== Concepts

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

// ======================================================================= Script

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

// ======================================================================= IsScript

template <typename T>
concept IsScript = std::derived_from<T, Script>;

// ============================================= Script Method Implementations

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
