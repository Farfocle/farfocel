#pragma once

namespace fr {

struct Log;

class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(const Log& log_entry) noexcept = 0;
    virtual void flush() noexcept = 0;
};

} // namespace fr
