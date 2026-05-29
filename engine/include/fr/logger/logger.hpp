#pragma once

#include "fr/core/queue.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/sink.hpp"
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

    DynamicArray<UniquePtr<Sink>> sinks;
    std::mutex sinks_mtx;
    void process_logs();

public:
    Logger();
    ~Logger();
    void log(StringView msg);
    void add_sink(UniquePtr<Sink> sink);
};
} // namespace fr
