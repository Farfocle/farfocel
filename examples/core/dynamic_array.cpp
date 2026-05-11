#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::DynamicArray<S32> numbers{2, 1, 3, 7};
        auto strings = fr::DynamicArray<fr::String>::from_repeated(4, "a");

        auto slice = numbers.slice(1, 3);

        for (S32 n : slice) {
            std::cout << n << "\n";
        }

        std::cout << "---\n";

        for (const fr::String &s : strings) {
            std::cout << s << "\n";
        }
    }

    fr::shutdown_core_ctx();

    return 0;
}
