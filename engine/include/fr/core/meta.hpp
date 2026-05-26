/**
 * @file meta.hpp
 * @author Kiju
 *
 * @brief Meta serves a purpose of a reflection-like system metadata for dynamically managed and
 * type-erased objects.
 */

#pragma once

#include <cstring>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/json.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/typeidx.hpp"

namespace fr {
struct TypeMeta {
    TypeIdx tidx{0};
    USize size{0};
    USize aligment{0};
    const char *name{"@noname"};

    void (*construct)(void *) noexcept {nullptr};
    void (*destroy)(void *) noexcept {nullptr};

    void (*json_writer_shape)(JsonWriterArchive &, void *) noexcept {nullptr};
    void (*json_reader_shape)(JsonReaderArchive &, void *) noexcept {nullptr};
};

template <typename T>
struct TypeInfo {
    FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);
    FR_STATIC_ASSERT_NOTHROW_DESTRUCTIBLE(T);
    FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);
    FR_STATIC_ASSERT((impl::HasMemberShape<JsonWriterArchive, T> ||
                      impl::HasADLShape<JsonWriterArchive, T>),
                     "T has to implement shape protocol");

    static const char *name() noexcept {
        return typeid(T).name();
    }

    static void construct(void *ptr) noexcept {
        new (ptr) T();
    }

    static void destroy(void *ptr) noexcept {
        static_cast<T *>(ptr)->~T();
    }

    static void json_writer_shape(JsonWriterArchive &archive, void *ptr) noexcept {
        call_shape(archive, *(static_cast<T *>(ptr)));
    }

    static void json_reader_shape(JsonReaderArchive &archive, void *ptr) noexcept {
        call_shape(archive, *(static_cast<T *>(ptr)));
    }
};

class TypeRegistry {
    DynamicArray<TypeMeta> m_metas{};

public:
    explicit TypeRegistry(Alloc *alloc) noexcept
        : m_metas(alloc) {
    }

    TypeIdx record(const TypeMeta &info) noexcept {
        TypeIdx tidx = static_cast<TypeIdx>(m_metas.size());
        m_metas.push_back(info);
        return tidx;
    }

    const TypeMeta &meta(TypeIdx tidx) const noexcept {
        FR_ASSERT(tidx < m_metas.size(), "invalid type index");
        return m_metas[tidx];
    }

    USize size() const noexcept {
        return m_metas.size();
    }

    template <typename T>
    TypeIdx gen_tidx(TypeMeta meta) noexcept {
        const char *name = meta.name;

        for (TypeMeta &m : m_metas) {
            if (std::strcmp(meta.name, name) == 0) {
                return m.tidx;
            }
        }

        TypeIdx tidx = static_cast<TypeIdx>(m_metas.size());
        m_metas.push_back(meta);
        return tidx;
    }

    Slice<const TypeMeta> storage() const noexcept {
        return m_metas.slice();
    }
};

template <typename T>
TypeIdx type_idx_from_registry(TypeRegistry &registry) noexcept {
    return registry.gen_tidx<T>({
        .id = 0,
        .size = sizeof(T),
        .alignment = alignof(T),
        .name = TypeInfo<T>::name(),
        .construct = TypeInfo<T>::construct,
        .destroy = TypeInfo<T>::destroy,
        .json_writer_shape = TypeInfo<T>::json_writer_shape,
        .json_reader_shape = TypeInfo<T>::json_reader_shape,
    });
}

template <typename T>
TypeIdx type_idx() noexcept {
    FR_ASSERT(get_ambient_ctx().type_registry, "no type registry on ambient ctx");
    return type_idx_from_registry<T>(*get_ambient_ctx().type_registry);
}
} // namespace fr

#define FR_TYPE(T)                                                                                 \
    template <>                                                                                    \
    struct fr::TypeInfo<T> {                                                                       \
        static const char *name() noexcept {                                                       \
            return #T;                                                                             \
        }                                                                                          \
        static void construct(void *ptr) noexcept {                                                \
            new (ptr) T();                                                                         \
        }                                                                                          \
        static void destroy(void *ptr) noexcept {                                                  \
            static_cast<T *>(ptr)->~T();                                                           \
        }                                                                                          \
        static void json_writer_shape(fr::JsonWriterArchive &archive, void *ptr) noexcept {        \
            call_shape(archive, *(static_cast<T *>(ptr)));                                         \
        }                                                                                          \
        static void json_reader_shape(fr::JsonReaderArchive &archive, void *ptr) noexcept {        \
            call_shape(archive, *(static_cast<T *>(ptr)));                                         \
        }                                                                                          \
    }
