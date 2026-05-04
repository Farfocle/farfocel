#include "fr/core/dynamic_array.hpp"
#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/json.hpp"
#include "fr/core/typedefs.hpp"

struct Pos {
    F32 x{0.0};
    F32 y{0.0};
    fr::DynamicArray<F32> numbers{};

    template <typename A>
    void shape(A &a) {
        a.prop("x", x);
        a.prop("y", y);
        a.prop("numbers", numbers);
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        Pos p{4.2, 6.7, {1.0, 2.0, 3.0}};
        fr::JsonSerializer writer({.types = true, .pretty = true});

        p.shape(writer);

        std::cout << writer.consume().c_str() << "\n";
    }

    fr::shutdown_core_ctx();
    return 0;
}
