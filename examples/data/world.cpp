/**
 * @file registry.cpp
 * @author Kiju
 *
 * @brief Example testing the basic capabilities of ECS.
 */

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
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
    fr::String path{"default"};

    template <typename A>
    void shape(A &a) {
        a.prop("width", width);
        a.prop("height", height);
        a.prop("path", path);
    }
};

void system_a(fr::World &world) {
    std::cout << "---- system_a: \n";
    for (auto [thing, pos] : world.query<Pos>()) {
        std::cout << fr::format("thing: {}; pos: {}", thing, pos) << "\n";
    }
}

void system_b(fr::World &world) {
    std::cout << "---- system_b: \n";
    for (auto [thing, sprite] : world.query<Sprite>()) {
        std::cout << fr::format("thing: {}; sprite: {}", thing, sprite) << "\n";
    }
}

S32 main() {
    fr::init_core_ctx();

    {
        fr::World world;

        world.schedule_sync(fr::Stage::PreUpdate, system_a);
        world.schedule_sync(fr::Stage::PostUpdate, system_b);

        fr::Thing a = world.handout();
        world.emplace<Pos>(a, Pos{1.0, 2.0});
        world.emplace<Sprite>(a, Sprite{1, 2, "a.png"});

        fr::Thing b = world.handout();
        world.emplace<Pos>(b, Pos{3.0, 4.0});
        world.emplace<Sprite>(b, Sprite{3, 4, "b.png"});

        fr::Thing c = world.handout();
        world.emplace<Pos>(c, Pos{5.0, 6.0});

        world.run_stage_sync(fr::Stage::PreUpdate);
        world.run_stage_sync(fr::Stage::PostUpdate);
    }

    fr::shutdown_core_ctx();
    return 0;
}
