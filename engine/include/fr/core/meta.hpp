/**
 * @file meta.hpp
 * @author Kiju
 *
 * @brief Metadata and registry utilities for a lightweight reflection-like system.
 * Provides type indices, type metadata, and a registry for dynamically managed
 * and type-erased objects. Includes hooks for construction, destruction, and
 * JSON shape-based serialization.
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

/**
 * @brief Forward declaration for type index lookup.
 */
template <typename T>
TypeIdx lookup_tidx() noexcept;

// ================================================================== Type Index

/**
 * @brief Small handle identifying a registered type.
 */
struct TypeIdx {
    /// @brief Underlying index type.
    using IDX = U32;

    /// @brief Default constructor (nil).
    TypeIdx() noexcept = default;

    /// @brief Create a TypeIdx from a raw index.
    static TypeIdx from_idx(IDX idx) noexcept {
        return TypeIdx(idx);
    }

    /// @brief Returns the underlying index.
    IDX idx() const noexcept {
        return m_idx;
    }

    /// @brief Returns the nil index value.
    static TypeIdx nil() noexcept {
        return TypeIdx(std::numeric_limits<IDX>::max());
    }

    /// @brief Returns true if this is nil.
    bool is_nil() const noexcept {
        return m_idx == std::numeric_limits<IDX>::max();
    }

    /// @brief Equality comparison.
    bool operator==(const TypeIdx &other) const noexcept {
        return m_idx == other.m_idx;
    }

    /**
     * @brief Returns the cached type index for T (first lookup per TU).
     */
    template <typename T>
    static TypeIdx from_type() noexcept {
        static TypeIdx cached_tidx = TypeIdx::nil();

        if (!cached_tidx.is_nil()) [[likely]] {
            return cached_tidx;
        }

        cached_tidx = lookup_tidx<T>();
        return cached_tidx;
    }

    /// @brief Shape protocol for serialization.
    FR_SHAPE(FR_PROP("@tidx", m_idx);)

private:
    /// @brief Private constructor for raw index.
    explicit TypeIdx(IDX idx) noexcept
        : m_idx(idx) {
    }

    /// @brief Stored index (nil by default).
    IDX m_idx{nil().idx()};
};

// =============================================================== Type Metadata

/**
 * @brief Metadata describing a registered type.
 */
struct TypeMeta {
    /// @brief Assigned type index.
    TypeIdx tidx{TypeIdx::nil()};

    /// @brief Size of the type in bytes.
    USize size{0};

    /// @brief Alignment of the type in bytes.
    USize alignment{0};

    /// @brief Human-readable name of the type.
    const char *name{"@noname"};

    /// @brief Default constructor hook.
    void (*construct)(void *) noexcept {nullptr};

    /// @brief Destructor hook.
    void (*destroy)(void *) noexcept {nullptr};

    /// @brief Shape writer hook (JSON).
    void (*json_writer_shape)(JsonWriterArchive &, void *) noexcept {nullptr};

    /// @brief Shape reader hook (JSON).
    void (*json_reader_shape)(JsonReaderArchive &, void *) noexcept {nullptr};

    /// @brief Shape protocol for serialization.
    FR_SHAPE(FR_PROP(tidx); FR_PROP(size); FR_PROP(alignment); FR_PROP(name);)
};

// =========================================================== Type Info Helpers

/**
 * @brief Default type information for a concrete T.
 */
template <typename T>
struct TypeInfo {
    // @todo Removed for now, scripts...
    // FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);
    // FR_STATIC_ASSERT_NOTHROW_DESTRUCTIBLE(T);
    // FR_STATIC_ASSERT_NOTHROW_DEFAULT_CONSTRUCTIBLE(T);

    /// @brief Returns the compiler-provided type name.
    static const char *name() noexcept {
        return typeid(T).name();
    }

    /// @brief Default placement-construct a T at the given address.
    static void construct(void *ptr) noexcept {
        new (ptr) T();
    }

    /// @brief Destroy a T at the given address.
    static void destroy(void *ptr) noexcept {
        static_cast<T *>(ptr)->~T();
    }

    /// @brief Serialize T via shape protocol (writer).
    static void json_writer_shape(JsonWriterArchive &archive, void *ptr) noexcept {
        if constexpr (impl::HasMemberShape<JsonWriterArchive, T> ||
                      impl::HasADLShape<JsonWriterArchive, T>) {
            call_shape(archive, *(static_cast<T *>(ptr)));
        } else {
            FR_ASSERT(false, "T has to implement shape protocol");
        }
    }

    /// @brief Deserialize T via shape protocol (reader).
    static void json_reader_shape(JsonReaderArchive &archive, void *ptr) noexcept {
        if constexpr (impl::HasMemberShape<JsonReaderArchive, T> ||
                      impl::HasADLShape<JsonReaderArchive, T>) {
            call_shape(archive, *(static_cast<T *>(ptr)));
        } else {
            FR_ASSERT(false, "T has to implement shape protocol");
        }
    }
};

// =============================================================== Type Registry

/**
 * @brief Registry storing metadata for known types.
 */
class TypeRegistry {
    /// @brief Storage for registered type metadata.
    DynamicArray<TypeMeta> m_metas{};

public:
    /// @brief Construct a registry using a specific allocator.
    explicit TypeRegistry(Alloc *alloc) noexcept
        : m_metas(alloc) {
    }

    /// @brief Record metadata and return its assigned type index.
    TypeIdx record(const TypeMeta &info) noexcept {
        TypeIdx tidx = TypeIdx::from_idx(m_metas.size());
        m_metas.push_back(info);
        return tidx;
    }

    /// @brief Access metadata for a given type index.
    const TypeMeta &meta(TypeIdx tidx) const noexcept {
        FR_ASSERT(tidx.idx() < m_metas.size(), "invalid type index");
        return m_metas[tidx.idx()];
    }

    /// @brief Number of registered types.
    USize size() const noexcept {
        return m_metas.size();
    }

    /// @brief Check if a type index if valid.
    bool check(TypeIdx tidx) const noexcept {
        return tidx.idx() < m_metas.size();
    }

    /**
     * @brief Generate or fetch a stable type index for T.
     */
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

    /// @brief Returns a read-only view of the stored metadata.
    Slice<const TypeMeta> storage() const noexcept {
        return m_metas.slice();
    }
};

// ============================================================ Internal Helpers

namespace impl {
/**
 * @brief Build or fetch a type index using a specific registry instance.
 */
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

// ====================================================================== Lookup

/**
 * @brief Lookup the type index for T using the ambient registry.
 */
template <typename T>
TypeIdx lookup_tidx() noexcept {
    FR_ASSERT(get_ambient_ctx().type_registry, "no type registry on ambient ctx");
    return impl::lookup_tidx_from_registry<T>(*get_ambient_ctx().type_registry);
}
} // namespace fr

// ====================================================================== Macros

/**
 * @brief Override TypeInfo for a given type with a stable name.
 */
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
