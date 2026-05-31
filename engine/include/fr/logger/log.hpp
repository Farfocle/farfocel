#pragma once

#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"

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
    U64 timestamp;
    String message;
};
} // namespace fr
