/**
 * @file string.hpp
 * @author Tfoedy
 *
 * @brief Farfocel's String!
 *
 * @details
 * Inspired by Facebook's folly implementation.
 * Supports SSO and it is expected to be performant.
 * Chech string's base class for details on memory management and optimizations.
 */

#pragma once

#include <algorithm>
#include <ostream>

#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/string_base.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

class String : public fr_stl_str::StringBase {
public:
    static constexpr USize npos = static_cast<USize>(-1);

    inline static USize growth_multiplier_percent = 200;

    using fr_stl_str::StringBase::StringBase;

    ////////
    /// CONSTRUCTORS, ASSIGNERS, DESTRUCTORS
    ////////

    String()
        : fr_stl_str::StringBase() {
    }

    /**
     * @brief Constructor from a StringView
     * @param str View of a string to copy
     */
    String(StringView str)
        : fr_stl_str::StringBase(str.size()) {
        append(str);
    }

    /**
     * @brief Constructor from an array of characters
     * @param str Pointer to the character array
     * @details It looks over characters until it finds null-terminator
     */
    String(const char *str)
        : fr_stl_str::StringBase(str ? StringView(str).size() : 0) {
        if (str) {
            append(StringView(str));
        }
    }

    /**
     * @brief Constructor from an array of characters of known length
     * @param str Pointer to the character array
     * @param count The amount of characters to copy
     */
    String(const char *str, USize count)
        : fr_stl_str::StringBase(count) {
        if (str && count > 0)
            append(StringView(str, count));
    }

    /**
     * @brief Constructor for a repeated character string
     * @param count Count of repeated characters
     * @param c Character to repeat
     */
    String(USize count, char c)
        : fr_stl_str::StringBase(count) {
        append(count, c);
    }

    /**
     * @brief Copy constructor
     * @param other String to copy
     */
    String(const String &other)
        : fr_stl_str::StringBase(other) {
    }

    /**
     * @brief Move constructor
     * @param other String to move
     */
    String(String &&other) noexcept
        : fr_stl_str::StringBase(std::move(other)) {
    }

    /**
     * @brief Copy assignment operator
     * @param other String to assign
     */
    String &operator=(const String &other) {
        if (this != &other) {
            clear();
            append(other.view());
        }
        return *this;
    }

    /**
     * @brief Move assignment operator
     * @param other String to move
     */
    String &operator=(String &&other) noexcept {
        if (this != &other) {
            this->swap(other);
        }
        return *this;
    }

    /**
     * @brief Assign operator from a StringView
     * @param str View to assign
     */
    String &operator=(StringView str) {
        if (is_aliased(str)) {
            String temp(str);
            clear();
            append(temp.view());
            return *this;
        }

        clear();
        append(str);
        return *this;
    }

    /**
     * @brief Similar to assign operator, but this repopulates, or zeros, current string
     * @param count Count of repeated characters
     * @param c Character to repeat
     */
    String &assign(USize count, char c) {
        clear();
        append(count, c);
        return *this;
    }

    ////////
    /// TYPE CONVERSIONS
    ////////

    /**
     * @brief Implicit conversion to StringView
     */
    operator StringView() const noexcept {
        return view();
    }

    ////////
    /// ITERATORS
    ////////

    /**
     * @brief Returns an iterator to the beginning
     * @return Pointer to the first character
     */
    char *begin() {
        return data();
    }

    /**
     * @brief Returns a constant iterator to the beginning
     * @return Constant pointer to the first character
     */
    const char *cbegin() const {
        return data();
    }

    /**
     * @brief Returns an iterator to the end
     * @return Pointer to the character following the last character
     */
    char *end() {
        return data() + size();
    }

    /**
     * @brief Returns a constant iterator to the end
     * @return Constant pointer to the character following the last character
     */
    const char *cend() const {
        return data() + size();
    }

    ////////
    /// ACCESSORS
    ////////

    /**
     * @brief Character accessor
     * @param pos Position of the character
     * @return Character at specified position
     */
    char &operator[](USize pos) {
        FR_ASSERT(pos < size(),
                  "fr::String operator[](USize pos): pos must be <= than string size");
        return data()[pos];
    }

    /**
     * @brief Read-only character accessor
     * @param pos Position of the character
     * @return Constant reference to the character at specified position
     */
    const char &operator[](USize pos) const {
        FR_ASSERT(pos <= size(),
                  "fr::String operator[](USize pos): pos must be <= than string size");
        return data()[pos];
    }

    /**
     * @brief Returns the first character
     * @return First character
     */
    char &front() {
        FR_ASSERT(size() > 0, "fr::String front(): string must not be empty");
        return data()[0];
    }

    /**
     * @brief Returns the read-only first character
     * @return Constant reference to the first character
     */
    const char &front() const {
        FR_ASSERT(size() > 0, "fr::String front(): string must not be empty");
        return data()[0];
    }

    /**
     * @brief Returns the last character
     * @return Last character
     */
    char &back() {
        FR_ASSERT(size() > 0, "fr::String back(): string must not be empty");
        return data()[size() - 1];
    }

    /**
     * @brief Returns the read-only last character
     * @return Constant reference to the last character
     */
    const char &back() const {
        FR_ASSERT(size() > 0, "fr::String back(): string must not be empty");
        return data()[size() - 1];
    }

    ////////
    /// CAPACITY
    //////

    /**
     * @brief Forcefully resizes the string
     * @param count New total string length
     * @param c Character to be used in place of newly allocated space
     * @details If count < string's length then it gets cut.
     * If count > string's length then new memory is allocated and populated with the specified
     * character.
     */
    void resize(USize count, char c = '\0') {
        USize current_size = size();

        if (current_size > count) {
            set_size(count);
            return;
        } else if (current_size == count) {
            return;
        }

        std::memset(prepare_append(count - current_size), c, count - current_size);
        set_size(count);
    }

    /**
     * @brief Quickly clears string's length without deallocating memory
     */
    void clear() noexcept {
        set_size(0);
    }

    ////////
    /// MODIFIERS
    ////////

    /**
     * @brief Adds a character to the end of the string
     * @param c Character to be added
     */
    void push_back(char c) {
        *prepare_append(1) = c;
        set_size(size() + 1);
    }

    /// @brief Removes a single character from the end of the string
    void pop_back() {
        FR_ASSERT(size() > 0, "fr::String pop_back(): string must not be empty");

        data()[size() - 1] = '\0';
        set_size(size() - 1);
    }

    /**
     * @brief Adds a string view to the current string
     * @param str The view to be added
     */
    String &append(StringView str) {
        USize count = str.size();
        if (count == 0)
            return *this;

        if (is_aliased(str)) {
            String temp(str);
            return append(temp.view());
        }

        fr::mem::copy_raw_range(str.data(), count, prepare_append(count));
        set_size(size() + count);

        return *this;
    }

    /**
     * @brief Adds a series of repeated characters to the current string
     * @param count The amount of characters to be added
     * @param c The character to be added
     */
    String &append(USize count, char c) {
        if (count == 0)
            return *this;

        std::memset(prepare_append(count), c, count);

        return *this;
    }

    /**
     * @brief Appends a string view to the current string
     * @param str The view to be added
     * @return Reference to the modified string
     */
    String &operator+=(StringView str) {
        return append(str);
    }

    ////////
    /// SLOWER MODIFIERS
    ////////

    /**
     * @brief Inserts a string view at a specified position
     * @param pos The position at which to insert
     * @param str The inserted view
     * @return Modified string
     */
    String &insert(USize pos, StringView str) {
        USize add_count = str.size();
        if (add_count == 0)
            return *this;

        if (is_aliased(str)) {
            String temp(str);
            return insert(pos, temp.view());
        }

        FR_ASSERT(pos <= size(), "fr::String insert(): pos is out of bounds");
        USize current_size = size();

        prepare_append(add_count);

        fr::mem::shift_range_right(data() + pos, current_size - pos, add_count);
        fr::mem::copy_raw_range(str.data(), add_count, data() + pos);

        set_size(current_size + add_count);

        return *this;
    }

    /**
     * @brief Inserts repeated characters at a specified position
     * @param pos The position at which to insert
     * @param count The amount of characters to add
     * @param c The added character
     * @return Modified string
     */
    String &insert(USize pos, USize count, char c) {
        if (count == 0)
            return *this;

        FR_ASSERT(pos <= size(),
                  "fr::String insert(USize pos, USize count, char c): pos is out of bounds");
        FR_ASSERT(
            c != '\0',
            "fr::String insert(USize pos, USize count, char c): c must not be null-terminator");
        USize add_count = count;
        USize current_size = size();

        prepare_append(add_count);

        fr::mem::shift_range_right(data() + pos, current_size - pos, add_count);
        fr::mem::set_raw_range(data() + pos, c, count);

        set_size(current_size + add_count);

        return *this;
    }

    /**
     * @brief Erases characters starting at a specified position
     * @param pos The position at which to start erasing
     * @param count The amount of characters to erase
     * @return Modified string
     */
    String &erase(USize pos = 0, USize count = npos) {
        USize current_size = size();

        FR_ASSERT(pos <= current_size,
                  "fr::String erase(USize pos = 0, USize count = npos): pos cannot "
                  "be greater than string's size");

        if (count == npos || pos + count > current_size) {
            count = current_size - pos;
        }

        if (count == 0)
            return *this;

        USize delta_count = current_size - (pos + count);

        if (delta_count > 0)
            fr::mem::shift_range_left(data() + pos, delta_count, count);

        set_size(current_size - count);
        return *this;
    }

    /**
     * @brief Replaces the specified portion of the string
     * @param pos Position at which to replace the string
     * @param str The new string view to replace with
     * @param count The amount of characters to replace
     * @return Modified string
     * @details If count = npos then everything to the right of pos is replaces
     */
    String &replace(USize pos, StringView str, USize count = npos) {
        if (is_aliased(str)) {
            String temp(str);
            return replace(pos, temp.view(), count);
        }

        USize current_size = size();

        FR_ASSERT(pos <= current_size,
                  "fr::String replace(USize pos, StringView str, USize count): pos cannot "
                  "be greater than string's size");

        if (count == npos || pos + count > current_size) {
            count = current_size - pos;
        }

        USize str_size = str.size();
        USize new_size = current_size - count + str_size;

        if (new_size > capacity())
            prepare_append(new_size - current_size);

        USize tail_length = current_size - (pos + count);
        if (tail_length > 0) {
            if (str_size > count)
                fr::mem::shift_range_right(data() + pos + count, tail_length, str_size - count);
            else if (str_size < count)
                fr::mem::shift_range_left(data() + pos + str_size, tail_length, count - str_size);
        }

        if (str_size > 0)
            fr::mem::copy_raw_range(str.data(), str_size, data() + pos);

        set_size(new_size);

        return *this;
    }

    ////////
    /// SEARCH
    ////////

    /**
     * @brief Searches for the substring in the string starting at the position
     * @param str Substring to find
     * @param pos Position to start looking at
     * @return Returns the position of the found substring or npos if not found
     */
    USize find(StringView str, USize pos = 0) const {
        USize current_size = size();
        USize str_size = str.size();

        if (pos > current_size || pos + str_size > current_size)
            return npos;
        if (str_size == 0)
            return pos;

        const char *match =
            std::search(data() + pos, data() + current_size, str.data(), str.data() + str_size);
        return match == data() + current_size ? npos : static_cast<USize>(match - data());
    }

    /**
     * @brief Searches for the character in the string starting at the position
     * @param c Character to find
     * @param pos Position to start looking at
     * @return Returns the position of the found character or npos if not found
     */
    USize find(char c, USize pos = 0) const {
        USize current_size = size();
        if (pos >= current_size)
            return npos;

        const void *match = std::memchr(data() + pos, c, current_size - pos);
        return match ? static_cast<USize>(static_cast<const char *>(match) - data()) : npos;
    }

    /**
     * @brief Searches for the substring in the string going backwards
     * @param str Substring to find
     * @param pos Position to start looking backwards from
     * @return Returns the position of the found substring or npos if not found
     */
    USize reverse_find(StringView str, USize pos = npos) const {
        USize current_size = size();
        USize str_size = str.size();

        if (str_size > current_size)
            return npos;

        USize start_pos =
            (pos == npos || pos > current_size - str_size) ? current_size - str_size : pos;

        if (str_size == 0)
            return start_pos;

        for (USize i = start_pos + 1; i-- > 0;) {
            if (std::memcmp(data() + i, str.data(), str_size) == 0) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Finds the first character equal to any of the characters in the given array
     * @param chars Array of characters to search for
     * @param pos Position to start looking at
     * @return Position of the first matching character or npos if not found
     */
    USize find_first_of(StringView chars, USize pos = 0) const {
        USize current_size = size();
        USize chars_size = chars.size();

        if (pos >= current_size || chars_size == 0)
            return npos;

        for (USize i = pos; i < current_size; ++i) {
            if (std::memchr(chars.data(), data()[i], chars_size))
                return i;
        }
        return npos;
    }

    /**
     * @brief Finds the first character that is NOT equal to any of the characters in the given
     * array
     * @param chars Array of characters to avoid
     * @param pos Position to start looking at
     * @return Position of the first non-matching character or npos if not found
     */
    USize find_first_not_of(StringView chars, USize pos = 0) const {
        USize current_size = size();
        USize chars_size = chars.size();

        if (pos >= current_size)
            return npos;
        if (chars_size == 0)
            return pos;

        for (USize i = pos; i < current_size; ++i) {
            if (!std::memchr(chars.data(), data()[i], chars_size))
                return i;
        }
        return npos;
    }

    /**
     * @brief Finds the last character equal to any of the characters in the given array
     * @param chars Array of characters to search for
     * @param pos Position to start looking backwards from
     * @return Position of the last matching character or npos if not found
     */
    USize find_last_of(StringView chars, USize pos = npos) const {
        USize current_size = size();
        USize chars_size = chars.size();

        if (current_size == 0 || chars_size == 0)
            return npos;

        USize start_pos = (pos >= current_size) ? current_size - 1 : pos;

        for (USize i = start_pos + 1; i-- > 0;) {
            if (std::memchr(chars.data(), data()[i], chars_size))
                return i;
        }
        return npos;
    }

    /**
     * @brief Finds the last character that is NOT equal to any of the characters in the given array
     * @param chars Array of characters to avoid
     * @param pos Position to start looking backwards from
     * @return Position of the last non-matching character or npos if not found
     */
    USize find_last_not_of(StringView chars, USize pos = npos) const {
        USize current_size = size();
        USize chars_size = chars.size();

        if (current_size == 0)
            return npos;

        USize start_pos = (pos >= current_size) ? current_size - 1 : pos;

        if (chars_size == 0)
            return start_pos;

        for (USize i = start_pos + 1; i-- > 0;) {
            if (!std::memchr(chars.data(), data()[i], chars_size))
                return i;
        }
        return npos;
    }

    ////////
    /// OTHER & QOL
    ////////

    /**
     * @brief Returns a newly constructed string object with its value initialized to a substring
     * @param pos Position of the first character to include
     * @param count Amount of characters to include
     * @return The new string object
     */
    String substr(USize pos, USize count = npos) const {
        USize current_size = size();
        if (pos >= current_size)
            return String();

        if (count == npos || pos + count > current_size)
            count = current_size - pos;

        return String(data() + pos, count);
    }

    /**
     * @brief Lexicographically compares this string to another
     * @param str The view to compare with
     * @return 0 if equal, <0 if this string is smaller, >0 if greater
     */
    int compare(StringView str) const {
        USize lhs_sz = size();
        USize rhs_sz = str.size();

        int res = std::memcmp(data(), str.data(), std::min(lhs_sz, rhs_sz));
        if (res != 0)
            return res;

        if (lhs_sz < rhs_sz)
            return -1;
        if (lhs_sz > rhs_sz)
            return 1;
        return 0;
    }

    /**
     * @brief Checks if the string begins with the given string view
     * @param str The prefix to check
     * @return True if it starts with the prefix, false otherwise
     */
    bool starts_with(StringView str) const {
        USize str_size = str.size();
        if (str_size > size())
            return false;
        return std::memcmp(data(), str.data(), str_size) == 0;
    }

    /**
     * @brief Checks if the string ends with the given string view
     * @param str The suffix to check
     * @return True if it ends with the suffix, false otherwise
     */
    bool ends_with(StringView str) const {
        USize current_size = size();
        USize str_size = str.size();
        if (str_size > current_size)
            return false;
        return std::memcmp(data() + current_size - str_size, str.data(), str_size) == 0;
    }

    /**
     * @brief Checks if the string contains the given string view
     * @param str The substring to check for
     * @return True if the substring is found, false otherwise
     */
    bool contains(StringView str) const {
        return find(str) != npos;
    }

    /**
     * @brief Removes whitespace characters surrounding the string
     * @details For example, running this on string "   Human Instrumentality  " will result in it
     * becoming "Human Instrumentality".
     *
     * This also trims it from '\n', '\t' and '\r' characters.
     */
    void trim() {
        USize current_size = size();
        if (current_size == 0)
            return;

        USize start = 0;
        while (start < current_size && (data()[start] == ' ' || data()[start] == '\n' ||
                                        data()[start] == '\t' || data()[start] == '\r')) {
            start++;
        }

        USize end = current_size;
        while (end > start && (data()[end - 1] == ' ' || data()[end - 1] == '\n' ||
                               data()[end - 1] == '\t' || data()[end - 1] == '\r')) {
            end--;
        }

        if (start > 0 || end < current_size) {
            if (end > start && start > 0) {
                fr::mem::shift_range_left(data(), end - start, start);
            }
            set_size(end - start);
        }
    }

    /**
     * @brief Converts all ASCII (and ONLY ASCII) letters in the string to lowercase
     */
    void to_lower_ascii() {
        USize current_size = size();
        for (USize i = 0; i < current_size; ++i) {
            if (data()[i] >= 'A' && data()[i] <= 'Z')
                data()[i] += 32;
        }
    }

    /**
     * @brief Converts all ASCII (and ONLY ASCII) letters in the string to uppercase
     */
    void to_upper_ascii() {
        USize current_size = size();
        for (USize i = 0; i < current_size; ++i) {
            if (data()[i] >= 'a' && data()[i] <= 'z')
                data()[i] -= 32;
        }
    }

    /**
     * @brief returns Non-owning view of the string's current data
     * @return StringView object pointing to the internal buffer
     */
    StringView view() const noexcept {
        return StringView(data(), size());
    }

    /**
     * @brief Returns a hash of the string.
     */
    Hash hash() const noexcept {
        return view().hash();
    }

private:
    /**
     * @brief Checks if the view is made of this particular string
     * @return True if string_view is made of this string, false if not
     */
    bool is_aliased(StringView str) const noexcept {
        const char *d = data();
        return (str.data() >= d) && (str.data() < d + size());
    }

    /**
     * @brief Prepares string for an influx of new characters
     * @param additional_count Amount of new characters
     * @return The address at which to add new characters
     */
    char *prepare_append(USize additional_count) {
        USize cap = capacity();
        USize current_size = size();

        if (current_size + additional_count > cap) {
            USize new_cap = (cap * growth_multiplier_percent) / 100;

            if (new_cap < current_size + additional_count) {
                new_cap = current_size + additional_count;
            }

            reserve(new_cap);
        }

        return data() + current_size;
    }
};

////////
/// GLOBAL OPERATORS
////////

/// @brief Checks if two strings are equal
inline bool operator==(const String &lhs, const String &rhs) {
    return lhs.view() == rhs.view();
}

/**
 * @brief Checks if two strings are not equal
 */
inline bool operator!=(const String &lhs, const String &rhs) {
    return lhs.view() != rhs.view();
}

/**
 * @brief Checks if the left string is lexicographically smaller than the right string
 */
inline bool operator<(const String &lhs, const String &rhs) {
    return lhs.view() < rhs.view();
}

/**
 * @brief Checks if the left string is lexicographically greater than the right string
 */
inline bool operator>(const String &lhs, const String &rhs) {
    return lhs.view() > rhs.view();
}

/// @brief Concatenates two strings into a new string
inline String operator+(const String &lhs, const String &rhs) {
    String res;
    res.reserve(lhs.size() + rhs.size());
    res.append(lhs.view());
    res.append(rhs.view());
    return res;
}

/// @brief Outputs the string to an output stream
inline std::ostream &operator<<(std::ostream &os, const String &str) {
    os.write(str.data(), str.size());
    return os;
}

} // namespace fr
