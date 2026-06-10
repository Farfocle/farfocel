#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/parts.hpp"
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

struct Sprite {
    U32 width{0};
    U32 height{0};
    fr::String path{};

    FR_SHAPE({
        FR_PROP(width);
        FR_PROP(height);
        FR_PROP(path);
    })
};

struct Game {
    fr::World world;
    fr::Thing player;

    void init() {
        player = world.spawn();
        world.emplace_now<fr::RelationsPart>(player);

        for (USize i = 0; i < 3; ++i) {
            fr::Thing child = world.spawn();
            world.emplace_now<fr::RelationsPart>(child);
            world.emplace_now<Pos>(child);

            for (USize j = 0; j < 3; ++j) {
                fr::Thing more_so_a_child = world.spawn();
                world.emplace_now<fr::RelationsPart>(more_so_a_child);
                world.emplace_now<Pos>(more_so_a_child);
                world.attach_child_now(child, more_so_a_child);
            }

            world.attach_child_now(player, child);
        }
    }

    void run_future() {
        world.sort_by_hierarchy_depth<Pos>();

        std::cout << "---- deep_query\n";
        for (auto [thing, relations, pos] : world.deep_query<fr::RelationsPart, Pos>(player)) {
            std::cout << "\n----\n";
            std::cout << fr::format("thing: {}\nrelations: {}\npos: {}\n", thing, relations, pos);
        }

        std::cout << "\n---- query<Pos>\n";
        for (auto [thing, pos] : world.query<Pos>()) {
            std::cout << fr::format("thing: {}; pos: {}\n", thing, pos);
        }
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        Game game;
        game.init();
        game.run_future();
    }

    fr::shutdown_core_ctx();
}
