/**
 * @file meta.hpp
 * @author Kiju
 *
 * @brief Meta serves a purpose of a reflection-like system metadata for dynamically managed and
 * type-erased objects.
 */

#pragma once

#include <cstring>
#include <limits>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/json.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
struct TypeIdx;

template <typename T>
TypeIdx lookup_tidx() noexcept;

struct TypeIdx {
    using IDX = U32;

    TypeIdx() noexcept = default;
    static TypeIdx from_idx(IDX idx) noexcept {
        return TypeIdx(idx);
    }

    IDX idx() const noexcept {
        return m_idx;
    }

    static TypeIdx nil() noexcept {
        return TypeIdx(std::numeric_limits<IDX>::max());
    }

    bool is_nil() const noexcept {
        return m_idx == std::numeric_limits<IDX>::max();
    }

    bool operator==(const TypeIdx &other) const noexcept {
        return m_idx == other.m_idx;
    }

    template <typename T>
    static TypeIdx from_type() noexcept {
        static TypeIdx cached_tidx = TypeIdx::nil();

        if (!cached_tidx.is_nil()) [[likely]] {
            return cached_tidx;
        }

        cached_tidx = lookup_tidx<T>();
        return cached_tidx;
    }

    template <typename Archive>
    void shape(Archive &archive) noexcept {
        archive.prop("@tidx", m_idx);
    }

    template <typename Archive>
    void shape(Archive &archive) const noexcept {
        archive.prop("@tidx", m_idx);
    }


private:
    explicit TypeIdx(IDX idx) noexcept
        : m_idx(idx) {
    }

    IDX m_idx{nil().idx()};
};

struct TypeMeta {
    TypeIdx tidx{TypeIdx::nil()};
    USize size{0};
    USize alignment{0};
    const char *name{"@noname"};

    void (*construct)(void *) noexcept {nullptr};
    void (*destroy)(void *) noexcept {nullptr};

    void (*json_writer_shape)(JsonWriterArchive &, void *) noexcept {nullptr};
    void (*json_reader_shape)(JsonReaderArchive &, void *) noexcept {nullptr};

    template <typename Archive>
    void shape(Archive &archive) noexcept {
        archive.prop("tidx", tidx);
        archive.prop("size", size);
        archive.prop("alignment", alignment);
        archive.prop("name", name);
    }

    template <typename Archive>
    void shape(Archive &archive) const noexcept {
        archive.prop("tidx", tidx);
        archive.prop("size", size);
        archive.prop("alignment", alignment);
        archive.prop("name", name);
    }

};

template <typename T>
struct TypeInfo {
    FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);
    FR_STATIC_ASSERT_NOTHROW_DESTRUCTIBLE(T);
    FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);

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
        if constexpr (impl::HasMemberShape<JsonWriterArchive, T> ||
                      impl::HasADLShape<JsonWriterArchive, T>) {
            call_shape(archive, *(static_cast<T *>(ptr)));
        } else {
            FR_ASSERT(false, "T has to implement shape protocol");
        }
    }

    static void json_reader_shape(JsonReaderArchive &archive, void *ptr) noexcept {
        if constexpr (impl::HasMemberShape<JsonReaderArchive, T> ||
                      impl::HasADLShape<JsonReaderArchive, T>) {
            call_shape(archive, *(static_cast<T *>(ptr)));
        } else {
            FR_ASSERT(false, "T has to implement shape protocol");
        }
    }
};

class TypeRegistry {
    DynamicArray<TypeMeta> m_metas{};

public:
    explicit TypeRegistry(Alloc *alloc) noexcept
        : m_metas(alloc) {
    }

    TypeIdx record(const TypeMeta &info) noexcept {
        TypeIdx tidx = TypeIdx::from_idx(m_metas.size());
        m_metas.push_back(info);
        return tidx;
    }

    const TypeMeta &meta(TypeIdx tidx) const noexcept {
        FR_ASSERT(tidx.idx() < m_metas.size(), "invalid type index");
        return m_metas[tidx.idx()];
    }

    USize size() const noexcept {
        return m_metas.size();
    }

    template <typename T>
    TypeIdx gen_tidx(TypeMeta meta) noexcept {
        const char *name = meta.name;

        for (USize i = 0; i < m_metas.size(); ++i) {
            TypeMeta &m = m_metas[i];
            if (std::strcmp(m.name, name) == 0) {
                if (m.tidx.is_nil()) {
                    m.tidx = TypeIdx::from_idx(i);
                }
                return m.tidx;
            }
        }

        TypeIdx tidx = TypeIdx::from_idx(m_metas.size());
        meta.tidx = tidx;
        m_metas.push_back(meta);
        return tidx;
    }

    Slice<const TypeMeta> storage() const noexcept {
        return m_metas.slice();
    }
};

namespace impl {
template <typename T>
TypeIdx lookup_tidx_from_registry(TypeRegistry &registry) noexcept {
    return registry.gen_tidx<T>({
        .tidx = TypeIdx::nil(),
        .size = sizeof(T),
        .alignment = alignof(T),
        .name = TypeInfo<T>::name(),
        .construct = TypeInfo<T>::construct,
        .destroy = TypeInfo<T>::destroy,
        .json_writer_shape = TypeInfo<T>::json_writer_shape,
        .json_reader_shape = TypeInfo<T>::json_reader_shape,
    });
}
} // namespace impl

template <typename T>
TypeIdx lookup_tidx() noexcept {
    FR_ASSERT(get_ambient_ctx().type_registry, "no type registry on ambient ctx");
    return impl::lookup_tidx_from_registry<T>(*get_ambient_ctx().type_registry);
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
