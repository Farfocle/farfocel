#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"
#include "fr/logger//logger.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"
#include "fr/physics/parts.hpp"

S32 main() {
    fr::init_core_ctx();
    auto pretty_sink = fr::make_unique<fr::PrettySink>();
    fr::get_ambient_ctx().logger->add_sink(std::move(pretty_sink));

    {
        fr::World world;

        for (USize i = 0; i < 100; ++i) {
            fr::Thing thing = world.spawn();
            world.emplace_now<fr::LocalTransformPart>(thing, fr::LocalTransformPart::identity());
            world.emplace_now<fr::WorldTransformPart>(thing, fr::WorldTransformPart::identity());
        }

        world.async_query<fr::LocalTransformPart, fr::WorldTransformPart>(
            [](auto chunk) {
                for (auto [thing, local_t, world_t] : chunk) {
                    FR_LOG("thing: {}; local: {}", thing, local_t);
                }
            },
            {.threads = 4});
    }

    fr::shutdown_core_ctx();
}
