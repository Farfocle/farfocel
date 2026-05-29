#include "fr/logger/logger.hpp"
#include "fr/core/queue.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/unique_ptr.hpp"

namespace fr {
Logger::Logger() {
    worker = std::thread(&Logger::process_logs, this);
}

Logger::~Logger() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        should_close = true;
    }
    cv.notify_one();
    if (worker.joinable()) {
        worker.join();
    }
}

void Logger::add_sink(UniquePtr<Sink> sink) {
    std::lock_guard<std::mutex> lock(sinks_mtx);
    sinks.push_back(std::move(sink));
}

void Logger::log(StringView msg) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        queue.enqueue(String(msg));
    }
    cv.notify_one();
}

void Logger::enqueue(String msg) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        queue.enqueue(std::move(msg));
    }
    cv.notify_one();
}

void Logger::process_logs() {
    Queue<String> local_queue;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]() { return !queue.is_empty() || should_close; });

            if (queue.is_empty() && should_close) {
                std::lock_guard<std::mutex> sink_lock(sinks_mtx);
                for (auto &sink : sinks) {
                    sink->flush();
                }
                return;
            }

            std::swap(queue, local_queue);
        }

        while (!local_queue.is_empty()) {
            String msg = std::move(local_queue.front());
            local_queue.dequeue();

            std::lock_guard<std::mutex> sink_lock(sinks_mtx);
            for (auto &sink : sinks) {
                sink->write(msg);
            }
        }
    }
}

} // namespace fr
