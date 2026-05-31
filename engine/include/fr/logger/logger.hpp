#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include "fr/core/format.hpp"
#include "fr/core/queue.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/log.hpp"
#include "fr/logger/sink.hpp"

namespace fr {
class Logger {
private:
    Queue<Log> queue;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;
    bool should_close = false;

    DynamicArray<UniquePtr<Sink>> sinks;
    std::mutex sinks_mtx;

    void process_logs();
    void enqueue(LogLevel level, String msg);

public:
    Logger();
    ~Logger();

    void add_sink(UniquePtr<Sink> sink);

    template <typename T>
    void log(LogLevel level, T && arg) {
        enqueue(level, format("{}", std::forward<T>(arg)));
    }

    template <typename... Ts>
    void log(LogLevel level, StringView fmt, Ts &&...args) {
        enqueue(level, format(fmt, std::forward<Ts>(args)...));
    }

    void log(LogLevel level, StringView msg);
};
} // namespace fr


// ------------------------------------------------------------------ Macros
#define _FR_LOG_BASE(level, ...) \
    if (auto* _logger = ::fr::get_ambient_ctx().logger) { \
        _logger->log(level, __VA_ARGS__); \
    }

#define FR_LOG(...) _FR_LOG_BASE(::fr::LogLevel::Info,  __VA_ARGS__)
#define FR_LOG_OK(...) _FR_LOG_BASE(::fr::LogLevel::Success, __VA_ARGS__)
#define FR_LOG_WARN(...) _FR_LOG_BASE(::fr::LogLevel::Warning, __VA_ARGS__)
#define FR_LOG_ERR(...) _FR_LOG_BASE(::fr::LogLevel::Error, __VA_ARGS__)
#define FR_LOG_CRIT(...) _FR_LOG_BASE(::fr::LogLevel::Critical, __VA_ARGS__)
