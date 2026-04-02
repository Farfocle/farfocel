#include "fr/core/dynamic_array.hpp"
#include "fr/core/allocator.hpp"
#include "fr/core/globals.hpp"
#include "fr/core/typedefs.hpp"
#include <print>

int main()
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

    for (S32 n : part)
    {
        std::println("part -> {}", n);
    }

    return 0;
}
