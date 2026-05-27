#include "fr/logger/logger.hpp"
#include "fr/core/string.hpp"
#include <iostream>

void Logger::log(fr::StringView msg) {
    std::cout << msg;
}
