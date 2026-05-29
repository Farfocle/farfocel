#include "fr/logger/sinks/standard_sink.hpp"
#include <iostream>

namespace fr {
void StandardSink::write(StringView msg) noexcept {
    std::cout.write(msg.data(), msg.size());
    std::cout << '\n';
}

void StandardSink::flush() noexcept {
    std::cout.flush();
}
} // namespace fr
