#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"

struct GameState {
    F32 dt{0.1f};
    USize frame{0};

    FR_SHAPE({
        FR_PROP(dt);
        FR_PROP(frame);
    })
};

void game_state_system(fr::Scope scope) {
    GameState &state = scope.get_resource<GameState>();

    std::cout << fr::format("state: {}\n", state);

    state.dt = 0.1f;
    state.frame += 1;
}

S32 main() {
    fr::init_core_ctx();

    {
        fr::World world;
        world.emplace_resource<GameState>();

        world.schedule(fr::Stage::Update, game_state_system);

        for (USize i = 0; i < 10; ++i) {
            world.run_stage(fr::Stage::Update);
        }
    }

    fr::shutdown_core_ctx();
}
