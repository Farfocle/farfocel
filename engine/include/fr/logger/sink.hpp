#pragma once

#include "fr/core/string_view.hpp"

namespace fr {
class Sink {
public:
    virtual ~Sink() noexcept = default;
    virtual void write(StringView msg) noexcept = 0;
    virtual void flush() noexcept = 0;
};
} // namespace fr
