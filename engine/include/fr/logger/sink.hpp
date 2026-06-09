/**
 * @file sink.hpp
 * @author Stachu
 * @brief Defines the Sink interface for abstracting log output destinations.
 */

#pragma once

namespace fr {

struct Log;

/**
 * @brief Abstract base class representing a log destination.
 * * Custom output targets (e.g., file, console) must inherit from Sink
 * and implement its pure virtual interface.
 */
class Sink {
public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of derived sinks.
     */
    virtual ~Sink() = default;

    /**
     * @brief Writes a single log record to the underlying destination.
     * @param log_entry The structured log record containing metadata and message payload.
     */
    virtual void write(const Log &log_entry) noexcept = 0;

    /**
     * @brief Flushes any buffered log data.
     */
    virtual void flush() noexcept = 0;
};

} // namespace fr
