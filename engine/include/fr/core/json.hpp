/**
 * @file json.hpp
 * @author Kiju
 *
 * @brief JSON serialization and desarialization.
 */

#pragma once

#include <cstdlib>
#include <type_traits>
#include <utility>

#include <yyjson.h>

#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash.hpp"     // IWYU pragma: keep
#include "fr/core/hash_map.hpp" // IWYU pragma: keep
#include "fr/core/hash_set.hpp" // IWYU pragma: keep
#include "fr/core/macros.hpp"
#include "fr/core/optional.hpp" // IWYU pragma: keep
#include "fr/core/pair.hpp"     // IWYU pragma: keep
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/tuple.hpp" // IWYU pragma: keep

namespace fr {

class JsonSerializer {
public:
    struct Options {
        bool types{false};
        bool pretty{false};
    };

    enum class State { Init, Serializing, Error };

    static constexpr ArchiveKind kind = ArchiveKind::Serializer;

    JsonSerializer()
        : JsonSerializer(Options{}) {};

    JsonSerializer(Options options)
        : m_options(options) {
        do_init_doc();
    }

    JsonSerializer(const JsonSerializer &) = delete;
    JsonSerializer &operator=(const JsonSerializer &) = delete;
    JsonSerializer(JsonSerializer &&) = delete;
    JsonSerializer &operator=(JsonSerializer &&) = delete;

    ~JsonSerializer() {
        do_cleanup();
    }

public:
    String consume() {
        String output{};
        if (m_state != State::Error && m_doc) {
            const yyjson_write_flag flags =
                m_options.pretty ? YYJSON_WRITE_PRETTY : YYJSON_WRITE_NOFLAG;

            USize len = 0;
            char *data = yyjson_mut_write(m_doc, flags, &len);
            if (data) {
                output = String(data, len);
                std::free(data);
            } else {
                m_state = State::Error;
            }
        }

        reset();
        return output;
    }

    void reset() {
        do_cleanup();
        do_init_doc();
    }

    State state() const {
        return m_state;
    }

    template <typename V>
    void prop(StringView name, V &value) {
        if (m_state == State::Error) {
            return;
        }

        if (!do_begin_write()) {
            return;
        }

        using RawT = std::remove_cvref_t<V>;

        if constexpr (std::is_same_v<RawT, bool>) {
            do_add_value(name, yyjson_mut_bool(m_doc, value));
        } else if constexpr (std::is_same_v<RawT, U8>) {
            do_add_value(name, yyjson_mut_uint(m_doc, static_cast<U64>(value)));
        } else if constexpr (std::is_same_v<RawT, U16>) {
            do_add_value(name, yyjson_mut_uint(m_doc, static_cast<U64>(value)));
        } else if constexpr (std::is_same_v<RawT, U32>) {
            do_add_value(name, yyjson_mut_uint(m_doc, static_cast<U64>(value)));
        } else if constexpr (std::is_same_v<RawT, U64>) {
            do_add_value(name, yyjson_mut_uint(m_doc, value));
        } else if constexpr (std::is_same_v<RawT, S8>) {
            do_add_value(name, yyjson_mut_sint(m_doc, static_cast<S64>(value)));
        } else if constexpr (std::is_same_v<RawT, S16>) {
            do_add_value(name, yyjson_mut_sint(m_doc, static_cast<S64>(value)));
        } else if constexpr (std::is_same_v<RawT, S32>) {
            do_add_value(name, yyjson_mut_sint(m_doc, static_cast<S64>(value)));
        } else if constexpr (std::is_same_v<RawT, S64>) {
            do_add_value(name, yyjson_mut_sint(m_doc, value));
        } else if constexpr (std::is_same_v<RawT, F32>) {
            do_add_value(name, yyjson_mut_real(m_doc, static_cast<F64>(value)));
        } else if constexpr (std::is_same_v<RawT, F64>) {
            do_add_value(name, yyjson_mut_real(m_doc, value));
        } else if constexpr (std::is_same_v<RawT, String>) {
            StringView view = value.view();
            do_add_value(name, yyjson_mut_strncpy(m_doc, view.data(), view.size()));
        } else if constexpr (std::is_same_v<RawT, StringView>) {
            do_add_value(name, yyjson_mut_strncpy(m_doc, value.data(), value.size()));
        } else if constexpr (IsShape<JsonSerializer, V>) {
            yyjson_mut_val *obj = yyjson_mut_obj(m_doc);
            if (!obj) {
                m_state = State::Error;
                return;
            }

            do_add_value(name, obj);
            m_stack.push_back(obj);

            constexpr StringView k_typename = impl::get_typename<V>();

            if (m_options.types) {
                do_add_value(StringView("@typename"),
                             yyjson_mut_strncpy(m_doc, k_typename.data(), k_typename.size()));
            }

            call_shape<JsonSerializer, V>(*this, value);
            m_stack.pop_back();
        } else {
            FR_STATIC_ASSERT(false, "Object does not implement the shape protocol");
        }
    }

    template <typename Fn>
    void list(StringView name, Fn &&fn) {
        if (m_state == State::Error) {
            return;
        }

        if (!do_begin_write()) {
            return;
        }

        yyjson_mut_val *arr = yyjson_mut_arr(m_doc);
        if (!arr) {
            m_state = State::Error;
            return;
        }

        do_add_value(name, arr);
        if (m_state == State::Error) {
            return;
        }

        m_stack.push_back(arr);
        std::forward<Fn>(fn)(*this);
        m_stack.pop_back();
    }

    template <typename Fn>
    void dict(StringView name, Fn &&fn) {
        if (m_state == State::Error) {
            return;
        }

        if (!do_begin_write()) {
            return;
        }

        yyjson_mut_val *obj = yyjson_mut_obj(m_doc);
        if (!obj) {
            m_state = State::Error;
            return;
        }

        do_add_value(name, obj);
        if (m_state == State::Error) {
            return;
        }

        m_stack.push_back(obj);
        std::forward<Fn>(fn)(*this);
        m_stack.pop_back();
    }

private:
    bool do_begin_write() {
        FR_ASSERT(m_state != State::Error, "serializer in error state");
        if (!m_doc || !m_root) {
            m_state = State::Error;
            return false;
        }
        if (m_state == State::Init) {
            m_state = State::Serializing;
        }
        return true;
    }

    void do_init_doc() {
        m_state = State::Init;
        m_doc = yyjson_mut_doc_new(nullptr);
        if (!m_doc) {
            m_root = nullptr;
            m_state = State::Error;
            return;
        }

        m_root = yyjson_mut_obj(m_doc);
        if (!m_root) {
            m_state = State::Error;
            return;
        }

        yyjson_mut_doc_set_root(m_doc, m_root);
        m_stack.push_back(m_root);
    }

    void do_cleanup() {
        if (m_doc) {
            yyjson_mut_doc_free(m_doc);
        }

        m_doc = nullptr;
        m_root = nullptr;
        m_stack.clear();
    }

    void do_add_value(StringView name, yyjson_mut_val *val) {
        if (!m_doc || !val) {
            m_state = State::Error;
            return;
        }

        yyjson_mut_val *parent = do_get_curr_obj();
        if (!parent) {
            m_state = State::Error;
            return;
        }

        if (yyjson_mut_is_arr(parent)) {
            yyjson_mut_arr_append(parent, val);
        } else {
            FR_ASSERT(!name.is_empty(), "property name must not be empty for object entries");
            yyjson_mut_val *key = yyjson_mut_strncpy(m_doc, name.data(), name.size());
            if (!key) {
                m_state = State::Error;
                return;
            }
            yyjson_mut_obj_add(parent, key, val);
        }
    }

    yyjson_mut_val *do_get_curr_obj() {
        return m_stack.is_empty() ? m_root : m_stack.back();
    }

private:
    yyjson_mut_doc *m_doc{nullptr};
    yyjson_mut_val *m_root{nullptr};

    DynamicArray<yyjson_mut_val *> m_stack{};
    State m_state{State::Init};
    const Options m_options;
};
} // namespace fr
