/**
 * @file log.hpp
 * @author Stachu
 * @brief Defines log severity levels and the core Log data structure.
 */

#pragma once

#include "fr/core/string.hpp"
#include "fr/core/timestamp.hpp"

namespace fr {

/**
 * @brief Severity levels used to categorize log messages.
 */
enum class LogLevel { Info, Success, Warning, Error, Critical };

/**
 * @brief Represents a single log entry containing metadata and the message text.
 */
struct Log {
    LogLevel level;
    Timestamp timestamp;
    String message;
};
} // namespace fr
