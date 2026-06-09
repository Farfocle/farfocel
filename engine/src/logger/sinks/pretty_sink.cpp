#include <iostream>

#include "fr/core/format.hpp"
#include "fr/core/timestamp.hpp"
#include "fr/logger/log.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"

namespace fr {

namespace {
// ANSI Escape Codes for styling
constexpr const char *STYLE_RESET = "\033[0m";
constexpr const char *STYLE_BOLD = "\033[1m";
constexpr const char *COLOR_DIM = "\033[2m";
constexpr const char *COLOR_GREEN = "\033[32m";
constexpr const char *COLOR_YELLOW = "\033[33m";
constexpr const char *COLOR_RED = "\033[31m";
constexpr const char *COLOR_MAGENTA = "\033[35m";

const char *level_to_string(LogLevel level, bool short_names) {
    if (short_names) {
        switch (level) {
        case LogLevel::Success:
            return "[OK] ";
        case LogLevel::Error:
            return "[ERR]";
        case LogLevel::Critical:
            return "[CRT]";
        case LogLevel::Warning:
            return "[WRN]";
        default:
            return "[LOG]";
        }
    } else {
        switch (level) {
        case LogLevel::Success:
            return "[SUCCESS] ";
        case LogLevel::Error:
            return "[ERROR]   ";
        case LogLevel::Critical:
            return "[CRITICAL]";
        case LogLevel::Warning:
            return "[WARNING] ";
        default:
            return "[LOG]     ";
        }
    }
}

const char *level_to_color(LogLevel level) {
    switch (level) {
    case LogLevel::Success:
        return COLOR_GREEN;
    case LogLevel::Error:
        return COLOR_RED;
    case LogLevel::Critical:
        return COLOR_MAGENTA;
    case LogLevel::Warning:
        return COLOR_YELLOW;
    default:
        return "";
    }
}
} // namespace

void PrettySink::write(const Log &log_entry) noexcept {
    try {
        String ts = log_entry.timestamp.to_string(m_options.timestampFormatOptions);
        String buffer;

        buffer = format("{}{}{}{}{}{}{}{} {}{}{}\n", COLOR_DIM, ts, STYLE_RESET,
                        ts.size() == 0 ? "" : " ", STYLE_BOLD, level_to_color(log_entry.level),
                        level_to_string(log_entry.level, m_options.shorterLevelNames), STYLE_RESET,
                        level_to_color(log_entry.level), log_entry.message, STYLE_RESET);

        std::cout.write(buffer.data(), buffer.size());
    } catch (...) {
    }
}

void PrettySink::flush() noexcept {
    std::cout.flush();
}

} // namespace fr
