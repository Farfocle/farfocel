#include <iostream>

#include "fr/core/ctx.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/json.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"

struct Pos {
    F32 x{0.0};
    F32 y{0.0};

    template <typename A>
    void shape(A &a) {
        a.prop("x", x);
        a.prop("y", y);
    }
};

struct World {
    fr::DynamicArray<Pos> positions{};

    template <typename A>
    void shape(A &a) {
        a.prop("positions", positions);
    }
};

S32 main() {
    fr::init_core_ctx();

    {
        World world{.positions = {{2.0, 1.0}, {3.0, 7.0}}};

        fr::JsonWriterArchive serializer({.types = false, .pretty = true});

        world.shape(serializer);

        fr::String serialized_json = serializer.consume();
        std::cout << serialized_json << "\n";
        std::cout << "---\n";

        fr::JsonReaderArchive deserializer(serialized_json);
        World deserialized_world;
        deserialized_world.shape(deserializer);

        std::cout << (world.positions[0].x == deserialized_world.positions[0].x) << "\n";
    }

    fr::shutdown_core_ctx();
    return 0;
}
