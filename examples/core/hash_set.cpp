#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/hash_set.hpp"
#include "fr/core/string.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::HashSet<fr::String> tags;

        tags.insert(fr::String("//"));
        tags.insert(fr::String("Karol"));
        tags.insert(fr::String("Szypula"));

        if (tags.contains("Szypula")) {
            std::cout << "Contains Szypula\n";
        }

        for (const auto &tag : tags) {
            std::cout << fr::format("Tag: {}\n", tag);
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
