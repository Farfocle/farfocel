/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Commands and command pool for the data layer.
 * @details Implements the `fr::CmdPool` which allows for lazy modifications of the world state.
 * Akin to relation database transactions, this sytem allows for safe, batched and synnchonized
 * mutations.
 *
 * @todo Make a mechanism to collect the changes in an efficient way, this could be really helpful
 * for deterministic replays of the evolution of the world.
 */

#pragma once

#include <utility>

#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/inline_any.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/part.hpp"
#include "fr/data/thing.hpp"

namespace fr {

// =============================================================== Command Types

/**
 * @brief Kind of command that targets a part.
 */
enum class CmdKind : U8 { DestroyPart, InsertPart, MutatePart, AttachChild, DetachChild };

/**
 * @brief Command to destroy a part owned by a thing.
 * @tparam T Part type.
 */
template <typename T>
struct DestroyPartCmd {
    using Part = T;
    Thing thing;
};

/**
 * @brief Command to insert a part for a thing.
 * @tparam T Part type.
 */
template <typename T>
struct InsertPartCmd {
    using Part = T;
    Thing thing;
    Part part;
};

/**
 * @brief Command to mutate a part for a thing.
 * @tparam T Part type.
 */
template <typename T>
struct MutatePartCmd {
    /// @brief Part alias for tooling and reflection.
    using Part = T;

    /// @brief Target thing.
    Thing thing;

    /// @brief Previous part state.
    Part prev;

    /// @brief Next part state.
    Part next;
};
} // namespace fr

// ================================================================ Command Pool

namespace fr::impl {

/**
 * @brief Storage for command buffers for a specific part type `T`.
 * @tparam T Part type.
 */
template <typename T>
struct CmdStorage {
    DynamicArray<DestroyPartCmd<T>> destroy_cmds{};
    DynamicArray<InsertPartCmd<T>> insert_cmds{};
    DynamicArray<MutatePartCmd<T>> mutate_cmds{};

    /**
     * @brief Default constructor that uses the ambient allocator.
     */
    CmdStorage() noexcept
        : CmdStorage(get_ambient_ctx().alloc) {
    }

    /**
     * @brief Constructs a `CmdStorage` with the specified allocator.
     * @param alloc Allocator to use for command buffer memory.
     * @pre `alloc` must be non-null.
     */
    explicit CmdStorage(Alloc *alloc) noexcept {
        destroy_cmds = DynamicArray<DestroyPartCmd<T>>::with_alloc(alloc);
        insert_cmds = DynamicArray<InsertPartCmd<T>>::with_alloc(alloc);
        mutate_cmds = DynamicArray<MutatePartCmd<T>>::with_alloc(alloc);
    }
};

/**
 * @brief Pool for managing all the command buffers.
 * @details Two main operations are supported: recording commands and committing them.
 */
class CmdPool {
public:
    // ------------------------------------ Typedefs & Constructors & Destructor

    using AnyCmdStorage = InlineAny<sizeof(CmdStorage<Byte>), alignof(CmdStorage<Byte>)>;
    using CmdCommitFn = void (*)(void *, AnyCmdStorage &) noexcept;

    /**
     * @brief Default constructor that uses the ambient allocator.
     */
    CmdPool() noexcept
        : CmdPool(get_ambient_ctx().alloc) {
    }

    /**
     * @brief Constructs a `CmdPool` with the specified allocator.
     * @param alloc Allocator to use for command buffer memory.
     * @pre `alloc` must be non-null.
     */
    explicit CmdPool(Alloc *alloc) noexcept
        : m_alloc(alloc) {
    }

    CmdPool(const CmdPool &) = delete;
    CmdPool(CmdPool &&) = delete;
    CmdPool &operator=(const CmdPool &) = delete;
    CmdPool &operator=(CmdPool &&) = delete;

    // --------------------------------------------------------- Record Commands

    /**
     * @brief Records a destroy command for the specified thing.
     * @tparam T Type of the part to destroy.
     * @tparam RegistryT Type of the registry.
     * @param registry Reference to the registry.
     * @param thing The thing to destroy.
     * @note If the thing is nil or dead; does nothing.
     * @note If the thing does not have the specified part; does nothing.
     */
    template <typename T, typename RegistryT>
    void record_destroy(RegistryT &registry, Thing thing) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        if (!registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        do_ensure_cmd_storage<T, RegistryT>().destroy_cmds.push_back(
            DestroyPartCmd<T>{.thing = thing});
    }

    /**
     * @brief Records an insert command for the specified thing and part.
     * @tparam T Type of the part to insert.
     * @tparam RegistryT Type of the registry.
     * @param registry Reference to the registry.
     * @param thing The thing to insert the part into.
     * @param part The part to insert.
     * @note If the thing is nil or dead; does nothing.
     * @note If the thing already has the specified part; does nothing.
     */
    template <typename T, typename RegistryT>
    void record_insert(RegistryT &registry, Thing thing, const T &part) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        if (registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        do_ensure_cmd_storage<T, RegistryT>().insert_cmds.push_back(
            InsertPartCmd<T>{.thing = thing, .part = part});
    }

    /**
     * @brief Records an insert command for a part into a thing.
     * @tparam T Type of the part.
     * @tparam RegistryT Type of the registry.
     * @param registry Reference to the registry.
     * @param thing The thing to insert the part into.
     * @param part The part to insert.
     * @note If the thing is nil or dead; does nothing.
     * @note If the thing already has the specified part; does nothing.
     */
    template <typename T, typename RegistryT>
    void record_insert(RegistryT &registry, Thing thing, T &&part) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        if (registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        do_ensure_cmd_storage<T, RegistryT>().insert_cmds.push_back(
            InsertPartCmd<T>{.thing = thing, .part = std::move(part)});
    }

    /**
     * @brief Records a mutate command for a part in a thing.
     * @tparam T Type of the part.
     * @tparam RegistryT Type of the registry.
     * @param registry Reference to the registry.
     * @param thing The thing to mutate the part in.
     * @param prev The previous value of the part.
     * @param next The new value of the part.
     * @note If the thing is nil or dead; does nothing.
     * @note If the thing does not have the specified part; does nothing.
     */
    template <typename T, typename RegistryT>
    void record_mutate(RegistryT &registry, Thing thing, const T &prev, const T &next) noexcept {
        if (thing.is_nil()) [[unlikely]] {
            return;
        }

        if (!registry.is_alive(thing)) [[unlikely]] {
            return;
        }

        if (!registry.template has<T>(thing)) [[unlikely]] {
            return;
        }

        do_ensure_cmd_storage<T, RegistryT>().mutate_cmds.push_back(
            MutatePartCmd<T>{.thing = thing, .prev = prev, .next = next});
    }

    // --------------------------------------------------------- Commit Commands

    /**
     * @brief Commits all destroy commands for all parts.
     * @tparam RegistryT Type of the registry.
     * @param registry Pointer to the registry.
     */
    template <typename RegistryT>
    void commit_destroy_all(RegistryT *registry) noexcept {
        for (USize i = 0; i < MAX_PARTS; ++i) {
            if (m_cmds[i].is_nil()) {
                continue;
            }

            if (m_commit_destroy_fns[i]) {
                m_commit_destroy_fns[i](static_cast<void *>(registry), m_cmds[i]);
            }
        }
    }

    /**
     * @brief Commits all insert commands for all parts.
     * @tparam RegistryT Type of the registry.
     * @param registry Pointer to the registry.
     */
    template <typename RegistryT>
    void commit_insert_all(RegistryT *registry) noexcept {
        for (USize i = 0; i < MAX_PARTS; ++i) {
            if (m_cmds[i].is_nil()) {
                continue;
            }

            if (m_commit_insert_fns[i]) {
                m_commit_insert_fns[i](static_cast<void *>(registry), m_cmds[i]);
            }
        }
    }

    /**
     * @brief Commits all mutate commands for all parts.
     * @tparam RegistryT Type of the registry.
     * @param registry Pointer to the registry.
     */
    template <typename RegistryT>
    void commit_mutate_all(RegistryT *registry) noexcept {
        for (USize i = 0; i < MAX_PARTS; ++i) {
            if (m_cmds[i].is_nil()) {
                continue;
            }

            if (m_commit_mutate_fns[i]) {
                m_commit_mutate_fns[i](static_cast<void *>(registry), m_cmds[i]);
            }
        }
    }

    // -------------------------------------------------------- Internal Methods

private:
    template <typename T, typename RegistryT>
    CmdStorage<T> &do_ensure_cmd_storage() noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        FR_ASSERT(tidx.idx() < MAX_PARTS, "invalid part type index");

        if (m_cmds[tidx.idx()].is_nil()) [[unlikely]] {
            do_create_cmd_storage<T, RegistryT>(tidx);
        }

        return m_cmds[tidx.idx()].cast_ref<CmdStorage<T>>();
    }

    template <typename T, typename RegistryT>
    void do_create_cmd_storage(TypeIdx tidx) noexcept {
        m_cmds[tidx.idx()].template emplace<CmdStorage<T>>(m_alloc);
        m_commit_destroy_fns[tidx.idx()] = &CmdPool::do_commit_destroy_cmds<T, RegistryT>;
        m_commit_insert_fns[tidx.idx()] = &CmdPool::do_commit_insert_cmds<T, RegistryT>;
        m_commit_mutate_fns[tidx.idx()] = &CmdPool::do_commit_mutate_cmds<T, RegistryT>;
    }

    template <typename T, typename RegistryT>
    static void do_commit_destroy_cmds(void *registry_ptr, AnyCmdStorage &cmds) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        CmdStorage<T> &typed_cmds = cmds.cast_ref<CmdStorage<T>>();

        for (const auto &cmd : typed_cmds.destroy_cmds) {
            registry->template destroy_checked<T>(cmd.thing);
        }

        typed_cmds.destroy_cmds.clear();
    }

    template <typename T, typename RegistryT>
    static void do_commit_insert_cmds(void *registry_ptr, AnyCmdStorage &cmds) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        CmdStorage<T> &typed_cmds = cmds.cast_ref<CmdStorage<T>>();

        for (const auto &cmd : typed_cmds.insert_cmds) {
            registry->template emplace_checked<T>(cmd.thing, cmd.part);
        }

        typed_cmds.insert_cmds.clear();
    }

    template <typename T, typename RegistryT>
    static void do_commit_mutate_cmds(void *registry_ptr, AnyCmdStorage &cmds) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        CmdStorage<T> &typed_cmds = cmds.cast_ref<CmdStorage<T>>();

        for (const auto &cmd : typed_cmds.mutate_cmds) {
            T *part = registry->template get_checked<T>(cmd.thing);
            if (part) [[likely]] {
                *part = cmd.next;
            }
        }

        typed_cmds.mutate_cmds.clear();
    }

    // -------------------------------------------------------- Member Variables
    Alloc *m_alloc{nullptr};
    Array<AnyCmdStorage, MAX_PARTS> m_cmds{};
    Array<CmdCommitFn, MAX_PARTS> m_commit_destroy_fns{};
    Array<CmdCommitFn, MAX_PARTS> m_commit_insert_fns{};
    Array<CmdCommitFn, MAX_PARTS> m_commit_mutate_fns{};
};

// ----------------------------------------------------------- Static Assertions

FR_STATIC_ASSERT(sizeof(CmdStorage<Byte>) == sizeof(CmdStorage<U64>),
                 "cmd storages must have the same size regardless of the part type");

FR_STATIC_ASSERT(alignof(CmdStorage<Byte>) == alignof(CmdStorage<U64>),
                 "cmd storages must have the same alignment regardless of the part type");
} // namespace fr::impl
