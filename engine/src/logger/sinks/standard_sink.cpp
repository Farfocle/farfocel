#include "fr/logger/sinks/standard_sink.hpp"
#include "fr/core/format.hpp"
#include "fr/logger/log.hpp"
#include <iostream>

namespace fr {

static const char *level_to_string(LogLevel level) {
    switch (level) {
    case LogLevel::Success:
        return "[SUCCESS] ";
    case LogLevel::Error:
        return "[ERROR]   ";
    case LogLevel::Critical:
        return "[CRITICAL]";
    default:
        return "          ";
    }
}

void StandardSink::write(const Log &log_entry) noexcept {
    try {
        String buffer = format("{} {} {}\n", log_entry.timestamp, level_to_string(log_entry.level),
                               log_entry.message);
        std::cout.write(buffer.data(), buffer.size());
    } catch (...) {
    }
}

void StandardSink::flush() noexcept {
    std::cout.flush();
}

} // namespace fr
