#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/typedefs.hpp"

struct Pos {
    F32 x{0.0};
    F32 y{0.0};

    template <typename A>
    void shape(A &a) {
        a.prop("x", x);
        a.prop("y", y);
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        Pos a{42.0, 67.69};

        std::cout << fr::format("Number: {}; Position: {}", 2, a);
    }

    fr::shutdown_core_ctx();
    return 0;
}
