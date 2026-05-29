#pragma once

#include "fr/logger/sink.hpp"

namespace fr {

class StandardSink : public Sink {
public:
    StandardSink() noexcept = default;
    ~StandardSink() noexcept override = default;

    void write(const Log &log_entry) noexcept override;
    void flush() noexcept override;
};

} // namespace fr
