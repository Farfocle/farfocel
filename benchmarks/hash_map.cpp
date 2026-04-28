/**
 * @file hash_map.cpp
 * @brief Comparison benchmark: fr::HashMap vs std::unordered_map
 */

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "fr/core/hash_map.hpp"
#include "fr/core/string.hpp"

S32 main() {
    ankerl::nanobench::Bench bench;
    bench.title("HashMap Comparison").unit("operation").relative(true);

    const USize num_elements = 10'000;
    std::vector<S32> keys(num_elements);
    std::vector<S32> values(num_elements);

    for (USize i = 0; i < num_elements; ++i) {
        keys[i] = static_cast<S32>(i);
        values[i] = static_cast<S32>(i * 10);
    }

    bench.run("fr::HashMap<S32, S32> - Insert", [&] {
        fr::HashMap<S32, S32> map;
        for (USize i = 0; i < num_elements; ++i) {
            map.insert(keys[i], values[i]);
        }

        ankerl::nanobench::doNotOptimizeAway(map);
    });

    bench.run("std::unordered_map<S32, S32> - Insert", [&] {
        std::unordered_map<S32, S32> map;
        for (USize i = 0; i < num_elements; ++i) {
            map.insert({keys[i], values[i]});
        }

        ankerl::nanobench::doNotOptimizeAway(map);
    });

    fr::HashMap<S32, S32> fr_map;
    std::unordered_map<S32, S32> std_map;

    for (USize i = 0; i < num_elements; ++i) {
        fr_map.insert(keys[i], values[i]);
        std_map.insert({keys[i], values[i]});
    }

    bench.run("fr::HashMap<S32, S32> - Find", [&] {
        USize sum = 0;
        for (auto key : keys) {
            if (auto v = fr_map.find(key)) {
                sum += *v.unwrap();
            }
        }

        ankerl::nanobench::doNotOptimizeAway(sum);
    });

    bench.run("std::unordered_map<S32, S32> - Find", [&] {
        USize sum = 0;
        for (auto key : keys) {
            auto it = std_map.find(key);
            if (it != std_map.end()) {
                sum += it->second;
            }
        }

        ankerl::nanobench::doNotOptimizeAway(sum);
    });

    const USize num_strings = 1'000;
    std::vector<fr::String> fr_keys;
    std::vector<std::string> std_keys;

    for (USize i = 0; i < num_strings; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "string_key_%zu", i);
        fr_keys.emplace_back(buf);
        std_keys.emplace_back(buf);
    }

    bench.run("fr::HashMap<fr::String, S32> - Insert", [&] {
        fr::HashMap<fr::String, S32> map;
        for (USize i = 0; i < num_strings; ++i) {
            map.insert(fr_keys[i], static_cast<S32>(i));
        }
        ankerl::nanobench::doNotOptimizeAway(map);
    });

    bench.run("std::unordered_map<std::string, S32> - Insert", [&] {
        std::unordered_map<std::string, S32> map;
        for (USize i = 0; i < num_strings; ++i) {
            map.insert({std_keys[i], static_cast<S32>(i)});
        }
        ankerl::nanobench::doNotOptimizeAway(map);
    });

    fr::HashMap<fr::String, S32> fr_map_str;
    std::unordered_map<std::string, S32> std_map_str;

    for (USize i = 0; i < num_strings; ++i) {
        fr_map_str.insert(fr_keys[i], static_cast<S32>(i));
        std_map_str.insert({std_keys[i], static_cast<S32>(i)});
    }

    bench.run("fr::HashMap<fr::String, S32> - Find", [&] {
        USize sum = 0;

        for (const auto &s : fr_keys) {
            if (auto v = fr_map_str.find(s)) {
                sum += *v.unwrap();
            }
        }

        ankerl::nanobench::doNotOptimizeAway(sum);
    });

    bench.run("std::unordered_map<std::string, S32> - Find", [&] {
        USize sum = 0;

        for (const auto &s : std_keys) {
            auto it = std_map_str.find(s);
            if (it != std_map_str.end()) {
                sum += it->second;
            }
        }

        ankerl::nanobench::doNotOptimizeAway(sum);
    });

    bench.run("fr::HashMap<fr::String, fr::String> - Insert", [&] {
        fr::HashMap<fr::String, fr::String> map;

        for (USize i = 0; i < num_strings; ++i) {
            map.insert(fr_keys[i], fr_keys[i]);
        }

        ankerl::nanobench::doNotOptimizeAway(map);
    });

    bench.run("std::unordered_map<std::string, std::string> - Insert", [&] {
        std::unordered_map<std::string, std::string> map;

        for (USize i = 0; i < num_strings; ++i) {
            map.insert({std_keys[i], std_keys[i]});
        }

        ankerl::nanobench::doNotOptimizeAway(map);
    });

    fr::HashMap<fr::String, fr::String> fr_map_str2;
    std::unordered_map<std::string, std::string> std_map_str2;

    for (USize i = 0; i < num_strings; ++i) {
        fr_map_str2.insert(fr_keys[i], fr_keys[i]);
        std_map_str2.insert({std_keys[i], std_keys[i]});
    }

    bench.run("fr::HashMap<fr::String, fr::String> - Find", [&] {
        USize total_len = 0;

        for (const auto &s : fr_keys) {
            if (auto v = fr_map_str2.find(s)) {
                total_len += (*v.unwrap()).size();
            }
        }

        ankerl::nanobench::doNotOptimizeAway(total_len);
    });

    bench.run("std::unordered_map<std::string, std::string> - Find", [&] {
        USize total_len = 0;

        for (const auto &s : std_keys) {
            auto it = std_map_str2.find(s);
            if (it != std_map_str2.end()) {
                total_len += it->second.size();
            }
        }

        ankerl::nanobench::doNotOptimizeAway(total_len);
    });

    return 0;
}
