#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/sinks/standard_sink.hpp"
#include "fr/logger/logger.hpp"

S32 main()
{
    fr::init_core_ctx();

    // setup a sink and add it to the logger
    auto standard_sink = fr::make_unique<fr::StandardSink>();
    fr ::get_ambient_ctx().logger->add_sink(std::move(standard_sink));

    // use logger
    fr::get_ambient_ctx().logger->log("Hello world!");
    fr::get_ambient_ctx().logger->log("It works!");

    fr::shutdown_core_ctx();

    return 0;
}
