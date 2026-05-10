#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/pair.hpp"
#include "fr/core/string.hpp"
#include "fr/core/tuple.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::Pair<S32, fr::String> p(42, "Answer");

        auto [num, str] = p;
        std::cout << fr::format("Pair: {} -> {}\n", num, str);

        fr::Tuple t(1, 2.5f, "#jakub");

        t.each([](auto &val) { std::cout << fr::format("Tuple item: {}\n", val); });

        auto [i, f, s] = t;
        std::cout << fr::format("Unpacked Tuple: {}, {}, {}\n", i, f, s);
    }

    fr::shutdown_core_ctx();
    return 0;
}
