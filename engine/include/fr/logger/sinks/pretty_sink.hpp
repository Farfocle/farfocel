#pragma once

#include "fr/core/timestamp.hpp"
#include "fr/logger/sink.hpp"

namespace fr {

class PrettySink : public Sink {
public:
    struct Options {
        TimestampFormatOptions timestampFormatOptions{TimestampFormatOptions{}};
        bool shorterLevelNames{false};
    };

    PrettySink() noexcept = default;
    explicit PrettySink(const Options &options) noexcept
        : m_options{options} {
    }
    ~PrettySink() noexcept override = default;

    void write(const Log &log_entry) noexcept override;
    void flush() noexcept override;

private:
    Options m_options;
};

} // namespace fr
