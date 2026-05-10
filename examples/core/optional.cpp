#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/optional.hpp"

fr::Optional<F32> divide(F32 a, F32 b) {
    if (b == 0.0f) {
        return fr::none();
    }

    return fr::some(a / b);
}

S32 main() {
    fr::init_core_ctx();

    {
        auto result = divide(10.0f, 2.0f);
        auto failed = divide(10.0f, 0.0f);

        if (result) {
            std::cout << fr::format("Result: {}\n", result.unwrap());
        }

        F32 val = failed.unwrap_or(-1.0f);
        std::cout << fr::format("Fallback: {}\n", val);

        auto squared = result.map([](F32 x) { return x * x; });
        if (squared) {
            std::cout << fr::format("Mapped: {}\n", squared.unwrap());
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
