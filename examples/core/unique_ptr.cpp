#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/unique_ptr.hpp"

struct Entity {
    S32 id;
    Entity(S32 i)
        : id(i) {
        std::cout << fr::format("Entity {} Created\n", id);
    }
    ~Entity() {
        std::cout << fr::format("Entity {} Destroyed\n", id);
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        auto e1 = fr::make_unique<Entity>(1);
        std::cout << fr::format("Active: {}\n", e1->id);

        auto e2 = std::move(e1);
        if (!e1) {
            std::cout << "e1 is now empty\n";
        }

        auto arr = fr::make_unique<S32[]>(5);
        arr[0] = 10;
        std::cout << fr::format("Array[0]: {}\n", arr[0]);
    }

    fr::shutdown_core_ctx();
    return 0;
}
