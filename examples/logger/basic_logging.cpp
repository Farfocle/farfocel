/**
 * @file basic_logging.cpp
 * @author Stachu
 *
 * @brief Example showcasing the basic logging using StandardSink.
 */

#include "fr/logger/logger.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/sinks/standard_sink.hpp"

S32 main() {
    fr::init_core_ctx();

    // setup the enhanced sink and add it to the logger
    auto enhanced_sink = fr::make_unique<fr::StandardSink>(fr::StandardSink::Options{});
    fr::get_ambient_ctx().logger->add_sink(std::move(enhanced_sink));

    // use logger
    FR_LOG("Hello {}! ", "world");
    FR_LOG_OK("It works!");
    FR_LOG_CRIT(42);
    FR_LOG_ERR(true);
    FR_LOG_WARN("This is a warning.");
    FR_LOG("{} + {} = {}", 2, 2, 2 + 2);

    fr::shutdown_core_ctx();
    return 0;
}
