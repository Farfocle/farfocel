#include <iostream>

#include "fr/core/array.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::Array<S32, 4> empty;
        fr::Array<S32, 4> numbers = {2, 1, 3, 7};

        std::cout << fr::format("Empty: {}\n", empty.slice());
        std::cout << fr::format("Numbers: {}\n", numbers.slice());

        numbers[2] = 100;
        auto [a, b, c, d] = numbers;

        std::cout << fr::format("Unpacked: {}, {}, {}, {}\n", a, b, c, d);
    }

    fr::shutdown_core_ctx();
    return 0;
}
