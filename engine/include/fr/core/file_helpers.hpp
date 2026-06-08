/**
 * @file file_helpers.hpp
 * @author Stachu
 * @brief Basic helpers for file handling.
 */

#pragma once

#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::file_helpers {

namespace impl {
[[nodiscard]] inline USize find_last_separator(StringView path) noexcept {
    if (path.is_empty())
        return StringView::npos;

    for (USize i = path.size(); i-- > 0;) {
        if (path[i] == '/' || path[i] == '\\') {
            return i;
        }
    }
    return StringView::npos;
}
} // namespace impl

/**
 * @brief Normalizes the path by replacing all backslashes with forward slashes.
 */
inline void normalize(String &path) noexcept {
    USize sz = path.size();
    for (USize i = 0; i < sz; ++i) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

/**
 * @brief Returns a normalized copy of the path with all backslashes converted to forward slashes.
 */
[[nodiscard]] inline String normalize(StringView path) noexcept {
    String result(path);
    normalize(result);
    return result;
}

/**
 * @brief Extracts the filename component (with extension) from the given path.
 */
[[nodiscard]] inline StringView get_filename(StringView path) noexcept {
    USize sep = impl::find_last_separator(path);
    if (sep == StringView::npos) {
        return path;
    }
    return path.view_from(sep + 1);
}

/**
 * @brief Returns the path to the parent directory, stripping the filename.
 */
[[nodiscard]] inline StringView get_parent_path(StringView path) noexcept {
    USize sep = impl::find_last_separator(path);
    if (sep == StringView::npos) {
        return StringView();
    }
    if (sep == 0) {
        return path.view_to(0); // root
    }
    return path.view_to(sep - 1);
}

/**
 * @brief Extracts the file extension (excluding the dot) from the path.
 */
[[nodiscard]] inline StringView get_extension(StringView path) noexcept {
    StringView filename = get_filename(path);
    USize dot = filename.find('.');

    if (dot == StringView::npos) {
        return StringView();
    }

    USize last_dot = dot;
    while (true) {
        USize next_dot = filename.find('.', last_dot + 1);
        if (next_dot == StringView::npos)
            break;
        last_dot = next_dot;
    }

    return filename.view_from(last_dot + 1);
}

/**
 * @brief Extracts the stem (filename without its extension) from the path.
 */
[[nodiscard]] inline StringView get_stem(StringView path) noexcept {
    StringView filename = get_filename(path);
    USize dot = filename.find('.');

    if (dot == StringView::npos || dot == 0) {
        return filename;
    }

    USize last_dot = dot;
    while (true) {
        USize next_dot = filename.find('.', last_dot + 1);
        if (next_dot == StringView::npos)
            break;
        last_dot = next_dot;
    }

    return filename.view_to(last_dot - 1);
}

} // namespace fr::file_helpers
