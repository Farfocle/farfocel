#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <unordered_set>
#include <vector>

#include "fr/core/hash_set.hpp"
#include "fr/core/string.hpp"

/**
 * @file hash_set_bench.cpp
 * @brief Comparison benchmark: fr::HashSet vs std::unordered_set
 */
S32 main() {
    ankerl::nanobench::Bench bench;
    bench.title("HashSet Comparison").unit("operation").relative(true);

    const USize num_elements = 10'000;
    std::vector<S32> data(num_elements);

    for (USize i = 0; i < num_elements; ++i) {
        data[i] = static_cast<S32>(i);
    }

    // Benchmark fr::HashSet Insertion
    bench.run("fr::HashSet - Insert", [&] {
        fr::HashSet<S32> set;
        for (auto val : data) {
            set.insert(val);
        }
        ankerl::nanobench::doNotOptimizeAway(set);
    });

    // Benchmark std::unordered_set Insertion
    bench.run("std::unordered_set - Insert", [&] {
        std::unordered_set<S32> set;
        for (auto val : data) {
            set.insert(val);
        }
        ankerl::nanobench::doNotOptimizeAway(set);
    });

    // Prepare sets for lookup benchmarks
    fr::HashSet<S32> fr_set;
    std::unordered_set<S32> std_set;
    for (auto val : data) {
        fr_set.insert(val);
        std_set.insert(val);
    }

    // Benchmark fr::HashSet Lookup
    bench.run("fr::HashSet - Contains", [&] {
        USize found = 0;
        for (auto val : data) {
            if (fr_set.contains(val)) {
                found++;
            }
        }
        ankerl::nanobench::doNotOptimizeAway(found);
    });

    // Benchmark std::unordered_set Lookup
    bench.run("std::unordered_set - Contains", [&] {
        USize found = 0;
        for (auto val : data) {
            if (std_set.find(val) != std_set.end()) {
                found++;
            }
        }
        ankerl::nanobench::doNotOptimizeAway(found);
    });

    // ---------------------------------------------------------
    // String Benchmarks
    // ---------------------------------------------------------

    const USize num_strings = 1'000;
    std::vector<fr::String> fr_strings;
    std::vector<std::string> std_strings;
    for (USize i = 0; i < num_strings; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "string_key_%zu", i);
        fr_strings.emplace_back(buf);
        std_strings.emplace_back(buf);
    }

    // Benchmark fr::HashSet<fr::String> Insertion
    bench.run("fr::HashSet<fr::String> - Insert", [&] {
        fr::HashSet<fr::String> set;
        for (const auto &s : fr_strings) {
            set.insert(s);
        }
        ankerl::nanobench::doNotOptimizeAway(set);
    });

    // Benchmark std::unordered_set<std::string> Insertion
    bench.run("std::unordered_set<std::string> - Insert", [&] {
        std::unordered_set<std::string> set;
        for (const auto &s : std_strings) {
            set.insert(s);
        }
        ankerl::nanobench::doNotOptimizeAway(set);
    });

    // Prepare sets for lookup
    fr::HashSet<fr::String> fr_set_str;
    std::unordered_set<std::string> std_set_str;
    for (USize i = 0; i < num_strings; ++i) {
        fr_set_str.insert(fr_strings[i]);
        std_set_str.insert(std_strings[i]);
    }

    // Benchmark fr::HashSet<fr::String> Lookup
    bench.run("fr::HashSet<fr::String> - Contains", [&] {
        USize found = 0;
        for (const auto &s : fr_strings) {
            if (fr_set_str.contains(s)) {
                found++;
            }
        }
        ankerl::nanobench::doNotOptimizeAway(found);
    });

    // Benchmark std::unordered_set<std::string> Lookup
    bench.run("std::unordered_set<std::string> - Contains", [&] {
        USize found = 0;
        for (const auto &s : std_strings) {
            if (std_set_str.find(s) != std_set_str.end()) {
                found++;
            }
        }
        ankerl::nanobench::doNotOptimizeAway(found);
    });

    return 0;
}
