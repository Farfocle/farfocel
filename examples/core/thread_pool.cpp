#include <atomic>

#include "fr/core/ctx.hpp"
#include "fr/core/thread_pool.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/logger/logger.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"

S32 main() {
    fr::init_core_ctx();

    auto enhanced_sink = fr::make_unique<fr::PrettySink>(fr::PrettySink::Options{});
    fr::get_ambient_ctx().logger->add_sink(std::move(enhanced_sink));

    {
        FR_LOG("Basic task submission");
        fr::ThreadPool pool(4);

        std::atomic<S32> counter{0};
        for (S32 i = 0; i < 8; ++i) {
            pool.submit([&counter, i] {
                ++counter;
                FR_LOG("  task {} done", i);
            });
        }

        pool.wait();
        FR_LOG("  completed: {}/8 tasks", counter.load());

        FR_LOG("Parallel sum");

        constexpr S32 N = 100;
        std::atomic<S32> sum{0};

        for (S32 i = 1; i <= N; ++i) {
            pool.submit([&sum, i] { sum.fetch_add(i, std::memory_order_relaxed); });
        }

        pool.wait();
        FR_LOG("  sum 1..{} = {} (expected {})", N, sum.load(), N * (N + 1) / 2);

        FR_LOG("Pool info");
        FR_LOG("  thread_count: {}", pool.thread_count());
        FR_LOG("  running_count (after wait): {}", pool.running_count());
    }

    fr::shutdown_core_ctx();
    return 0;
}
