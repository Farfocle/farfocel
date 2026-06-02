#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/relations.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include <iostream>

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
        fr::Thing player = world.handout();
        world.emplace_now<fr::Relations>(player);

        for (USize i = 0; i < 3; ++i) {
            fr::Thing child = world.handout();
            world.emplace_now<fr::Relations>(child);
            world.emplace_now<Pos>(child);
            world.attach_child_now(player, child);
        }

        for (auto [thing, rel] : world.shallow_query<fr::Relations>(player)) {
            std::cout << fr::format("thing: {}; rel: {}\n", thing, rel);
        }
    }

    fr::shutdown_core_ctx();
}
