#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/format.hpp"
#include "fr/core/slice.hpp"

void print_slice(fr::Slice<const S32> s) {
    std::cout << fr::format("Viewing: {}\n", s);
}

S32 main() {
    fr::init_core_ctx();

    {
        fr::DynamicArray<S32> data = {4, 2, 0, 6, 9};

        fr::Slice s = data.slice(1, 3);
        print_slice(s);

        auto mut_s = data.slice_mut();
        mut_s[0] = 100;

        std::cout << fr::format("Modified: {}\n", data.slice());
    }

    fr::shutdown_core_ctx();
    return 0;
}
