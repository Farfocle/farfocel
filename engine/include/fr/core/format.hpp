/**
 * @file format.hpp
 * @author Kiju
 *
 * @brief Simple format library utilizing the shape protocol.
 */

#pragma once

#include <charconv>
#include <system_error>

#include "fr/core/json.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typetraits.hpp"

namespace fr {

/**
 * @brief Options for controlling the formatting output.
 */
struct FormatOptions {
    bool pretty{false};
    bool types{false};
    bool serializer{true};
    S32 float_precision{-1};
};

namespace impl {

/**
 * @brief Trims trailing zeros and redundant decimal points from a float string.
 *
 * @param s The string to trim.
 */
inline void trim_float_string(String &s) {
    if (s.find(".") == String::npos) {
        return;
    }

    while (s.size() > 0 && s.back() == '0') {
        s.pop_back();
    }

    if (s.size() > 0 && s.back() == '.') {
        s.pop_back();
    }
}

} // namespace impl

/**
 * @brief Converts a primitive value to its string representation.
 *
 * This function uses std::to_chars for high-performance conversion of numeric types.
 * It also handles bool, char, and Byte types which are considered primitives in this project.
 *
 * @tparam T A type satisfying the IsPrimitive concept.
 * @param value The value to convert.
 * @return A String containing the converted value.
 */
template <IsPrimitive T>
String primitive_to_string(T value) noexcept {
    if constexpr (IsBool<T>) {
        return value ? String::from_view("true") : String::from_view("false");
    } else if constexpr (IsChar<T>) {
        return String::from_sized_chars(&value, 1);
    } else if constexpr (IsByte<T>) {
        char buffer[8];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<U8>(value));
        FR_ASSERT(ec == std::errc(), "failed to convert byte to string");

        return String::from_sized_chars(buffer, static_cast<USize>(ptr - buffer));
    } else {
        char buffer[64];
        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);

        if (ec != std::errc()) [[unlikely]] {
            return String();
        }

        return String::from_sized_chars(buffer, static_cast<USize>(ptr - buffer));
    }
}

namespace impl {

/**
 * @brief Internal dispatcher for converting any type to String for formatting.
 */
template <typename T>
String to_string_dispatch(const T &val, const FormatOptions &opts) {
    using RawT = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<RawT, String>) {
        return val;
    } else if constexpr (std::is_same_v<RawT, StringView>) {
        return String::from_view(val);
    } else if constexpr (std::is_same_v<RawT, const char *> || std::is_same_v<RawT, char *> ||
                         (std::is_array_v<RawT> &&
                          std::is_same_v<std::remove_extent_t<RawT>, char>)) {
        return String::from_chars(val);
    } else if constexpr (IsPrimitive<RawT>) {
        if constexpr (IsF<RawT>) {
            if (opts.float_precision >= 0) {
                char buffer[128];
                auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), val,
                                               std::chars_format::fixed, opts.float_precision);
                if (ec == std::errc()) {
                    return String::from_sized_chars(buffer, static_cast<USize>(ptr - buffer));
                }
            }
        }
        String s = primitive_to_string(val);
        if constexpr (IsF<RawT>) {
            trim_float_string(s);
        }
        return s;
    } else {
        if (!opts.serializer) {
            return String::from_view("{Object}");
        }

        JsonWriterArchive::Options jopts{.types = opts.types, .pretty = opts.pretty};
        JsonWriterArchive serializer(jopts);

        call_shape(serializer, const_cast<RawT &>(val));

        return serializer.consume();
    }
}

} // namespace impl

/**
 * @brief Formats a string with options and variable arguments.
 *
 * @tparam Ts Types of the arguments to format.
 * @param opts Formatting options.
 * @param fmt The format string containing "{}" placeholders.
 * @param args Arguments to replace placeholders.
 * @return The formatted String.
 */
template <typename... Ts>
String format_with_options(const FormatOptions &opts, StringView fmt, Ts &&...args) {
    if constexpr (sizeof...(Ts) == 0) {
        return String(fmt);
    }

    String arg_strings[] = {impl::to_string_dispatch(std::forward<Ts>(args), opts)...};
    String result;

    USize pos = 0;
    USize arg_idx = 0;
    const USize fmt_size = fmt.size();

    while (pos < fmt_size) {
        USize next = fmt.find("{}", pos);
        if (next == StringView::npos) {
            result.append(fmt.view_from(pos));
            break;
        }

        if (next > pos) {
            result.append(fmt.view(pos, next - 1));
        }

        if (arg_idx < sizeof...(Ts)) {
            result.append(arg_strings[arg_idx++].view());
        } else {
            result.append("{}");
        }
        pos = next + 2;
    }

    return result;
}

/**
 * @brief Formats a string with default options.
 *
 * @tparam Ts Types of the arguments to format.
 * @param fmt The format string containing "{}" placeholders.
 * @param args Arguments to replace placeholders.
 * @return The formatted String.
 */
template <typename... Ts>
String format(StringView fmt, Ts &&...args) {
    return format_with_options(FormatOptions{}, fmt, std::forward<Ts>(args)...);
}

} // namespace fr
