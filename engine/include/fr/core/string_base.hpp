/**
 * @file string_base.hpp
 * @author Tfoedy
 *
 * @brief Low-level base class for handling string's memory and SSO.
 *
 * @details
 * fr::string is 32 bytes, and short string can contain at most 23 characters (not counting null
 * terminator).
 *
 * When the amount of text characters (without null terminator) is more than 23, the data is
 * no longer stored within the SSO's stack buffer but allocated/moved to the heap.
 *
 * The string category is determined by the 2 most significant bits of the last byte of the object,
 * where '00' indicates SHORT STRING and '10' LONG STRING.
 *
 * This is heavily inspired by Facebook's string from folly.
 * Watch https://www.youtube.com/watch?v=kPR8h4-qZdk&t=611s
 * */

#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>

#include "fr/core/alloc.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/macros.hpp"
#include "fr/core/mem.hpp"
#include "fr/core/typedefs.hpp"

namespace fr::impl {

/**
 * @brief Tag struct used to indicate the transfer of the ownership of an existing memory of
 * a string
 */
struct acquire_memory_t {
    explicit acquire_memory_t() = default;
};
inline constexpr acquire_memory_t acquire_memory{};

/// @brief Base class of fr::string for handling its memory.
class StringBase {
public:
    ////////
    /// CONSTRUCTORS & DESTRUCTORS
    ////////

    /**
     * @brief Default constructor
     *
     * @param alloc Pointer to the allocator.
     * @note This sets the type to short string
     */
    explicit StringBase(fr::Alloc *alloc = fr::globals::get_default_allocator()) noexcept
        : m_alloc(alloc) {
        m_data.short_string.text[0] = '\0';
        m_data.short_string.space_left = max_short_string_size;
    }

    /**
     * @brief Preallocating constructor
     *
     * @param capacity_to_allocate Characters to preallocate.
     * @param alloc Pointer to the allocator.
     */
    explicit StringBase(USize capacity_to_allocate,
                        fr::Alloc *alloc = fr::globals::get_default_allocator())
        : m_alloc(alloc) {
        // Short string
        if (capacity_to_allocate <= 23) {
            m_data.short_string.text[0] = '\0';
            m_data.short_string.space_left = max_short_string_size;
        } else { // Long string
            m_alloc = alloc;
            // +1 ensures the space for '\0'
            m_data.long_string.ptr =
                static_cast<char *>(m_alloc->allocate(capacity_to_allocate + 1, alignof(char)));
            // capacity is stored without null terminator
            m_data.long_string.capacity = capacity_to_allocate;
            m_data.long_string.size = 0;
            m_data.long_string.ptr[0] = '\0';

            set_flag_to_long_string();
        }
    }

    /**
     * @brief Acquiring constructor
     *
     * @param ptr Allocated string memory.
     * @param size Length without null-terminator.
     * @param allocated_capacity Memory size with null-terminator.
     * @param alloc The allocator used.
     */
    StringBase(char *ptr, USize size, USize allocated_capacity, acquire_memory_t,
               fr::Alloc *alloc = fr::globals::get_default_allocator())
        : m_alloc(alloc) {
        FR_ASSERT(size > 0, "size must be non-zero");
        FR_ASSERT(allocated_capacity >= size + 1, "capacity too small");
        FR_ASSERT(ptr[size] == '\0', "missing null terminator");

        m_data.long_string.ptr = ptr;
        m_data.long_string.size = size;
        // capacity excludes null-terminator
        m_data.long_string.capacity = allocated_capacity - 1;
        set_flag_to_long_string();
    }

    /**
     * @brief Copy constructor
     *
     * @param other Copied string.
     */
    StringBase(const StringBase &other)
        : m_alloc(other.m_alloc) {
        if (other.is_short_string()) {
            // using long_string is faster than short_string, as we just care about copying the
            // 24-bytes of the union and not the details
            m_data.long_string = other.m_data.long_string;
            set_flag_to_short_string();
        } else {
            const auto alloc_size = (other.m_data.long_string.size + 1) * sizeof(char);
            char *new_ptr = static_cast<char *>(m_alloc->allocate(alloc_size, alignof(char)));

            fr::mem::copy_init_range(other.m_data.long_string.ptr, alloc_size, new_ptr);

            m_data.long_string.ptr = new_ptr;
            m_data.long_string.size = other.m_data.long_string.size;
            m_data.long_string.capacity = other.m_data.long_string.size;

            set_flag_to_long_string();
        }
    }

    /**
     * @brief Move constructor
     *
     * @param other Moved string.
     */
    StringBase(StringBase &&other) noexcept
        : m_alloc(other.m_alloc) {
        m_data.long_string = other.m_data.long_string;

        other.m_data.short_string.text[0] = '\0';
        other.m_data.short_string.space_left = 23;
    }

    /**
     * @brief Destructor
     */
    ~StringBase() {
        if (!is_short_string()) {
            // retrieve long_string's capacity from the two last bits
            USize capacity = m_data.long_string.capacity & extract_capacity_mask;
            m_alloc->deallocate(m_data.long_string.ptr, capacity + 1, alignof(char));
        }
    }

    ////////
    /// OPERATORS
    ////////
    StringBase &operator=(const StringBase &other) = delete;
    StringBase &operator=(StringBase &&other) = delete;

    ////////
    /// GENERAL
    ////////

    /**
     * @brief Swaps this string with the other string.
     *
     * @param other Swapped string.
     */
    void swap(StringBase &other) noexcept {
        auto const t = m_data.long_string;
        m_data.long_string = other.m_data.long_string;
        other.m_data.long_string = t;

        std::swap(m_alloc, other.m_alloc);
    }

    /**
     * @brief Gets the current text size
     *
     * @return Number of characters without null-terminator.
     */
    [[nodiscard]] USize size() const noexcept {
        // if string is short, final_size constains garbage
        USize final_size = m_data.long_string.size;

        // get the last byte of the object (either containing space_left or the long flag)
        const U8 *bytes = reinterpret_cast<const U8 *>(&m_data);
        U8 last_byte = bytes[sizeof(m_data) - 1];

        // max_short_string is 23, so this is like 23 - last_byte
        // if short: last_byte is between 0-23 and the result of this is >=0
        // if long: last_byte is >= 128 and the result of this <0 causing unsigned underflow
        USize maybe_small_size = max_short_string_size - last_byte;

        // by casting maybe_small_size to signed integer, the unsigned underflow becomes a negative
        // number and this operation compiles down to a CMOV instruction, meaning that we can avoid
        // branching
        final_size =
            (static_cast<std::ptrdiff_t>(maybe_small_size) >= 0) ? maybe_small_size : final_size;

        return final_size;
    }

    /**
     * @brief Gets the maximum characters without null-terminator.
     *
     * @return Capacity in characters.
     */
    [[nodiscard]] USize capacity() const noexcept {
        if (is_short_string()) [[likely]]
            return max_short_string_size;

        // uses the mask to mask out the 2 most significant bits to reveal long string's capacity
        return m_data.long_string.capacity & extract_capacity_mask;
    }

    /**
     * @brief Gets a pointer to the raw string data.
     *
     * @return Pointer with null-terminator.
     */
    [[nodiscard]] char *data() noexcept {
        if (is_short_string()) [[likely]]
            return m_data.short_string.text;

        return m_data.long_string.ptr;
    }

    /**
     * @brief Gets a read-only pointer to the raw string data.
     *
     * @return Constant pointer with null-terminator.
     */
    [[nodiscard]] const char *data() const noexcept {
        if (is_short_string()) [[likely]]
            return m_data.short_string.text;

        return m_data.long_string.ptr;
    }

    /**
     * @brief Gets a read-only C-style string.
     *
     * @return Constant C-string.
     */
    [[nodiscard]] const char *c_str() const noexcept {
        return data();
    }

    /**
     * @brief Checks if its a short string (SSO).
     *
     * @return True if using stack buffer.
     */
    [[nodiscard]] bool is_short_string() const noexcept {
        const U8 *bytes = reinterpret_cast<const U8 *>(&m_data);
        U8 last_byte = bytes[sizeof(m_data.long_string) - 1];
        if ((last_byte & extract_flag_mask) == 0)
            return true;

        return false;
    }

    /**
     * @brief Reserves space for characters.
     *
     * @param new_capacity Requested capacity.
     */
    void reserve(USize new_capacity) {
        if (new_capacity <= capacity())
            return;

        const auto alloc_size = new_capacity + 1;

        // STACK -> HEAP
        if (is_short_string()) {
            char *new_ptr = static_cast<char *>(m_alloc->allocate(alloc_size, alignof(char)));

            USize current_size = get_short_string_size();

            fr::mem::copy_init_range(reinterpret_cast<const char *>(&m_data), current_size + 1,
                                     new_ptr);

            m_data.long_string.ptr = new_ptr;
            m_data.long_string.size = current_size;
            m_data.long_string.capacity = new_capacity;

            set_flag_to_long_string();
        } else { // HEAP -> BIGGER HEAP
            USize old_alloc_size = capacity() + 1;

            m_data.long_string.ptr = static_cast<char *>(m_alloc->reallocate(
                m_data.long_string.ptr, old_alloc_size, alloc_size, alignof(char)));
            m_data.long_string.capacity = new_capacity;

            set_flag_to_long_string();
        }
    }

    /**
     * @brief Frees unused allocated capacity.
     */
    void shrink_to_fit() {
        if (is_short_string()) {
            return;
        }

        USize current_size = m_data.long_string.size;
        USize current_capacity = m_data.long_string.capacity & extract_capacity_mask;

        // HEAP -> STACK
        if (current_size <= max_short_string_size) {
            char *old_ptr = m_data.long_string.ptr;
            char buf[max_short_string_size + 1];
            fr::mem::copy_raw_range(old_ptr, current_size + 1, buf);

            m_alloc->deallocate(old_ptr, current_capacity + 1, alignof(char));

            fr::mem::copy_raw_range(buf, current_size + 1, reinterpret_cast<char *>(&m_data));
            m_data.short_string.space_left = static_cast<U8>(max_short_string_size - current_size);
        } else { // HEAP -> SMALLER HEAP
            if (current_capacity == current_size)
                return;

            USize alloc_size = current_size + 1;
            USize old_alloc_size = current_capacity + 1;

            char *new_ptr = static_cast<char *>(m_alloc->reallocate(
                m_data.long_string.ptr, old_alloc_size, alloc_size, alignof(char)));

            m_data.long_string.ptr = new_ptr;
            m_data.long_string.capacity = current_size;
            set_flag_to_long_string();
        }
    }

    /**
     * @brief Retrieves the size of short string directly.
     *
     * @return Size in characters.
     */
    [[nodiscard]] USize get_short_string_size() const noexcept {
        FR_ASSERT(is_short_string(), "not a short string");

        return max_short_string_size - m_data.short_string.space_left;
    }

    /**
     * @brief Retrieves the size of long string directly.
     *
     * @return Size in characters.
     */
    [[nodiscard]] USize get_long_string_size() const noexcept {
        FR_ASSERT(!is_short_string(), "not a long string");
        return m_data.long_string.size;
    }

protected:
    /**
     * @brief Updates the size of the string.
     *
     * @param new_size New characteer count.
     */
    void set_size(USize new_size) noexcept {
        FR_ASSERT(new_size <= capacity(), "size overflow");

        if (is_short_string()) {
            if (new_size < max_short_string_size)
                m_data.short_string.text[new_size] = '\0';

            // if new_size == 23, space_left become 0 becoming the null terminator character
            // alligned at the end of the union allowing the storage of 23 characters instead of 22
            m_data.short_string.space_left = static_cast<U8>(max_short_string_size - new_size);
        } else {
            m_data.long_string.size = new_size;
            m_data.long_string.ptr[new_size] = '\0';
        }
    }

private:
    /**
     * @brief Sets the string flag to short string.
     */
    void set_flag_to_short_string() noexcept {
        // Get the bytes of the union
        U8 *bytes = reinterpret_cast<U8 *>(&m_data);
        // Clear the last two bits to 00
        bytes[sizeof(m_data) - 1] &= ~extract_flag_mask;
    }

    /**
     * @brief Sets the string flag to long string.
     */
    void set_flag_to_long_string() noexcept {
        U8 *bytes = reinterpret_cast<U8 *>(&m_data);
        bytes[sizeof(m_data) - 1] &= ~extract_flag_mask;
        // Set the last two bits to 10
        bytes[sizeof(m_data) - 1] |= (U8(2) << 6);
    }

    FR_STATIC_ASSERT(std::endian::native == std::endian::little,
                     "SSO bitmasking requires little-endian architecture");

    static constexpr USize max_short_string_size = 23;

    static const U8 extract_flag_mask = 0xC0;
    static const USize extract_capacity_mask =
        ~(USize(extract_flag_mask) << (8 * (sizeof(USize) - 1)));

    fr::Alloc *m_alloc;
    union {
        struct {
            char text[max_short_string_size];
            U8 space_left;

        } short_string;

        struct {
            char *ptr;
            USize size;
            USize capacity;
        } long_string;
    } m_data;
};

} // namespace fr::impl
