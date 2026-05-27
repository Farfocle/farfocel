/**
 * @file cmd.hpp
 * @author Kiju
 *
 * @brief Commands for the data layer.
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

/**
 * @brief Kind of command that targets a part.
 */
enum class CmdKind : U8 { DestroyPart, InsertPart, MutatePart };

/**
 * @brief Command to destroy a part owned by a thing.
 * @tparam T Part type.
 */
template <typename T>
struct DestroyPartCmd {
    /// @brief Part alias for tooling and reflection.
    using Part = T;

    /// @brief Target thing.
    Thing thing;
};

/**
 * @brief Command to insert a part for a thing.
 * @tparam T Part type.
 */
template <typename T>
struct InsertPartCmd {
    /// @brief Part alias for tooling and reflection.
    using Part = T;

    /// @brief Target thing.
    Thing thing;

    /// @brief Part payload.
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

namespace fr::impl {

template <typename T>
struct CmdStorage {
    DynamicArray<DestroyPartCmd<T>> destroy_cmds{};
    DynamicArray<InsertPartCmd<T>> insert_cmds{};
    DynamicArray<MutatePartCmd<T>> mutate_cmds{};

    CmdStorage() noexcept
        : CmdStorage(get_ambient_ctx().alloc) {
    }

    explicit CmdStorage(Alloc *alloc) noexcept {
        destroy_cmds = DynamicArray<DestroyPartCmd<T>>::with_alloc(alloc);
        insert_cmds = DynamicArray<InsertPartCmd<T>>::with_alloc(alloc);
        mutate_cmds = DynamicArray<MutatePartCmd<T>>::with_alloc(alloc);
    }
};

class CmdPool {
public:
    using AnyCmdStorage = InlineAny<sizeof(CmdStorage<Byte>), alignof(CmdStorage<Byte>)>;
    using CmdCommitFn = void (*)(void *, AnyCmdStorage &) noexcept;

    CmdPool() noexcept
        : CmdPool(get_ambient_ctx().alloc) {
    }

    explicit CmdPool(Alloc *alloc) noexcept
        : m_alloc(alloc) {
    }

    CmdPool(const CmdPool &) = delete;
    CmdPool(CmdPool &&) = delete;
    CmdPool &operator=(const CmdPool &) = delete;
    CmdPool &operator=(CmdPool &&) = delete;

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
            registry->template try_destroy<T>(cmd.thing);
        }

        typed_cmds.destroy_cmds.clear();
    }

    template <typename T, typename RegistryT>
    static void do_commit_insert_cmds(void *registry_ptr, AnyCmdStorage &cmds) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        CmdStorage<T> &typed_cmds = cmds.cast_ref<CmdStorage<T>>();

        for (const auto &cmd : typed_cmds.insert_cmds) {
            registry->template try_insert<T>(cmd.thing, cmd.part);
        }

        typed_cmds.insert_cmds.clear();
    }

    template <typename T, typename RegistryT>
    static void do_commit_mutate_cmds(void *registry_ptr, AnyCmdStorage &cmds) noexcept {
        RegistryT *registry = static_cast<RegistryT *>(registry_ptr);
        CmdStorage<T> &typed_cmds = cmds.cast_ref<CmdStorage<T>>();

        for (const auto &cmd : typed_cmds.mutate_cmds) {
            T *part = registry->template try_get<T>(cmd.thing);
            if (part) [[likely]] {
                *part = cmd.next;
            }
        }

        typed_cmds.mutate_cmds.clear();
    }

    Alloc *m_alloc{nullptr};
    Array<AnyCmdStorage, MAX_PARTS> m_cmds{};
    Array<CmdCommitFn, MAX_PARTS> m_commit_destroy_fns{};
    Array<CmdCommitFn, MAX_PARTS> m_commit_insert_fns{};
    Array<CmdCommitFn, MAX_PARTS> m_commit_mutate_fns{};
};

FR_STATIC_ASSERT(sizeof(CmdStorage<Byte>) == sizeof(CmdStorage<U64>),
                 "cmd storages must have the same size regardless of the part type");

FR_STATIC_ASSERT(alignof(CmdStorage<Byte>) == alignof(CmdStorage<U64>),
                 "cmd storages must have the same alignment regardless of the part type");
} // namespace fr::impl
