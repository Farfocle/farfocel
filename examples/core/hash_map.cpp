#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/string.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::HashMap<S32, fr::String> items;

        items.insert(4, "Sta");
        items.insert(2, "nislaw");
        items[0] = fr::String("Dera");

        if (auto fruit = items.find(2)) {
            // items.find(2) returns Optional<Value*>, so fruit.unwrap() is Value*
            std::cout << fr::format("Found: {}\n", *fruit.unwrap());
        }

        for (auto pair : items) {
            auto id = pair.first();
            auto name = pair.second();
            std::cout << fr::format("ID: {}, Name: {}\n", id, name);
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
