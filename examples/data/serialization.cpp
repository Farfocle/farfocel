#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/json.hpp"
#include "fr/core/meta.hpp"
#include "fr/core/shape.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"

struct Pos {
    F32 x{0.0f};
    F32 y{0.0f};

    FR_SHAPE({
        FR_PROP(x);
        FR_PROP(y);
    })
};

FR_TYPE(Pos);

struct Health {
    F32 current{100.0f};
    F32 max{100.0f};

    FR_SHAPE({
        FR_PROP(current);
        FR_PROP(max);
    })
};

FR_TYPE(Health);

struct GameConfig {
    U32 tick_rate{60};
    F32 gravity{9.81f};

    FR_SHAPE({
        FR_PROP(tick_rate);
        FR_PROP(gravity);
    })
};

FR_TYPE(GameConfig);

fr::String serialize(fr::World &world) {
    fr::JsonWriterArchive writer({.pretty = true});
    call_shape(writer, world);
    return writer.consume();
}

void deserialize(fr::World &world, fr::StringView json) {
    fr::JsonReaderArchive reader(json);
    call_shape(reader, world);
}

S32 main() {
    fr::init_core_ctx();

    {
        fr::World world;
        world.emplace_resource<GameConfig>(GameConfig{.tick_rate = 30, .gravity = 20.0f});

        fr::Thing a = world.spawn();
        world.emplace_now<Pos>(a, Pos{.x = 1.0f, .y = 2.0f});
        world.emplace_now<Health>(a, Health{.current = 80.0f, .max = 100.0f});

        fr::Thing b = world.spawn();
        world.emplace_now<Pos>(b, Pos{.x = 5.0f, .y = 3.0f});

        fr::String json = serialize(world);
        std::cout << "---- serialized\n" << json.c_str() << "\n";

        fr::World world2;

        world2.ensure<Pos>();
        world2.ensure<Health>();
        world2.ensure<GameConfig>();

        deserialize(world2, "{}");

        fr::String json2 = serialize(world2);
        std::cout << "---- deserialized\n" << json2.c_str() << "\n";
    }

    fr::shutdown_core_ctx();
}
