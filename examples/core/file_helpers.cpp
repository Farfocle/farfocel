#include "fr/core/file.hpp"
#include "fr/core/string.hpp"
#include "fr/logger/logger.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"

S32 main() {
    fr::init_core_ctx();

    auto standard_sink = fr::make_unique<fr::PrettySink>(fr::PrettySink::Options{});
    fr::get_ambient_ctx().logger->add_sink(std::move(standard_sink));

    {
        fr::String path = "/home/jeffrey/Documents/shopping_list.txt";
        FR_LOG("Full path: {}", path);
        FR_LOG("Filename: {}", fr::file::get_filename(path));
        FR_LOG("Extension: {}", fr::file::get_extension(path));
        FR_LOG("Parent path: {}", fr::file::get_parent_path(path));
        FR_LOG("Stem: {}", fr::file::get_stem(path));

        auto sz = fr::file::get_file_size(path);
        if(sz) {
            FR_LOG("Size: {}", sz.unwrap());
        } else {
            FR_LOG_ERR("Cannot get file size");
        }

        auto all_bytes = fr::file::read_all_bytes(path);
        if (all_bytes) {
            auto bytes_array = all_bytes.unwrap(); // file bytes
            // log all bytes:
            FR_LOG("Reading {} bytes:", bytes_array.size());
            for (USize i = 0; i < bytes_array.size(); ++i) {
                Byte b = bytes_array[i];
                FR_LOG("{}", static_cast<U8>(b));
            }
        } else {
            FR_LOG_ERR("Cannot read file");
        }

        auto text = fr::file::read_all_text(path);
        if(text) {
            FR_LOG("Contents: {}", text.unwrap());
        } else {
            FR_LOG_ERR("Error reading file contents");
        }

    }

    fr::shutdown_core_ctx();
    return 0;
}
