/**
 * @file registry.cpp
 * @author Kiju
 *
 * @brief Example showcasing the basic functionality of the hidden ECS layer.
 */

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/registry.hpp"
#include "fr/data/thing.hpp"
#include <iostream>

struct Pos {
    F32 x{0.0};
    F32 y{0.0};

    template <typename A>
    void shape(A &a) {
        a.prop("x", x);
        a.prop("y", y);
    }
};

struct Sprite {
    U32 width{0};
    U32 height{0};
    fr::String path{};

    template <typename A>
    void shape(A &a) {
        a.prop("width", width);
        a.prop("height", height);
        a.prop("path", path);
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        fr::impl::Registry registry;

        fr::Thing a = registry.handout();
        registry.emplace<Pos>(a, Pos{42.0, 67.0});
        registry.emplace<Sprite>(a, Sprite{42, 42, "banana.png"});

        fr::Thing b = registry.handout();
        registry.emplace<Pos>(b, Pos{42.0, 69.0});
        registry.emplace<Sprite>(b, Sprite{42, 42, "apple.png"});

        fr::Thing c = registry.handout();
        registry.emplace<Pos>(c, Pos{13.0, 12.0});

        std::cout << "---- query<Pos, Sprite>()\n";
        for (auto [thing, pos, spirte] : registry.query<Pos, Sprite>()) {
            std::cout << fr::format("pos: {}; sprite: {}", pos, spirte) << "\n";
        }

        std::cout << "---- query<Pos>().without<Sprite>()\n";
        for (auto [thing, pos] : registry.query<Pos>().without<Sprite>()) {
            std::cout << fr::format("pos: {}", pos) << "\n";
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
