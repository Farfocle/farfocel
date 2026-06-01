#include "fr/logger/sinks/standard_sink.hpp"
#include "fr/core/format.hpp"
#include "fr/core/timestamp.hpp"
#include "fr/logger/log.hpp"
#include <iostream>

namespace fr {

static const char *level_to_string(LogLevel level, bool short_names) {
    if (short_names) {
        switch (level) {
        case LogLevel::Success:
            return "[OK] ";
        case LogLevel::Error:
            return "[ERR] ";
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

void StandardSink::write(const Log &log_entry) noexcept {
    try {
        String ts = log_entry.timestamp.to_string(m_options.timestampFormatOptions);

        String buffer =
            format("{} {} {}\n", ts, level_to_string(log_entry.level, m_options.shorterLevelNames),
                   log_entry.message);
        std::cout.write(buffer.data(), buffer.size());
    } catch (...) {
    }
}

void StandardSink::flush() noexcept {
    std::cout.flush();
}

} // namespace fr
