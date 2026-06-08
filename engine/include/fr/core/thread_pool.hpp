/**
 * @file thread_pool.hpp
 * @author Kiju
 *
 * @brief Simple thread pool implementation.
 * @details Uses `fr::Queue` to store tasks and `std::thread` to execute them. This implemention is
 * not suitable for a large number of tasks. Because it uses a mutex on the queue, it may not scale
 * well (it definitely will not scale well).
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

#include "fr/core/alloc.hpp"
#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/inline_function.hpp"
#include "fr/core/queue.hpp"

namespace fr {

/**
 * @brief Thread pool class.
 * @note The `Task` type is an `InlineFunction` with a capacity of 128 bytes.
 * @details Manages a pool of threads that execute tasks from a queue.
 */
class ThreadPool {
public:
    // ---------------------------------------------------------------- Typedefs
    using Task = Fn128<void(void)>;
    static constexpr USize MAX_THREADS = 64;

private:
    // ----------------------------------------------------------------- Members
    Queue<Task> m_tasks{};
    USize m_thread_count{std::thread::hardware_concurrency()};
    Array<std::thread, MAX_THREADS> m_threads{};
    std::atomic<USize> m_running_count{0};
    bool m_stop{false};

    std::mutex m_mutex{};
    std::condition_variable m_work_cv{};
    std::condition_variable m_done_cv{};

public:
    // ------------------------------------------------------------ Constructors

    ThreadPool(USize thread_count = std::thread::hardware_concurrency()) noexcept
        : fr::ThreadPool(get_ambient_ctx().alloc, thread_count) {
    }

    explicit ThreadPool(Alloc *alloc,
                        USize thread_count = std::thread::hardware_concurrency()) noexcept
        : m_tasks(alloc),
          m_thread_count(thread_count) {
        for (USize i = 0; i < m_thread_count; ++i) {
            m_threads[i] = std::thread([this] { do_work(); });
        }
    }

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    ~ThreadPool() noexcept {
        stop();
    }

    // --------------------------------------------------------------------- API

    /// @brief Returns the number of threads in the pool.
    USize thread_count() const noexcept {
        return m_thread_count;
    }

    /// @brief Returns the number of running threads in the pool.
    USize running_count() const noexcept {
        return m_running_count.load(std::memory_order_relaxed);
    }

    /**
     * @brief Submits a task to the thread pool.
     * @param task The task to submit.
     */
    void submit(Task task) noexcept {
        {
            std::lock_guard lock(m_mutex);
            m_tasks.enqueue(std::move(task));
        }

        m_work_cv.notify_one();
    }

    /// @brief Waits for all tasks to complete.
    void wait() noexcept {
        std::unique_lock lock(m_mutex);
        m_done_cv.wait(lock, [this] {
            return m_tasks.is_empty() && m_running_count.load(std::memory_order_relaxed) == 0;
        });
    }

    /// @brief Stops the thread pool. Waits for all tasks to complete.
    void stop() noexcept {
        {
            std::lock_guard lock(m_mutex);

            if (m_stop) {
                return;
            }

            m_stop = true;
        }

        m_work_cv.notify_all();
        for (USize i = 0; i < m_thread_count; ++i) {
            if (m_threads[i].joinable()) {
                m_threads[i].join();
            }
        }
    }

private:
    // --------------------------------------------------------------- Internals

    void do_work() {
        while (true) {
            Task task;

            {
                std::unique_lock lock(m_mutex);
                m_work_cv.wait(lock, [this] { return !m_tasks.is_empty() || m_stop; });

                if (m_tasks.is_empty() || m_stop) {
                    return;
                }

                task = std::move(m_tasks.front());
                m_tasks.dequeue();
                ++m_running_count;
            }

            task();
            --m_running_count;
            m_done_cv.notify_all();
        }
    }
};

} // namespace fr
