/**
 * @file showcase.cpp
 * @author Stachu
 *
 * @brief Example showcasing Logger using PrettySink with Options.
 */

#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/logger.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"
#include <iostream>

void find_user_number(S32 target_number) {
    FR_LOG("Starting search loop for target number: {}", target_number);

    for (S32 i = 0; i <= 100; ++i) {
        FR_LOG("Iterating... current index: {}", i);

        if (i == 67) {
            FR_LOG_ERR("67!");
            return;
        }

        if (i == target_number) {
            FR_LOG_OK("Found your number: {}", i);
            return;
        }

        FR_LOG_WARN("{} is not your number", i);
    }
}

S32 main() {
    fr::init_core_ctx();

    {
        // 1. setup sink
        auto enhanced_sink = fr::make_unique<fr::PrettySink>(fr::PrettySink::Options{
            .timestampFormatOptions{.date = false, // no date = skiping calendar math (faster)
                                    .time = true,
                                    .milliseconds = false},
            .shorterLevelNames = true // e.g. "ERR" instead of "ERROR"
        });

        // 2. add sink to the logger
        fr::get_ambient_ctx().logger->add_sink(std::move(enhanced_sink));

        // et voila
        std::cout << "Enter a number between 0 and 100: ";
        S32 user_input = 0;
        std::cin >> user_input;

        if (user_input < 0 || user_input > 100) {
            FR_LOG_CRIT("Input {} is out of bounds! Must be between 0 and 100.", user_input);
        } else {
            find_user_number(user_input);
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
