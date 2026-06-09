/**
 * @file resource.hpp
 * @author Kiju
 *
 * @brief Resources are a way to create and manage data shared across scripts and systems.
 */

#pragma once

#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/part.hpp"

namespace fr::impl {
class ResourcePool {
    Alloc *m_alloc{get_ambient_ctx().alloc};
    Array<void *, MAX_PARTS> m_resources{Array<void *, MAX_PARTS>::from_repeated(nullptr)};

public:
    ResourcePool() noexcept = default;
    ResourcePool(Alloc *alloc) noexcept
        : m_alloc(alloc) {
    }

    ResourcePool(const ResourcePool &) = delete;
    ResourcePool &operator=(const ResourcePool &) = delete;
    ResourcePool(ResourcePool &&) noexcept = default;
    ResourcePool &operator=(ResourcePool &&) noexcept = default;

    ~ResourcePool() noexcept {
        for (TypeIdx::IDX tidx = 0; tidx < MAX_PARTS; ++tidx) {
            if (m_resources[tidx]) {
                TypeMeta meta = TypeIdx::from_idx(tidx).meta();
                meta.destroy(m_resources[tidx]);
                m_alloc->deallocate(m_resources[tidx], meta.size, meta.alignment);
            }
        }
    }

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
    T &emplace(Args &&...args) noexcept {
        TypeIdx tidx = TypeIdx::from_type<T>();
        const TypeMeta &meta = tidx.meta();
        USize idx = tidx.idx();

        if (m_resources[idx]) {
            meta.destroy(m_resources[idx]);
            m_alloc->deallocate(m_resources[idx], meta.size, meta.alignment);
        }

        T *ptr = static_cast<T *>(m_alloc->allocate(meta.size, meta.alignment));

        new (ptr) T(std::forward<Args>(args)...);
        m_resources[idx] = ptr;
        return *ptr;
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
    void insert(T &&t) noexcept {
        emplace<T>(std::forward<T>(t));
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
    void insert(const T &t) noexcept {
        emplace<T>(t);
    }

    /**
     * @brief Inserts a raw copy of a resource `T` into the pool.
     *
     * @param tidx The type index of the resource to insert.
     * @param ptr The pointer to the resource to insert.
     *
     * @note If a resource of type index `tidx` already exists, it will be destroyed and replaced
     * with the new resource.
     */
    void insert_raw_copy(TypeIdx tidx, void *ptr) {
        USize idx = tidx.idx();
        const TypeMeta &meta = tidx.meta();

        if (m_resources[idx] == nullptr) {
            m_resources[idx] = m_alloc->allocate(meta.size, meta.alignment);
        } else {
            meta.destroy(m_resources[idx]);
        }

        meta.copy_construct(m_resources[idx], ptr);
    }

    /**
     * @brief Inserts a raw move of a resource `T` into the pool.
     *
     * @param tidx The type index of the resource to insert.
     * @param ptr The pointer to the resource to insert.
     *
     * @note If a resource of type index `tidx` already exists, it will be destroyed and replaced
     * with the new resource.
     */
    void insert_raw_move(TypeIdx tidx, void *ptr) {
        USize idx = tidx.idx();
        const TypeMeta &meta = tidx.meta();

        if (m_resources[idx] == nullptr) {
            m_resources[idx] = m_alloc->allocate(meta.size, meta.alignment);
        } else {
            meta.destroy(m_resources[idx]);
        }

        meta.move_construct(m_resources[idx], ptr);
    }

    /**
     * @brief Destroys a resource of type `T` from the pool.
     * @tparam T The type of the resource to destroy.
     * @return `true` if the resource was destroyed, `false` if it was not found.
     */
    template <typename T>
    bool destroy() {
        TypeIdx tidx = TypeIdx::from_type<T>();
        return destroy_raw(tidx);
    }

    /**
     * @brief Destroys a raw resource of type index `tidx` from the pool.
     * @param tidx The type index of the resource to destroy.
     * @return `true` if the resource was destroyed, `false` if it was not found.
     */
    bool destroy_raw(TypeIdx tidx) {
        USize idx = tidx.idx();
        const TypeMeta &meta = tidx.meta();

        if (m_resources[idx] == nullptr) {
            return false;
        }

        meta.destroy(m_resources[idx]);
        m_alloc->deallocate(m_resources[idx], meta.size, meta.alignment);
        m_resources[idx] = nullptr;
        return true;
    }

    /**
     * @brief Checks if a resource of type `T` exists in the pool.
     * @tparam T The type of the resource to check.
     * @return `true` if the resource exists, `false` otherwise.
     */
    template <typename T>
    bool check() {
        TypeIdx tidx = TypeIdx::from_type<T>();
        return check_raw(tidx);
    }

    /**
     * @brief Checks if a raw resource of type index `tidx` exists in the pool.
     * @param tidx The type index of the resource to check.
     * @return `true` if the resource exists, `false` otherwise.
     */
    bool check_raw(TypeIdx tidx) {
        USize idx = tidx.idx();
        return m_resources[idx] != nullptr;
    }

    /**
     * @brief Tries to get a resource of type `T` from the pool.
     * @tparam T The type of the resource to get.
     * @return A pointer to the resource if it exists, `nullptr` otherwise.
     */
    template <typename T>
    T *try_get() {
        TypeIdx tidx = TypeIdx::from_type<T>();
        return static_cast<T *>(try_get_raw(tidx));
    }

    /**
     * @brief Tries to get a raw resource of type index `tidx` from the pool.
     * @param tidx The type index of the resource to get.
     * @return A pointer to the resource if it exists, `nullptr` otherwise.
     */
    void *try_get_raw(TypeIdx tidx) {
        USize idx = tidx.idx();
        return m_resources[idx];
    }

    /**
     * @brief Gets a resource of type `T` from the pool.
     * @tparam T The type of the resource to get.
     * @return A reference to the resource.
     */
    template <typename T>
    T &get() {
        TypeIdx tidx = TypeIdx::from_type<T>();
        return *static_cast<T *>(get_raw(tidx));
    }

    /**
     * @brief Gets a raw resource of type index `tidx` from the pool.
     * @param tidx The type index of the resource to get.
     * @return A pointer to the resource if it exists, `nullptr` otherwise.
     */
    void *get_raw(TypeIdx tidx) {
        void *ptr = try_get_raw(tidx);
        FR_ASSERT(ptr, "resource not found");
        return ptr;
    }

    /// @brief Returns the capacity of the resource pool.
    USize capacity() const {
        return m_resources.size();
    }

    /// @brief Returns the number of resources in the pool.
    USize count() const {
        USize n = 0;

        for (void *r : m_resources) {
            if (r) {
                ++n;
            }
        }

        return n;
    }

    // --------------------------------------------------------------- Shape

    void shape(JsonWriterArchive &archive) noexcept {
        USize n = count();

        archive.prop("@count", n);
        archive.list("@items", [&](JsonWriterArchive &la) {
            for (TypeIdx::IDX i = 0; i < MAX_PARTS; ++i) {
                if (!m_resources[i]) {
                    continue;
                }

                TypeIdx tidx = TypeIdx::from_idx(i);
                const TypeMeta &meta = tidx.meta();
                if (!meta.json_writer_shape) {
                    continue;
                }

                const char *type_name = meta.name;
                la.dict("", [&](JsonWriterArchive &ea) {
                    ea.prop("@typename", type_name);
                    meta.json_writer_shape(ea, m_resources[i]);
                });
            }
        });
    }

    void shape(JsonReaderArchive &archive) noexcept {
        archive.list("@items", [&](JsonReaderArchive &la) {
            USize n = la.current_list_size();

            for (USize i = 0; i < n; ++i) {
                la.dict("", [&](JsonReaderArchive &ea) {
                    StringView type_name;
                    ea.prop("@typename", type_name);

                    TypeIdx tidx = do_find_by_type_name(type_name);
                    if (tidx.is_nil()) {
                        return;
                    }

                    const TypeMeta &meta = tidx.meta();
                    if (!meta.json_reader_shape || !meta.construct) {
                        return;
                    }

                    USize idx = tidx.idx();
                    if (m_resources[idx]) {
                        meta.destroy(m_resources[idx]);
                    } else {
                        m_resources[idx] = m_alloc->allocate(meta.size, meta.alignment);
                    }

                    meta.construct(m_resources[idx]);
                    meta.json_reader_shape(ea, m_resources[idx]);
                });
            }
        });
    }

private:
    static TypeIdx do_find_by_type_name(StringView name) noexcept {
        const TypeRegistry *reg = get_ambient_ctx().type_registry;
        Slice<const TypeMeta> storage = reg->storage();

        for (USize i = 0; i < storage.size(); ++i) {
            const char *n = storage[i].name;
            if (name == StringView{n, std::strlen(n)}) {
                return storage[i].tidx;
            }
        }

        return TypeIdx::nil();
    }
};
} // namespace fr::impl
