#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/logger/logger.hpp"

S32 main()
{
    fr::init_core_ctx();

    fr::get_ambient_ctx().logger->log("Hello world!");

    fr::shutdown_core_ctx();
    return 0;
}
