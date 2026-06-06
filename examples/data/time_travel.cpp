#include <iostream>
#include <thread>

#include "fr/core/ctx.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/cmd.hpp"
#include "fr/data/world.hpp"

struct Pos {
    U32 x{0};
    U32 y{0};

    FR_SHAPE({
        FR_PROP(x);
        FR_PROP(y);
    })
};

struct Cube {
    fr::String name;

    FR_SHAPE({ FR_PROP(name); })
};

struct State {
    fr::CmdBatchTimeline timeline;
    bool move_cubes{true};
};

void move_cubes_system(fr::Scope scope) {
    State &state = scope.get_resource<State>();
    fr::CmdBatch batch(1024);

    if (!state.move_cubes) {
        return;
    }

    for (auto [thing, pos] : scope.query<Pos>()) {

        Pos prev = pos;
        pos.x += 1;
        pos.y += 1;

        batch.record_mutate(thing, prev, pos);
    }

    state.timeline.push(std::move(batch), 1);
}

void draw_cubes_system(fr::Scope scope) {
    std::cout << "----------------------------------------------\n";
    for (auto [thing, pos] : scope.query<Pos>()) {
        for (USize i = 0; i < 20; ++i) {
            for (USize j = 0; j < 20; ++j) {
                if (i == pos.x % 20 && j == pos.y % 20) {
                    std::cout << "X ";
                } else {
                    std::cout << ". ";
                }
            }

            std::cout << "\n";
        }
    }
}

struct Game {
    fr::World world;
    fr::Thing cube;

    void init() {
        world.schedule(fr::Stage::PreUpdate, move_cubes_system);
        world.schedule(fr::Stage::PostUpdate, draw_cubes_system);

        world.emplace_resource<State>();

        cube = world.handout();
        world.insert_now(cube, Pos{0, 0});
        world.insert_now(cube, Cube{"main"});
    }

    void run() {
        world.run();
        world.commit_future();
    }

    void undo() {
        State &state = world.get_resource<State>();
        world.commit_past_from(state.timeline.batch());
        state.timeline.go_past();
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        Game game;
        game.init();

        // Normal run loop.
        for (USize i = 0; i < 10; ++i) {
            using namespace std::chrono_literals;
            game.run();
            std::this_thread::sleep_for(1000ms);
        }

        game.world.get_resource<State>().move_cubes = false;

        // Undo loop.
        for (USize i = 0; i < 10; ++i) {
            using namespace std::chrono_literals;
            game.undo();
            game.run();
            std::this_thread::sleep_for(1000ms);
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
