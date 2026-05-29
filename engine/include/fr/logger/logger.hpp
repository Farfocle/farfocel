#pragma once

#include "fr/core/format.hpp"
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
    void enqueue(String msg);

public:
    Logger();
    ~Logger();

    void add_sink(UniquePtr<Sink> sink);

    template <typename T>
    void log(T && arg) {
        enqueue(format("{}", std::forward<T>(arg)));
    }

    template <typename... Ts>
    void log(StringView fmt, Ts &&...args) {
        enqueue(format(fmt, std::forward<Ts>(args)...));
    }

    void log(StringView msg);
};
} // namespace fr
