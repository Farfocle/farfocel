#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"

struct Pos {
    F32 x{0};
    F32 y{0};
};

S32 main() {
    fr::init_core_ctx();

    {
        fr::World world;

        fr::Thing player = world.spawn();

        world.insert(player, Pos{});

        world.commit();
    }

    fr::shutdown_core_ctx();
}
