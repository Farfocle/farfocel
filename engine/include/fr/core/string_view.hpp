/**
 * @file string_view.hpp
 * @author Tfoedy
 *
 * @brief This is a window into the characters of a string that does not hold its own memory.
 * This must be used instead of referenced fr::string
 *
 */
#pragma once

#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
class StringView {
public:
    static constexpr USize npos = static_cast<USize>(-1);

    /// @brief Default constructor
    constexpr StringView() noexcept
        : m_data(nullptr),
          m_size(0) {
    }

    /**
     * @brief Constructor from a pointer and a known length
     * @param str Pointer to the character array
     * @param len Length of the string without null-terminator
     */
    constexpr StringView(const char *str, USize len) noexcept
        : m_data(str),
          m_size(len) {
        FR_ASSERT(str, "fr::StringView(const char *str, USize len): str must not be nullptr");
    }

    /**
     * @brief Constructor from a C-string
     * @param str Pointer to the character array containing null-terminator
     */
    constexpr StringView(const char *str) noexcept
        : m_data(str),
          m_size(0) {

        FR_ASSERT(str, "fr::StringView(const char *str, USize len): str must not be nullptr");
        m_size = const_strlen(str);
    }

    /**
     *
     * @brief Copy constructor
     * @param other Copier view
     */
    constexpr StringView(const StringView &other) noexcept = default;

    /**
     *
     * @brief Copy assignment opeator
     * @apram other Assigned view
     */
    constexpr StringView &operator=(const StringView &other) noexcept = default;

    /**
     * @brief Gets the pointer to the beginning of the string
     * @return Pointer to the character array
     */
    [[nodiscard]] constexpr const char *begin() const noexcept {
        return m_data;
    }

    /**
     * @brief Gets the pointer to the end of the string
     * @return Pointer to the character array right after the last character
     *
     */
    [[nodiscard]] constexpr const char *end() const noexcept {
        return m_data + m_size;
    }

    /**
     * @brief Gets a read-only pointer to the raw string data
     * @return Pointer to the character array
     */
    [[nodiscard]] constexpr const char *data() const noexcept {
        return m_data;
    }

    /**
     * @brief Gets the current text size
     * @return Number of characters in the view
     */
    [[nodiscard]] constexpr USize get_size() const noexcept {
        return m_size;
    }

    /**
     * @brief Checks if the view is empty
     * @return True if size is 0, false otherwise
     */
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return m_size == 0;
    }

    /**
     * @brief Reads a character at a given index
     * @param pos Index of the character
     * @return Character at the specified position
     * @pre pos < m_size
     */
    constexpr char operator[](USize pos) const noexcept {
        FR_ASSERT(m_size > pos,
                  "fr::StringView operator[](USize pos): pos must be smaller than string's size");
        return m_data[pos];
    }

    /**
     * @brief Gets the first character of the view
     * @return First character
     */
    constexpr char front() const noexcept {
        FR_ASSERT(m_size > 0, "fr::StringView front(): m_size must > 0");
        return m_data[0];
    }

    /**
     * @brief Gets the last character of the view
     * @return Last character
     */
    constexpr char back() const noexcept {
        FR_ASSERT(m_size > 0, "fr::StringView back(): m_size must > 0");
        return m_data[m_size - 1];
    }

    /**
     * @brief Removes characters from the front
     * @param n Number of characters to remove
     * @pre n <= m_size
     */
    constexpr void remove_prefix(USize n) noexcept {
        FR_ASSERT(n <= m_size, "fr::StringView remove_prefix(USize n): n must be <= m_size");
        m_data += n;
        m_size -= n;
    }

    /**
     * @brief Removes characters from the back
     * @param n Number of characters to remove
     * @pre n <= m_size
     */
    constexpr void remove_suffix(USize n) noexcept {
        FR_ASSERT(n <= m_size, "fr::StringView remove_suffix(USize n): n must be <= m_size");
        m_size -= n;
    }

    /**
     * @brief Cuts a smaller view out of the current one
     * @param pos Starting position
     * @param count Number of characters to include (default is npos)
     * @return A new StringView representing the substring
     * @pre pos <= m_size
     */
    constexpr StringView substr(USize pos, USize count = npos) const noexcept {
        FR_ASSERT(pos <= m_size, "fr::StringView substr(USize n): pos must be <= m_size");

        USize max_count = m_size - pos;
        USize a_count = (count > max_count) ? max_count : count;

        return StringView(m_data + pos, a_count);
    }

    /**
     * @brief Comparison of two views
     * @param other The view to compare with
     * @return 0 if identical, < 0 if this is lexicographically before other, > 0 if after
     */
    constexpr int compare(StringView other) const noexcept {
        USize len = (m_size < other.m_size) ? m_size : other.m_size;
        int cmp_res = const_strncmp(m_data, other.m_data, len);

        if (cmp_res != 0)
            return cmp_res;
        if (m_size < other.m_size)
            return -1;
        if (m_size > other.m_size)
            return 1;

        return 0;
    }

    /**
     * @brief Checks if the view starts with a specific prefix
     * @param prefix The view to check for
     * @return True if it starts with the prefix, false otherwise
     */
    constexpr bool start_with(StringView prefix) const noexcept {
        if (prefix.m_size > m_size)
            return false;
        return const_strncmp(m_data, prefix.m_data, prefix.m_size) == 0;
    }

    /**
     * @brief Checks if the view ends with a specific suffix
     * @param suffix The view to check for
     * @return True if it ends with the suffix, false otherwise
     */
    constexpr bool ends_with(StringView suffix) const noexcept {
        if (suffix.m_size > m_size)
            return false;
        return const_strncmp(m_data + m_size - suffix.m_size, suffix.m_data, suffix.m_size) == 0;
    }

    /**
     * @brief Searches for a single character
     * @param c Character to find
     * @param pos Starting position for the search
     * @return Index of the found character, or npos if not found
     */
    constexpr USize find(char c, USize pos = 0) const noexcept {
        for (USize i = pos; i < m_size; ++i)
            if (m_data[i] == c)
                return i;

        return npos;
    }

    /**
     * @brief Searches for a substring
     * @param str The substring to find
     * @param pos Starting position for the search
     * @return Index of the start of the substring, or npos if not found
     */
    constexpr USize find(StringView str, USize pos = 0) const noexcept {
        if (str.m_size == 0)
            return pos <= m_size ? pos : npos;
        if (pos + str.m_size > m_size)
            return npos;

        for (USize i = pos; i <= m_size - str.m_size; ++i)
            if (const_strncmp(m_data + i, str.m_data, str.m_size) == 0)
                return i;
        return npos;
    }

private:
    /**
     * @brief constexpr version of strlen()
     * @param str Null-terminated character array
     * @return Length of the string
     * @details Works by looking for the \0 character.
     */
    static constexpr USize const_strlen(const char *str) noexcept {
        USize len = 0;

        while (str[len] != '\0')
            len++;

        return len;
    }

    /**
     * @brief Compare two string up to a specific number of characters
     * @param str1 First character array
     * @param str2 Second character array
     * @param n Maximum number of characters to compare
     * @return 0 if identical, < 0 if str1 is before str2, > 0 if str1 is after str2
     */
    static constexpr int const_strncmp(const char *str1, const char *str2, USize n) noexcept {
        for (USize i = 0; i < n; ++i) {
            unsigned char c1 = static_cast<unsigned char>(str1[i]);
            unsigned char c2 = static_cast<unsigned char>(str2[i]);

            if (c1 > c2)
                return 1;
            if (c1 < c2)
                return -1;
            if (c1 == '\0')
                return 0;
        }
        return 0;
    }

    const char *m_data;
    USize m_size;
};

/**
 * @brief Checks if two views are the same
 * @param lhs Left view
 * @param rhs Right vieww
 * @return True if sizes and contents the same
 */
inline constexpr bool operator==(StringView lhs, StringView rhs) noexcept {
    return lhs.get_size() == rhs.get_size() && lhs.compare(rhs) == 0;
}

/**
 * @brief Checks if two views are NOT equal
 * @param lhs Left view
 * @param rhs Right view
 * @return True if sizes or contents do not match
 */
inline constexpr bool operator!=(StringView lhs, StringView rhs) noexcept {
    return !(lhs == rhs);
}

/**
 * @brief Checks if the left view is lexicographically less than the right view
 * @param lhs Left view
 * @param rhs Right view
 * @return True if lhs < rhs
 */
inline constexpr bool operator<(StringView lhs, StringView rhs) noexcept {
    return lhs.compare(rhs) < 0;
}
/**
 * @brief Checks if the left view is lexicographically greater than the right view
 * @param lhs Left view
 * @param rhs Right view
 * @return True if lhs < rhs
 */
inline constexpr bool operator>(StringView lhs, StringView rhs) noexcept {
    return lhs.compare(rhs) > 0;
}

} // namespace fr
