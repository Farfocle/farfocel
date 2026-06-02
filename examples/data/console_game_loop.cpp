#include <chrono>
#include <iostream>
#include <thread>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/format.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"

struct Transform {
    F32 x{0.0};
    F32 y{0.0};

    FR_SHAPE({
        FR_PROP(x);
        FR_PROP(y);
    })
};

struct Health {
    S32 max_health{100};
    S32 health{10};

    FR_SHAPE({
        FR_PROP(max_health);
        FR_PROP(health);
    })
};

struct Player {
    fr::String name{"player"};
    fr::DynamicArray<fr::Thing> minions{};

    FR_SHAPE({
        FR_PROP(name);
        FR_PROP(minions);
    })
};

struct Minion {
    fr::String name{"unnamed minion"};
    U32 level{0};

    FR_SHAPE({
        FR_PROP(name);
        FR_PROP(level);
    })
};

void player_update(fr::Scope scope) {
    for (auto [thing, player, health, transform] : scope.query<Player, Health, Transform>()) {
        fr::Thing minion = scope.handout();
        scope.insert(minion, Transform{});
        scope.insert(minion, Health{.max_health = 10, .health = 10});
        scope.insert(minion, Minion{.name = "Karol Szypula", .level = 1});

        player.minions.push_back(minion);

        health.health -= 20;
        transform.x += 1.0;
        transform.y += 1.0;
    }
}

void minion_update(fr::Scope scope) {
    for (auto [thing, minion, health] : scope.query<Minion, Health>()) {
        health.health -= 3;
    }
}

void clear_dead(fr::Scope scope) {
    for (auto [thing, health] : scope.query<Health>()) {
        if (health.health <= 0) {
            scope.kill(thing);
        }
    }
}

void print_player(fr::Scope scope) {
    for (auto [thing, transform, health, player] : scope.query<Transform, Health, Player>()) {
        std::cout << fr::format("[PLAYER]\nthing: {}\nplayer: {}\ntransform: {}\nhealth: {}\n",
                                thing, player, transform, health);
    }
}

void print_minions(fr::Scope scope) {
    for (auto [thing, transform, health, minion] : scope.query<Transform, Health, Minion>()) {
        std::cout << fr::format("[MINION]\nthing: {}\nminion: {}\ntransform: {}\nhealth: {}\n",
                                thing, minion, transform, health);
    }
}

struct Game {
    fr::World world;
    fr::Thing player;

    void init() {
        do_init_things();
        do_init_systems();
    }

    void run_loop(USize iterations) {
        using namespace std::chrono_literals;

        for (USize i = 0; i < iterations; ++i) {
            std::cout << "\n======================= Iteration " << i << "\n\n";
            run_frame();
            std::this_thread::sleep_for(2000ms);
        }
    }

    void run_frame() {
        world.run_stage_sync(fr::Stage::PreUpdate);
        world.run_stage_sync(fr::Stage::Update);
        world.run_stage_sync(fr::Stage::PostUpdate);
        world.commit_cmds();
    }

private:
    void do_init_things() {
        player = world.handout();
        world.insert(player, Player{});
        world.insert(player, Health{.max_health = 200, .health = 200});
        world.insert(player, Transform{});
        world.commit_cmds();
    }

    void do_init_systems() {
        world.schedule_sync(fr::Stage::Update, player_update);
        world.schedule_sync(fr::Stage::Update, minion_update);
        world.schedule_sync(fr::Stage::Update, clear_dead);
        world.schedule_sync(fr::Stage::PostUpdate, print_minions);
        world.schedule_sync(fr::Stage::PostUpdate, print_player);
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        Game game;
        game.init();
        game.run_loop(7);
    }

    fr::shutdown_core_ctx();
    return 0;
};
