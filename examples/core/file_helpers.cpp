#include "fr/core/file_helpers.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/sinks/standard_sink.hpp"

S32 main() {
    fr::init_core_ctx();

    auto standard_sink = fr::make_unique<fr::StandardSink>(fr::StandardSink::Options{});
    fr::get_ambient_ctx().logger->add_sink(std::move(standard_sink));

    {
        fr::String path = "/home/jeffrey/Documents/shopping_list.txt";
        FR_LOG("Full path: {}", path);
        FR_LOG("Filename: {}", fr::file_helpers::get_filename(path));
        FR_LOG("Extension: {}", fr::file_helpers::get_extension(path));
        FR_LOG("Parent path: {}", fr::file_helpers::get_parent_path(path));
        FR_LOG("Stem: {}", fr::file_helpers::get_stem(path));
    }

    fr::shutdown_core_ctx();
    return 0;
}
