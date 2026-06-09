/**
 * @file standard_sink.hpp
 * @author Stachu
 * @brief Defines the StandardSink class for writing raw logs to the standard output
 */

#pragma once

#include "fr/core/timestamp.hpp"
#include "fr/logger/sink.hpp"

namespace fr {

/**
 * @brief A built-in, standard implementation of the Sink interface.
 * * Handles local formatting preferences such as alternative level names and custom timestamps.
 */
class StandardSink : public Sink {
public:
    /**
     * @brief Configuration settings that control how logs are formatted in this sink.
     */
    struct Options {
        /** @brief Preferences for displaying date and time */
        TimestampFormatOptions timestampFormatOptions{TimestampFormatOptions{}};

        /** @brief Truncates severity tags (e.g. using "ERR" vs "ERROR") */
        bool shorterLevelNames{false};
    };

    /**
     * @brief Constructs a StandardSink with default formatting preferences.
     */
    StandardSink() noexcept = default;

    /**
     * @brief Constructs a StandardSink with specific formatting customisations.
     * @param options Configuration options for this sink instance.
     */
    explicit StandardSink(const Options &options) noexcept
        : m_options{options} {
    }

    /**
     * @brief Default virtual destructor.
     */
    ~StandardSink() noexcept override = default;

    /**
     * @brief Formats and writes the log record to the standard stream.
     * @param log_entry The structured log record payload.
     */
    void write(const Log &log_entry) noexcept override;

    /**
     * @brief Flushes the internal stream buffer.
     */
    void flush() noexcept override;

private:
    Options m_options;
};

} // namespace fr
