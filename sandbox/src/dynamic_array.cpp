#include <print>

#include "fr/core/allocator.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/tuple.hpp"
#include "fr/core/typedefs.hpp"

int main() {
    {
        fr::Allocator *heap_alloc = fr::globals::get_default_allocator();

        auto numbers = fr::DynamicArray<S32>::with_allocator(heap_alloc);

        numbers.push_back(0);
        numbers.push_back(1);
        numbers.push_back(2);
        numbers.push_back(3);
        numbers.push_back(4);
        numbers.push_back(5);
        numbers.push_back(6);

        auto part = numbers.slice();

        for (S32 n : part) {
            std::println("part -> {}", n);
        }

        std::println("----");
        auto pair = fr::Tuple(42, 0.42f);
        auto [a, b] = pair;

        std::println("pair.0 -> {}", a);
        std::println("pair.1 -> {}", b);
    }

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
