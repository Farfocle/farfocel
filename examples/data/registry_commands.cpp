/**
 * @file registry.cpp
 * @author Kiju
 *
 * @brief Example showcasing the basic functionality of the hidden ECS layer with commands.
 * @details Commands offer a lazy way to modify the registry state (insert and destroy parts).
 */

#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"

struct Pos {
    F32 x{0.0};
    F32 y{0.0};

    FR_SHAPE({
        FR_PROP(x);
        FR_PROP(y);
    })
};

S32 main() {
    fr::init_core_ctx();

    {
        fr::World world;

        fr::Thing a = world.handout();
        world.insert<Pos>(a, Pos{42.0, 67.0});

        world.commit_insert_part_cmds();

        fr::Thing b = world.handout();
        world.insert<Pos>(b, Pos{42.0, 69.0});

        fr::Thing c = world.handout();
        world.insert<Pos>(c, Pos{13.0, 12.0});

        std::cout << "--- commit 0\n";
        for (auto [thing, pos] : world.query<Pos>()) {
            std::cout << fr::format("thing: {}; pos: {}", thing, pos) << "\n";
        }

        world.commit_insert_part_cmds();

        std::cout << "--- commit 1\n";
        for (auto [thing, pos] : world.query<Pos>()) {
            std::cout << fr::format("thing: {}; pos: {}", thing, pos) << "\n";
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
