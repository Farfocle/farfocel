/**
 * @file   timestamp.hpp
 * @author Stachu
 * @brief  Timestamp class and utilities for date/time manipulation and formatting.
 */

#pragma once

#include "fr/core/string.hpp"
#include "fr/core/time.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

/**
 * @brief Represents a decomposed UTC date and time.
 */
struct DateTime {
    S32 year;
    U8 month;
    U8 day;
    U8 hour;
    U8 minute;
    U8 second;
    U16 millisecond;
};

/**
 * @brief Configuration choices for formatting a timestamp string.
 */
struct TimestampFormatOptions {
    bool date = true;
    bool time = true;
    bool milliseconds = true;
};

/**
 * @brief Represents a point in time measured in milliseconds since the epoch.
 */
class Timestamp {
private:
    U64 ms_since_epoch;

public:
    /**
     * @brief Constructs a timestamp initialized to zero milliseconds.
     */
    Timestamp() noexcept
        : ms_since_epoch(0) {
    }

    /**
     * @brief Constructs a timestamp with a specific duration in milliseconds.
     */
    explicit Timestamp(U64 ms) noexcept
        : ms_since_epoch(ms) {
    }

    /**
     * @brief Creates a Timestamp representing the current system time.
     */
    [[nodiscard]] static Timestamp now() noexcept {
        return Timestamp(time::get_system_now_ms());
    }

    // ------------------------------------------------------ Time parts getters

    /**
     * @brief Gets the raw milliseconds since epoch.
     */
    [[nodiscard]] U64 get_ms() const noexcept {
        return ms_since_epoch;
    }

    /**
     * @brief Gets the millisecond component of the current second.
     */
    [[nodiscard]] U16 get_millisecond() const noexcept {
        return static_cast<U16>(ms_since_epoch % 1000);
    }

    /**
     * @brief Gets the second component of the current minute.
     */
    [[nodiscard]] U8 get_second() const noexcept {
        return static_cast<U8>((ms_since_epoch / 1000) % 60);
    }

    /**
     * @brief Gets the minute component of the current hour.
     */
    [[nodiscard]] U8 get_minute() const noexcept {
        return static_cast<U8>((ms_since_epoch / 60000) % 60);
    }

    /**
     * @brief Gets the hour component of the current day.
     */
    [[nodiscard]] U8 get_hour() const noexcept {
        return static_cast<U8>((ms_since_epoch / 3600000) % 24);
    }

    /**
     * @brief Decomposes the timestamp into a full UTC DateTime struct.
     */
    [[nodiscard]] DateTime to_utc_datetime() const noexcept {
        const U64 total_seconds = ms_since_epoch / 1000;
        const U16 ms = static_cast<U16>(ms_since_epoch % 1000);

        const U64 total_minutes = total_seconds / 60;
        const U8 ss = static_cast<U8>(total_seconds % 60);

        const U64 total_hours = total_minutes / 60;
        const U8 mm = static_cast<U8>(total_minutes % 60);

        const U64 days = total_hours / 24;
        const U8 hh = static_cast<U8>(total_hours % 24);

        // Howard Hinnant's algorithm for epoch days to civil date
        const S32 z = static_cast<S32>(days) + 719468;
        const S32 era = (z >= 0 ? z : z - 146096) / 146097;
        const U32 doe = static_cast<U32>(z - era * 146097);
        const U32 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;

        S32 y = static_cast<S32>(yoe) + era * 400;
        const U32 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const U32 mp = (5 * doy + 2) / 153;
        const U32 d = doy - (153 * mp + 2) / 5 + 1;
        const U32 m = mp < 10 ? mp + 3 : mp - 9;

        y += (m <= 2 ? 1 : 0);

        return DateTime{y, static_cast<U8>(m), static_cast<U8>(d), hh, mm, ss, ms};
    }

    // ------------------------------------------------------ Date parts getters

    /**
     * @brief Gets the calendar year from the UTC representation.
     */
    [[nodiscard]] S32 get_year() const noexcept {
        return to_utc_datetime().year;
    }

    /**
     * @brief Gets the calendar month (1-12) from the UTC representation.
     */
    [[nodiscard]] U8 get_month() const noexcept {
        return to_utc_datetime().month;
    }

    /**
     * @brief Gets the day of the month (1-31) from the UTC representation.
     */
    [[nodiscard]] U8 get_day() const noexcept {
        return to_utc_datetime().day;
    }

    // ------------------------------------------------------- String formatting
private:
    /**
     * @brief Internal helper to format the time components into a string buffer.
     */
    [[nodiscard]] String to_time_string_impl(TimestampFormatOptions options) const {
        char buf[12]; // for "HH:MM:SS.mmm"
        USize idx = 0;

        const U8 hour = get_hour();
        const U8 minute = get_minute();
        const U8 second = get_second();
        const U16 millisecond = get_millisecond();

        buf[idx++] = static_cast<char>('0' + (hour / 10));
        buf[idx++] = static_cast<char>('0' + (hour % 10));
        buf[idx++] = ':';
        buf[idx++] = static_cast<char>('0' + (minute / 10));
        buf[idx++] = static_cast<char>('0' + (minute % 10));
        buf[idx++] = ':';
        buf[idx++] = static_cast<char>('0' + (second / 10));
        buf[idx++] = static_cast<char>('0' + (second % 10));

        if (options.milliseconds) {
            buf[idx++] = '.';
            buf[idx++] = static_cast<char>('0' + (millisecond / 100));
            buf[idx++] = static_cast<char>('0' + ((millisecond / 10) % 10));
            buf[idx++] = static_cast<char>('0' + (millisecond % 10));
        }

        return String::from_sized_chars(buf, idx);
    }

public:
    /**
     * @brief Formats the timestamp into a human-readable String based on options.
     */
    [[nodiscard]] String to_string(TimestampFormatOptions options = {}) const {

        // nothing requested
        if (!options.date && !options.time) {
            return String::from_sized_chars("", 0);
        }

        // Time-only string (bypasses calendar math)
        if (!options.date) {
            return to_time_string_impl(options);
        }

        // Date requested
        const DateTime dt = to_utc_datetime();

        char buf[23];
        USize idx = 0;

        // Extract Date components
        buf[idx++] = static_cast<char>('0' + (dt.year / 1000));
        buf[idx++] = static_cast<char>('0' + ((dt.year / 100) % 10));
        buf[idx++] = static_cast<char>('0' + ((dt.year / 10) % 10));
        buf[idx++] = static_cast<char>('0' + (dt.year % 10));
        buf[idx++] = '-';
        buf[idx++] = static_cast<char>('0' + (dt.month / 10));
        buf[idx++] = static_cast<char>('0' + (dt.month % 10));
        buf[idx++] = '-';
        buf[idx++] = static_cast<char>('0' + (dt.day / 10));
        buf[idx++] = static_cast<char>('0' + (dt.day % 10));

        if (options.time) {
            buf[idx++] = ' ';
            buf[idx++] = static_cast<char>('0' + (dt.hour / 10));
            buf[idx++] = static_cast<char>('0' + (dt.hour % 10));
            buf[idx++] = ':';
            buf[idx++] = static_cast<char>('0' + (dt.minute / 10));
            buf[idx++] = static_cast<char>('0' + (dt.minute % 10));
            buf[idx++] = ':';
            buf[idx++] = static_cast<char>('0' + (dt.second / 10));
            buf[idx++] = static_cast<char>('0' + (dt.second % 10));

            if (options.milliseconds) {
                buf[idx++] = '.';
                buf[idx++] = static_cast<char>('0' + (dt.millisecond / 100));
                buf[idx++] = static_cast<char>('0' + ((dt.millisecond / 10) % 10));
                buf[idx++] = static_cast<char>('0' + (dt.millisecond % 10));
            }
        }

        return String::from_sized_chars(buf, idx);
    }
};

} // namespace fr
