#define DOCTEST_CONFIG_IMPLEMENT

#include <doctest.h>

#include "fr/core/ctx.hpp"

int main(int argc, char **argv) {
    fr::init_core_ctx();

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();

    fr::shutdown_core_ctx();
    return res;
}
