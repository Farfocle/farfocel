#include <print>

#include "fr/core/alloc.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/tuple.hpp"
#include "fr/core/typedefs.hpp"

int main() {
    {
        auto numbers = fr::DynamicArray<U32>::filled_with(10, 4);

        numbers.push_back(67);

        auto part = numbers.slice(0, 3);

        for (U32 n : part) {
            std::println("{}", n);
        }
    }

    std::println("----");
    auto t = fr::Tuple(42, 0.42f, true);
    auto [a, b, c] = t;

    std::println("size -> {}", t.size());
    std::println("a -> {}", a);
    std::println("b -> {}", b);
    std::println("c -> {}", c);

    t.each([](const auto &item) { std::println("each -> {}", item); });

    t.map([](const auto &item) { return sizeof(item); }).each([](const auto &item) {
        std::println("each -> sizeof -> {}", item);
    });

    std::println("----");
    for (const auto &frame : fr::globals::get_allocation_stack()->frames()) {
        std::println("frame ts={} action={} prev={:p} next={:p} prev_size={} next_size={} align={} "
                     "tag={} success={} attempt={}",
                     frame.timestamp, static_cast<U32>(frame.action), frame.prev_pointer,
                     frame.next_pointer, frame.prev_size, frame.next_size, frame.alignment,
                     frame.tag, frame.success, frame.attempt);
    }

    return 0;
}
