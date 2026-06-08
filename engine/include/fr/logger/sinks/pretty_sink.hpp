/**
 * @file pretty_sink.hpp
 * @autor Stachu
 * @brief Defines the PrettySink class for human-friendly, stylized log output formatting.
 */

#pragma once

#include "fr/core/timestamp.hpp"
#include "fr/logger/sink.hpp"

namespace fr {

/**
 * @brief A stylized implementation of the Sink interface.
 * * Formats log entries with human-centric presentation enhancements, such as ANSI
 * terminal colors and custom timestamp styling.
 */
class PrettySink : public Sink {
public:
    /**
     * @brief Configuration settings that control how logs are styled in this sink.
     */
    struct Options {
        /** @brief Preferences for displaying date, time, or fractional seconds. */
        TimestampFormatOptions timestampFormatOptions{TimestampFormatOptions{}};

        /** @brief Truncates severity tags (e.g. using "OK" vs "SUCCESS"). */
        bool shorterLevelNames{false};
    };

    /**
     * @brief Constructs a PrettySink with default styling and formatting options.
     */
    PrettySink() noexcept = default;

    /**
     * @brief Constructs a PrettySink with specific styling and formatting adjustments.
     * @param options Configuration ruleset for this sink instance.
     */
    explicit PrettySink(const Options &options) noexcept
        : m_options{options} {
    }

    /**
     * @brief Default virtual destructor.
     */
    ~PrettySink() noexcept override = default;

    /**
     * @brief Transforms, stylizes, and writes the log record to the target stream.
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
