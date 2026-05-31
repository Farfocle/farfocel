#pragma once

#include "fr/core/string.hpp"
#include "fr/core/timestamp.hpp"

namespace fr {
enum class LogLevel {
    Info,
    Success,
    Warning,
    Error,
    Critical
};

struct Log {
    LogLevel level;
    Timestamp timestamp;
    String message;
};
} // namespace fr
