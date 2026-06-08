#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/string.hpp"
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

struct PlayerScript : public fr::Script {
    PlayerScript() = default;
    PlayerScript(S32 hp_arg)
        : hp(hp_arg) {
    }

    S32 hp{10};

    void on_init() {
        insert(Pos{42.0, 67.0});
        insert(Sprite{6, 9, "player.png"});
    }

    void on_update() {
        Pos &pos = get<Pos>();
        Sprite &sprite = get<Sprite>();

        std::cout << "\n---- PlayerScript\n";
        std::cout << fr::format("self: {}\npos: {}\nsprite: {}\nhp: {}", self, pos, sprite, hp)
                  << "\n";

        pos.x += 1.0;
        pos.y += 2.0;
        hp -= 1;
    }
};

struct Game {
    fr::World world;
    fr::Thing player;

    void init() {
        player = world.spawn();
        world.insert_script(player, PlayerScript(10));
        world.commit();
    }

    void run_future() {
        world.run();
        world.commit();
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        Game game;
        game.init();
        game.run_future();
        game.run_future();
        game.run_future();
    }

    fr::shutdown_core_ctx();
    return 0;
}
