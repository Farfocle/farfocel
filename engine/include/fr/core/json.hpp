/**
 * @file json.hpp
 * @author Kiju
 * @brief JSON serialization and deserialization utilizing the shape protocol and yyjson.
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

/**
 * @brief Serializer for converting C++ objects into JSON strings.
 *
 * This class implements the Archive concept for serialization. It uses the 'shape'
 * protocol to reflect object structures.
 *
 * Example usage:
 * @code
 * struct MyData {
 *     U32 id;
 *     String name;
 *     void shape(auto &archive) {
 *         archive.prop("id", id);
 *         archive.prop("name", name);
 *     }
 * };
 *
 * MyData data{1, "Farfocel"};
 * JsonSerializer writer({.pretty = true});
 * writer.prop("root", data);
 * String json = writer.consume();
 * @endcode
 */
class JsonSerializer {
public:
    /**
     * @brief Serialization configuration options.
     */
    struct Options {
        bool types{false};  ///< If true, includes "@typename" property in objects.
        bool pretty{false}; ///< If true, outputs formatted JSON with indentation.
    };

    /**
     * @brief Internal state of the serializer.
     */
    enum class State { Init, Serializing, Error };

    static constexpr ArchiveKind kind = ArchiveKind::Serializer;

    /**
     * @brief Construct a serializer with default options.
     */
    JsonSerializer()
        : JsonSerializer(Options{}) {};

    /**
     * @brief Construct a serializer with specific options.
     * @param options Configuration for the serializer.
     */
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
    /**
     * @brief Finalize serialization and return the resulting JSON string.
     *
     * This method resets the serializer to its initial state.
     * @return Serialized JSON string or an empty string on error.
     */
    String consume() {
        String output{};
        if (m_state != State::Error && m_doc) {
            const yyjson_write_flag flags =
                m_options.pretty ? YYJSON_WRITE_PRETTY : YYJSON_WRITE_NOFLAG;

            USize len = 0;
            char *data = yyjson_mut_write(m_doc, flags, &len);
            if (data) {
                output = String::from_sized_chars(data, len);
                std::free(data);
            } else {
                m_state = State::Error;
            }
        }

        reset();
        return output;
    }

    /**
     * @brief Reset the serializer to start a new serialization session.
     */
    void reset() {
        do_cleanup();
        do_init_doc();
    }

    /**
     * @brief Returns the current state of the serializer.
     * @return Current state.
     */
    State state() const {
        return m_state;
    }

    /**
     * @brief Serializes a property with a given name.
     *
     * @tparam V Value type.
     * @param name Key name for the property.
     * @param value Reference to the value to serialize.
     */
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
        } else if constexpr (std::is_same_v<RawT, const char *> || std::is_same_v<RawT, char *>) {
            if (value) {
                do_add_value(name, yyjson_mut_strcpy(m_doc, value));
            } else {
                do_add_value(name, yyjson_mut_null(m_doc));
            }
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

    /**
     * @brief Starts a list (JSON array) context.
     *
     * @tparam Fn Callback function type.
     * @param name Key name for the array.
     * @param fn Function to execute within the array context.
     */
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

    /**
     * @brief Starts a dictionary (JSON object) context.
     *
     * @tparam Fn Callback function type.
     * @param name Key name for the object.
     * @param fn Function to execute within the object context.
     */
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

    /**
     * @brief Returns the size of the current list being serialized.
     * @return Always 0 for serializer.
     */
    USize current_list_size() const {
        return 0;
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

/**
 * @brief Deserializer for populating C++ objects from JSON strings.
 *
 * This class implements the Archive concept for deserialization. It uses the shape
 * protocol to populate object members from JSON data.
 *
 * Example usage:
 * @code
 * MyData data;
 * JsonDeserializer reader(json_string);
 * reader.prop("root", data);
 * if (reader.consume()) {
 *     // Data successfully populated
 * }
 * @endcode
 */
class JsonDeserializer {
public:
    /**
     * @brief Internal state of the deserializer.
     */
    enum class State { Init, Deserializing, Error };

    static constexpr ArchiveKind kind = ArchiveKind::Deserializer;

    /**
     * @brief Construct a deserializer from a JSON string.
     * @param json JSON string to parse.
     */
    JsonDeserializer(StringView json) {
        m_json_doc = yyjson_read(json.data(), json.size(), 0);
        if (!m_json_doc) {
            m_state = State::Error;
            return;
        }

        m_json_root = yyjson_doc_get_root(m_json_doc);
        if (!m_json_root) {
            m_state = State::Error;
            return;
        }

        m_stack.push_back(m_json_root);
        m_indices.push_back(0);
        m_state = State::Deserializing;
    }

    JsonDeserializer(const JsonDeserializer &) = delete;
    JsonDeserializer &operator=(const JsonDeserializer &) = delete;
    JsonDeserializer(JsonDeserializer &&) = delete;
    JsonDeserializer &operator=(JsonDeserializer &&) = delete;

    ~JsonDeserializer() {
        if (m_json_doc) {
            yyjson_doc_free(m_json_doc);
        }
    }

public:
    /**
     * @brief Finalize deserialization and return success status.
     * @return True if deserialization was successful and no errors occurred.
     */
    bool consume() {
        return m_state != State::Error;
    }

    /**
     * @brief Returns the current state of the deserializer.
     * @return Current state.
     */
    State state() const {
        return m_state;
    }

    /**
     * @brief Deserializes a property by name into a value.
     *
     * @tparam V Value type.
     * @param name Key name for the property.
     * @param value Reference to the value to populate.
     */
    template <typename V>
    void prop(StringView name, V &value) {
        if (m_state == State::Error) {
            return;
        }

        yyjson_val *val = do_get_value(name);
        if (!val) {
            return;
        }

        using RawT = std::remove_cvref_t<V>;

        if constexpr (std::is_same_v<RawT, bool>) {
            value = yyjson_get_bool(val);
        } else if constexpr (std::is_same_v<RawT, U8>) {
            value = static_cast<U8>(yyjson_get_uint(val));
        } else if constexpr (std::is_same_v<RawT, U16>) {
            value = static_cast<U16>(yyjson_get_uint(val));
        } else if constexpr (std::is_same_v<RawT, U32>) {
            value = static_cast<U32>(yyjson_get_uint(val));
        } else if constexpr (std::is_same_v<RawT, U64>) {
            value = yyjson_get_uint(val);
        } else if constexpr (std::is_same_v<RawT, S8>) {
            value = static_cast<S8>(yyjson_get_sint(val));
        } else if constexpr (std::is_same_v<RawT, S16>) {
            value = static_cast<S16>(yyjson_get_sint(val));
        } else if constexpr (std::is_same_v<RawT, S32>) {
            value = static_cast<S32>(yyjson_get_sint(val));
        } else if constexpr (std::is_same_v<RawT, S64>) {
            value = yyjson_get_sint(val);
        } else if constexpr (std::is_same_v<RawT, F32>) {
            value = static_cast<F32>(yyjson_get_num(val));
        } else if constexpr (std::is_same_v<RawT, F64>) {
            value = yyjson_get_num(val);
        } else if constexpr (std::is_same_v<RawT, String>) {
            const char *str = yyjson_get_str(val);
            if (str) {
                value = String::from_sized_chars(str, yyjson_get_len(val));
            }
        } else if constexpr (std::is_same_v<RawT, StringView>) {
            const char *str = yyjson_get_str(val);
            if (str) {
                value = StringView(str, yyjson_get_len(val));
            }
        } else if constexpr (IsShape<JsonDeserializer, V>) {
            m_stack.push_back(val);
            m_indices.push_back(0);
            call_shape<JsonDeserializer, V>(*this, value);
            m_indices.pop_back();
            m_stack.pop_back();
        }
    }

    /**
     * @brief Navigates into a list (JSON array) context.
     *
     * @tparam Fn Callback function type.
     * @param name Key name for the array.
     * @param fn Function to execute within the array context.
     */
    template <typename Fn>
    void list(StringView name, Fn &&fn) {
        if (m_state == State::Error) {
            return;
        }

        yyjson_val *arr = do_get_value(name);
        if (!arr || !yyjson_is_arr(arr)) {
            return;
        }

        m_stack.push_back(arr);
        m_indices.push_back(0);
        std::forward<Fn>(fn)(*this);
        m_indices.pop_back();
        m_stack.pop_back();
    }

    /**
     * @brief Navigates into a dictionary (JSON object) context.
     *
     * @tparam Fn Callback function type.
     * @param name Key name for the object.
     * @param fn Function to execute within the object context.
     */
    template <typename Fn>
    void dict(StringView name, Fn &&fn) {
        if (m_state == State::Error) {
            return;
        }

        yyjson_val *obj = do_get_value(name);
        if (!obj || !yyjson_is_obj(obj)) {
            return;
        }

        m_stack.push_back(obj);
        m_indices.push_back(0);
        std::forward<Fn>(fn)(*this);
        m_indices.pop_back();
        m_stack.pop_back();
    }

    /**
     * @brief Returns the number of items in the current JSON array context.
     * @return Number of elements in the array or 0 if not in an array context.
     */
    USize current_list_size() const {
        if (m_state == State::Error || m_stack.is_empty()) {
            return 0;
        }

        yyjson_val *val = m_stack.back();
        if (yyjson_is_arr(val)) {
            return yyjson_arr_size(val);
        }

        return 0;
    }

private:
    yyjson_val *do_get_value(StringView name) {
        if (m_stack.is_empty()) {
            return nullptr;
        }
        yyjson_val *parent = m_stack.back();
        if (yyjson_is_obj(parent)) {
            if (name.is_empty()) {
                // If in object and name is empty, it might be a root call or error.
                // But normally we expect a name.
                return nullptr;
            }
            return yyjson_obj_getn(parent, name.data(), name.size());
        } else if (yyjson_is_arr(parent)) {
            if (m_indices.is_empty()) {
                return nullptr;
            }
            USize &idx = m_indices.back();
            return yyjson_arr_get(parent, idx++);
        }
        return nullptr;
    }

private:
    yyjson_doc *m_json_doc{nullptr};
    yyjson_val *m_json_root{nullptr};

    DynamicArray<yyjson_val *> m_stack{};
    DynamicArray<USize> m_indices{};
    State m_state{State::Init};
};

} // namespace fr
