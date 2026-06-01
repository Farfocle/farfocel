/**
 * @file logger.hpp
 * @author Stachu
 * @brief Defines the thread-safe, asynchronous Logger class and helper macros.
 */

#pragma once

#include "fr/core/format.hpp"
#include "fr/core/queue.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/log.hpp"
#include "fr/logger/sink.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace fr {

/**
 * @brief Asynchronous logger that dispatches queued logs to registered sinks via a background
 * worker thread.
 */
class Logger {
private:
    Queue<Log> queue;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;
    bool should_close = false;

    DynamicArray<UniquePtr<Sink>> sinks;
    std::mutex sinks_mtx;

    /**
     * @brief Background worker loop that flushes and writes queued logs to all sinks.
     */
    void process_logs();

    /**
     * @brief Enqueues a new log entry into the lock-protected queue.
     * @param level The severity tier of the message.
     * @param msg The log content payload.
     */
    void enqueue(LogLevel level, String msg);

public:
    /**
     * @brief Constructs the logger instance and spins up the asynchronous worker thread.
     */
    Logger();

    /**
     * @brief Flushes pending logs to sinks, and joins the background thread
     * safely.
     */
    ~Logger();

    /**
     * @brief Registers an output target interface (sink) to consume processed log records.
     * @param sink a unique pointer to a concrete implementation of Sink.
     */
    void add_sink(UniquePtr<Sink> sink);

    /**
     * @brief Formats and logs a single standard or custom object.
     */
    template <typename T>
    void log(LogLevel level, T &&arg) {
        enqueue(level, format("{}", std::forward<T>(arg)));
    }

    /**
     * @brief Formats and logs a structured message string matching standard placeholders.
     * @details Uses fr::format
     * @param level The severity tier of the message.
     * @param fmt The format string view containing placement tokens.
     */
    template <typename... Ts>
    void log(LogLevel level, StringView fmt, Ts &&...args) {
        enqueue(level, format(fmt, std::forward<Ts>(args)...));
    }

    /**
     * @brief Logs a raw string view message.
     */
    void log(LogLevel level, StringView msg);
};
} // namespace fr

// ------------------------------------------------------------------ Macros

/**
 * @brief Base macro to dispatch parameters directly to the global ambient logger instance if
 * active.
 */
#define _FR_LOG_BASE(level, ...)                                                                   \
    if (auto *_logger = ::fr::get_ambient_ctx().logger) {                                          \
        _logger->log(level, __VA_ARGS__);                                                          \
    }

/** @brief Logs standard informational tracing output. */
#define FR_LOG(...) _FR_LOG_BASE(::fr::LogLevel::Info, __VA_ARGS__)

/** @brief Logs successful operational results or confirmation actions. */
#define FR_LOG_OK(...) _FR_LOG_BASE(::fr::LogLevel::Success, __VA_ARGS__)

/** @brief Logs non-breaking anomalies that require systemic visibility. */
#define FR_LOG_WARN(...) _FR_LOG_BASE(::fr::LogLevel::Warning, __VA_ARGS__)

/** @brief Logs runtime failures or unexpected exception paths. */
#define FR_LOG_ERR(...) _FR_LOG_BASE(::fr::LogLevel::Error, __VA_ARGS__)

/** @brief Logs critical failures */
#define FR_LOG_CRIT(...) _FR_LOG_BASE(::fr::LogLevel::Critical, __VA_ARGS__)
