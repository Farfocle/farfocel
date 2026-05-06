#include "fr/core/dynamic_array.hpp"
#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::DynamicArray<S32> numbers{1, 2, 3, 4};

        auto slice = numbers.slice(1, 3);

        for (S32 n : slice) {
            std::cout << n << "\n";
        }
    }

    fr::shutdown_core_ctx();

    return 0;
}
