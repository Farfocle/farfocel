#pragma once

#include "fr/core/timestamp.hpp"
#include "fr/logger/sink.hpp"

namespace fr {

class StandardSink : public Sink {
public:
    struct Options {
        TimestampFormatOptions timestampFormatOptions{TimestampFormatOptions{}};
        bool shorterLevelNames{false};
    };

    StandardSink() noexcept = default;
    explicit StandardSink(const Options &options) noexcept
        : m_options{options} {
    }
    ~StandardSink() noexcept override = default;

    void write(const Log &log_entry) noexcept override;
    void flush() noexcept override;

private:
    Options m_options;
};

} // namespace fr
