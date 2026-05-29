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
    fr::get_ambient_ctx().logger->log(fr::LogLevel::Info, "Hello {}! ", "world");
    fr::get_ambient_ctx().logger->log(fr::LogLevel::Success, "It works!");
    fr::get_ambient_ctx().logger->log(fr::LogLevel::Critical, 42);
    fr::get_ambient_ctx().logger->log(fr::LogLevel::Error, true);
    fr::get_ambient_ctx().logger->log(fr::LogLevel::Info, "{} + {} = {}", 2, 2, 2+2);

    fr::shutdown_core_ctx();
    return 0;
}
