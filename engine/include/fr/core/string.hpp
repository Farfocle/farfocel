/**
 * @file string.hpp
 * @author Tfoedy
 *
 * @brief String
 */

#pragma once

#include "fr/core/string_base.hpp"

namespace fr {

class String : public fr_stl_str::StringBase {
public:
    static constexpr USize npos = static_cast<USize>(-1);

    using fr_stl_str::StringBase::StringBase;

    ////////
    /// CONSTRUCTORS, ASSIGNERS, DESTRUCTORS
    ////////

    String() {
    }

    // with null-terminator
    String(const char *str) {
    }

    String(const char *str, size_t count) {
    }

    String(size_t count, char char_to_repeat) {
    }

    String(const String &other) {
    }

    String(String &&other) noexcept {
    }

    String &operator=(String other) noexcept {
    }

    String &assign(size_t count, char char_to_repeat) {
    }

    ////////
    /// ITERATORS
    ////////

    // 1. begin
    // 2. cbegin
    // 3. end
    // 4. cend

    ////////
    /// ACCESSORS
    ////////

    char &operator[](size_t pos) {
    }
    // const version

    char &front() {
    }
    // const version

    char &back() {
    }
    // const version

    ////////
    /// CAPACITY
    ////////

    void resize(size_t count, char char_to_repeat = '\0') {
    }

    void clear() noexcept {
    }

    ////////
    /// MODIFIERS
    ////////

    void push_back(char character) {
    }

    void pop_back() {
    }

    String &append(const char *str, size_t count) {
    }

    String &append(const char *str) {
    }

    String &append(size_t count, char character) {
    }

    String &operator+=(const String &str) {
    }

    String &operator+=(const char *str) {
    }

    // below slow modifiers requiring memory move
    String &insert(size_t index, const char *str, size_t count) {
    }

    String &insert(size_t index, size_t count, char character) {
    }

    String &erase(size_t index = 0, size_t count = 0) {
    }

    String &replace(size_t pos, size_t count, const char *str, size_t str_count) {
    }

    ////////
    /// SEARCH
    ////////

    size_t find(const char *str, size_t pos = 0) const {
    }

    size_t find(char character, size_t pos = 0) const {
    }

    size_t reverse_find(const char *str, size_t pos) const {
    }

    size_t find_first_of(const char *chars, size_t pos = 0) const {
    }

    size_t find_first_not_of(const char *chars, size_t pos = 0) const {
    }

    size_t find_last_of(const char *chars, size_t pos = 0) const {
    }

    size_t find_last_not_of(const char *chars, size_t pos = 0) const {
    }

    ////////
    /// OTHER & QOL
    ////////

    String substr(size_t pos, size_t count) const {
    }

    int compare(const String &str) const {
    }

    bool starts_with(const char *str) const {
    }

    bool ends_with(const char *str) const {
    }

    bool contains(const char *str) const {
    }

    void trim() {
    }

    void to_lower_ascii() {
    }

    void to_upper_ascii() {
    }

    fr::Slice<fr::String> split(char delimiter) const {
    }

private:
};

// plus operatory ==, !=, <, >, +, <<

} // namespace fr
