#include <iostream>

#include "fr/core/algo.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/format.hpp"
#include "fr/core/typedefs.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::DynamicArray<U32> numbers{9, 8, 7, 6, 5, 4, 3, 3, 2, 2, 1, 0, 0, 0, 14, 0, 56};

        fr::radix_sort_inplace(numbers.slice_mut());

        std::cout << "---- numbers\n";
        std::cout << fr::format("numbers:  {}", numbers) << "\n";
    }

    fr::shutdown_core_ctx();
    return 0;
}
