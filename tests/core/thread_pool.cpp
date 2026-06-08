#include <atomic>
#include <chrono>

#include <doctest.h>

#include "fr/core/thread_pool.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

TEST_CASE("ThreadPool - construction") {
    ThreadPool pool(2);
    CHECK(pool.thread_count() == 2);
    CHECK(pool.running_count() == 0);
}

TEST_CASE("ThreadPool - submit and wait") {
    ThreadPool pool(2);
    std::atomic<S32> counter{0};

    for (S32 i = 0; i < 10; ++i) {
        pool.submit([&counter] { ++counter; });
    }

    pool.wait();
    CHECK(counter.load() == 10);
}

TEST_CASE("ThreadPool - tasks run concurrently") {
    ThreadPool pool(4);
    std::atomic<S32> peak{0};
    std::atomic<S32> active{0};

    for (S32 i = 0; i < 8; ++i) {
        pool.submit([&peak, &active] {
            S32 current = ++active;
            S32 expected = peak.load();
            while (expected < current && !peak.compare_exchange_weak(expected, current)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            --active;
        });
    }

    pool.wait();
    CHECK(peak.load() > 1);
}

TEST_CASE("ThreadPool - stop is idempotent") {
    ThreadPool pool(2);
    pool.stop();
    pool.stop();
}

TEST_CASE("ThreadPool - stop after submit") {
    ThreadPool pool(2);
    std::atomic<S32> counter{0};

    for (S32 i = 0; i < 4; ++i) {
        pool.submit([&counter] { ++counter; });
    }

    pool.wait();
    pool.stop();
    CHECK(counter.load() == 4);
}

} // namespace fr
