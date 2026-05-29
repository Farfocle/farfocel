#pragma once

#include "fr/core/queue.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace fr {
class Logger {
private:
    Queue<String> queue;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;
    bool should_close = false;
    void process_logs();

public:
    Logger();
    ~Logger();
    void log(StringView msg);
};
} // namespace fr
