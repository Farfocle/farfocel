/**
 * @file file_helpers.cpp
 * @author Stachu
 * @brief Example showcasing fr::file usage.
 */

#include "fr/core/ctx.hpp"
#include "fr/core/file.hpp"
#include "fr/core/slice.hpp"
#include "fr/core/string.hpp"
#include "fr/core/typedefs.hpp"
#include "fr/core/unique_ptr.hpp"
#include "fr/logger/logger.hpp"
#include "fr/logger/sinks/pretty_sink.hpp"

S32 main() {
    fr::init_core_ctx();

    auto standard_sink = fr::make_unique<fr::PrettySink>(fr::PrettySink::Options{});
    fr::get_ambient_ctx().logger->add_sink(std::move(standard_sink));

    {
        // example path with mixed forward and backward slashes that will be normalized
        fr::String readPath = "\\home\\staszek/Documents/inwokacja.txt";
        fr::String writePath = "/home/staszek/Documents/shopping_list.txt";
        fr::file::normalize(readPath);
        fr::file::normalize(writePath);

        FR_LOG("Full path: {}", readPath);

        FR_LOG("Unix-normalized path: {}", fr::file::get_normalized_unix(readPath));
        FR_LOG("Windows-normalized path: {}", fr::file::get_normalized_windows(readPath));
        FR_LOG("Path for this platform: {}", fr::file::get_normalized(readPath));

        if(fr::file::exists(readPath)) {
            FR_LOG_OK("File exists!");
        } else {
            FR_LOG_WARN("File does not exist!");
        }

        FR_LOG("Filename: {}", fr::file::get_filename(readPath));
        FR_LOG("Extension: {}", fr::file::get_extension(readPath));
        FR_LOG("Parent path: {}", fr::file::get_parent_path(readPath));
        FR_LOG("Stem: {}", fr::file::get_stem(readPath));

        auto sz = fr::file::get_file_size(readPath);
        if (sz) {
            FR_LOG("Size: {}", sz.unwrap());
        } else {
            FR_LOG_ERR("Cannot get file size");
        }

        auto all_bytes = fr::file::read_all_bytes(readPath);
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

        auto text = fr::file::read_all_text(readPath);
        if (text) {
            FR_LOG("Contents: {}", text.unwrap());
        } else {
            FR_LOG_ERR("Error reading file contents");
        }

        // this example will copy read bytes to a new file to showcase write_all_bytes function

        if (all_bytes) {
            auto bytes_array = all_bytes.unwrap();
            fr::Slice<Byte> slice_to_save(bytes_array.data(), bytes_array.size());

            fr::file::normalize(writePath);
            auto write_result = fr::file::write_all_bytes(writePath, slice_to_save);

            if (write_result) {
                FR_LOG_OK("File saved to {}!", writePath);
            } else {
                FR_LOG_ERR("Error saving file.");
            }
        } else {
            FR_LOG_WARN("write_all_bytes example will not be ran becuse it requires read_all_bytes "
                        "to succeed")
        }
    }

    fr::shutdown_core_ctx();
    return 0;
}
