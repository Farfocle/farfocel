#pragma once

#include "fr/core/string_view.hpp"
#include "fr/logger/sink.hpp"

namespace fr {

class StandardSink : public Sink {
public:
    StandardSink() noexcept = default;
    ~StandardSink() noexcept override = default;

    void write(StringView msg) noexcept override;
    void flush() noexcept override;
};

} // namespace fr
