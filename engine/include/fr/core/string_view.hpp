/**
 * @file string_view.hpp
 * @author Tfoedy
 *
 * @brief This is a window into the characters of a string that does not hold its own memory.
 * This must be used instead of referenced fr::string
 */

#pragma once

#include "fr/core/hash.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {
class StringView {
public:
    static constexpr USize npos = static_cast<USize>(-1);

    constexpr StringView() noexcept
        : m_data(nullptr),
          m_size(0) {
    }

    /**
     * @brief Constructor from a pointer and a known length.
     *
     * @param str Pointer to the character array.
     * @param len Length of the string.
     */
    constexpr StringView(const char *str, USize len) noexcept
        : m_data(str),
          m_size(len) {
        FR_ASSERT(str != nullptr || len == 0, "pointer must be non-null if size is non-zero");
    }

    /**
     * @brief Constructor from a C-string.
     *
     * @param str Null-terminated character array.
     */
    constexpr StringView(const char *str) noexcept
        : m_data(str),
          m_size(0) {

        FR_ASSERT(str != nullptr, "pointer must be non-null");
        m_size = const_strlen(str);
    }

    /**
     * @brief Copy constructor.
     *
     * @param other View to copy.
     */
    constexpr StringView(const StringView &other) noexcept = default;

    /**
     * @brief Copy assignment operator.
     *
     * @param other View to assign.
     * @return Reference to this view.
     */
    constexpr StringView &operator=(const StringView &other) noexcept = default;

    /**
     * @brief Gets the pointer to the beginning of the string.
     *
     * @return Constant pointer to the first character.
     */
    [[nodiscard]] constexpr const char *begin() const noexcept {
        return m_data;
    }

    /**
     * @brief Gets the pointer to the end of the string.
     *
     * @return Constant pointer past the last character.
     */
    [[nodiscard]] constexpr const char *end() const noexcept {
        return m_data + m_size;
    }

    /**
     * @brief Gets a read-only pointer to the raw string data.
     *
     * @return Constant pointer to the character array.
     */
    [[nodiscard]] constexpr const char *data() const noexcept {
        return m_data;
    }

    /**
     * @brief Gets the current text size.
     *
     * @return Number of characters in the view.
     */
    [[nodiscard]] constexpr USize size() const noexcept {
        return m_size;
    }

    /**
     * @brief Checks if the view is empty.
     *
     * @return True if size is 0.
     */
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return m_size == 0;
    }

    /**
     * @brief Character accessor.
     *
     * @param pos Index of the character.
     * @return Character at position.
     * @pre pos < size().
     */
    constexpr char operator[](USize pos) const noexcept {
        FR_ASSERT(pos < m_size, "index out of bounds");
        return m_data[pos];
    }

    /**
     * @brief Returns the first character.
     *
     * @return First character.
     * @pre !is_empty().
     */
    constexpr char front() const noexcept {
        FR_ASSERT(m_size > 0, "empty view access");
        return m_data[0];
    }

    /**
     * @brief Returns the last character.
     *
     * @return Last character.
     * @pre !is_empty().
     */
    constexpr char back() const noexcept {
        FR_ASSERT(m_size > 0, "empty view access");
        return m_data[m_size - 1];
    }

    /**
     * @brief Removes characters from the front.
     *
     * @param n Number of characters to remove.
     */
    constexpr void remove_prefix(USize n) noexcept {
        FR_ASSERT(n <= m_size, "prefix size overflow");
        m_data += n;
        m_size -= n;
    }

    /**
     * @brief Removes characters from the back.
     *
     * @param n Number of characters to remove.
     */
    constexpr void remove_suffix(USize n) noexcept {
        FR_ASSERT(n <= m_size, "suffix size overflow");
        m_size -= n;
    }

    /**
     * @brief Returns a substring view.
     *
     * @param pos Start position.
     * @param count Number of characters.
     * @return Substring view.
     */
    constexpr StringView substr(USize pos, USize count = npos) const noexcept {
        FR_ASSERT(pos <= m_size, "index out of bounds");

        USize max_count = m_size - pos;
        USize a_count = (count > max_count) ? max_count : count;

        return StringView(m_data + pos, a_count);
    }

    /**
     * @brief Returns a hash of the current view.
     */
    [[nodiscard]] constexpr Hash hash() const noexcept {
        return hash_bytes(m_data, m_size);
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
    constexpr bool starts_with(StringView prefix) const noexcept {
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
        if (pos >= m_size)
            return npos;

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
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
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
