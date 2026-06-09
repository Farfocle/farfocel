/**
 * @file file.hpp
 * @author Stachu
 * @brief Basic helpers for file handling.
 */

#pragma once

#include "fr/core/dynamic_array.hpp"
#include "fr/core/optional.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"
#include <cstdio>
#include <sys/stat.h>

namespace fr::file {

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

struct ScopedFile {
    FILE* handle;
    explicit ScopedFile(FILE* f) : handle(f) {}
    ~ScopedFile() { if (handle) std::fclose(handle); }
    operator FILE*() const { return handle; }
};
} // namespace impl

/**
 * @brief Normalizes the path for Unix-like systems by replacing all backslashes with forward
 * slashes.
 */
inline void normalize_unix(String &path) noexcept {
    USize sz = path.size();
    for (USize i = 0; i < sz; ++i) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

/**
 * @brief Normalizes the path for Windows by replacing all forward slashes with backslashes.
 */
inline void normalize_windows(String &path) noexcept {
    USize sz = path.size();
    for (USize i = 0; i < sz; ++i) {
        if (path[i] == '/') {
            path[i] = '\\';
        }
    }
}

/**
 * @brief Platform-aware path normalization.
 * Uses backslashes on Windows and forward slashes on Unix-like systems.
 */
inline void normalize(String &path) noexcept {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    normalize_windows(path);
#else
    normalize_unix(path);
#endif
}

/**
 * @brief Returns a normalized copy of the path. Platform-aware.
 */
[[nodiscard]] inline String get_normalized(StringView path) noexcept {
    String result(path);
    normalize(result);
    return result;
}

/**
 * @brief Returns a normalized copy of the path for Unix-like systems.
 */
[[nodiscard]] inline String get_normalized_unix(StringView path) noexcept {
    String result(path);
    normalize_unix(result);
    return result;
}

/**
 * @brief Returns a normalized copy of the path for Windows.
 */
[[nodiscard]] inline String get_normalized_windows(StringView path) noexcept {
    String result(path);
    normalize_windows(result);
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
    if (filename.is_empty()) {
        return StringView();
    }

    for (USize i = filename.size(); i-- > 0;) {
        if (filename[i] == '.') {
            if (i == 0) break; // Hidden file edge case (for example .gitignore)
            return filename.view_from(i + 1);
        }
    }
    return StringView();
}

/**
 * @brief Extracts the stem (filename without its extension) from the path.
 */
[[nodiscard]] inline StringView get_stem(StringView path) noexcept {
    StringView filename = get_filename(path);
    if (filename.is_empty()) {
        return filename;
    }

    for (USize i = filename.size(); i-- > 0;) {
        if (filename[i] == '.') {
            if (i == 0) break; // Hidden file edge case
            return filename.view_to(i - 1);
        }
    }
    return filename;
}

/**
 * @brief Gets file size
 */
[[nodiscard]] inline Optional<S64> get_file_size(const String& path) {
    struct stat stat_buf;
    int rc = stat(path.c_str(), &stat_buf);

    if (rc == 0)
        return stat_buf.st_size;
    return fr::none();
}

/**
 * @brief Reads all bytes to a DynamicArray using specified allocator.
 * @param alloc Pointer to the allocator to use.
 * @param path Path to the file
 */
[[nodiscard]] inline fr::Optional<fr::DynamicArray<Byte>> read_all_bytes(Alloc *alloc,
                                                                         const fr::String& path) {
    impl::ScopedFile file(std::fopen(path.c_str(), "rb"));
    if (!file) {
        return fr::none();
    }

    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::rewind(file);

    if (size < 0) {
        return fr::none();
    }

    if (size == 0) {
        return fr::DynamicArray<Byte>::with_alloc(alloc);
    }

    auto buffer = fr::DynamicArray<Byte>::with_size(alloc, static_cast<USize>(size));

    USize bytesRead = std::fread(buffer.data(), 1, static_cast<USize>(size), file);
    if (bytesRead < static_cast<USize>(size)) {
        return fr::none();
    }

    return buffer;
}

/**
 * @brief Reads all bytes to a DynamicArray.
 * @param path Path to the file
 */
[[nodiscard]] inline fr::Optional<fr::DynamicArray<Byte>> read_all_bytes(const fr::String& path) {
    return read_all_bytes(get_ambient_ctx().alloc, path);
}

/**
 * @brief Reads all text from a file into a String using a specified allocator.
 * @param alloc Pointer to the allocator to use.
 * @param path Path to the file
 */
[[nodiscard]] inline fr::Optional<fr::String> read_all_text(Alloc *alloc, const fr::String& path) {
    impl::ScopedFile file(std::fopen(path.c_str(), "rb"));
    if (!file) {
        return fr::none();
    }

    std::fseek(file, 0, SEEK_END);
    long size = std::ftell(file);
    std::rewind(file);

    if (size < 0) {
        return fr::none();
    }

    if (size == 0) {
        return fr::String::with_alloc(alloc);
    }

    auto result = fr::String::with_capacity(alloc, static_cast<USize>(size));
    result.grow_default(static_cast<USize>(size));

    USize bytesRead = std::fread(result.data(), 1, static_cast<USize>(size), file);
    if (bytesRead < static_cast<USize>(size)) {
        return fr::none();
    }

    return result;
}

/**
 * @brief Reads all text from a file into a String.
 * @param path Path to the file
 */
[[nodiscard]] inline fr::Optional<fr::String> read_all_text(const fr::String& path) {
    return read_all_text(get_ambient_ctx().alloc, path);
}

/**
 * @brief Writes all bytes from a Slice to a file.
 * @param path Path to the destination file.
 * @param bytes Slice of elements to write.
 * @return True if the entire slice was written successfully, false otherwise.
 */
inline bool write_all_bytes(const fr::String& path, fr::Slice<Byte> bytes) noexcept {
    impl::ScopedFile file(std::fopen(path.c_str(), "wb"));
    if (!file) {
        return false;
    }

    USize size = bytes.size();

    if (size > 0) {
        USize bytesWritten = std::fwrite(bytes.data(), 1, size, file);
        if (bytesWritten < size) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Checks if a file or directory exists at the given path.
 * @param path Path to the file or directory.
 * @return True if file exists, false otherwise.
 */
[[nodiscard]] inline bool exists(const fr::String& path) noexcept {
    struct stat stat_buf;
    return stat(path.c_str(), &stat_buf) == 0;
}

} // namespace fr::file
