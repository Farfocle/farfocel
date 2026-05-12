#include <iostream>

#include "fr/core/bitset.hpp"
#include "fr/core/ctx.hpp"
#include "fr/core/format.hpp"
#include "fr/core/json.hpp"

S32 main() {
    fr::init_core_ctx();

    {
        fr::Bitset<10> bs;
        bs.zero_all();
        bs.one_bit(1);
        bs.one_bit(4);
        bs.one_bit(9);

        std::cout << "--- Basic Operations ---\n";
        std::cout << fr::format("Bitset size: {}\n", bs.size());
        std::cout << fr::format("Ones count: {}\n", bs.count_ones());
        std::cout << fr::format("Bit 4 is: {}\n", bs.check_bit(4) ? "SET" : "UNSET");

        std::cout << "\n--- each() ---\n";
        bs.each([](USize idx, bool val) {
            if (val) {
                std::cout << fr::format("Bit {} is ON\n", idx);
            }
        });

        std::cout << "\n--- each_one() ---\n";
        bs.each_one([](USize idx) { std::cout << fr::format("Found set bit at: {}\n", idx); });

        fr::Bitset<10> other;
        other.one_bit(1);
        other.one_bit(2);

        auto intersection = bs & other;
        std::cout << "\n--- Bitwise AND ---\n";
        intersection.each_one(
            [](USize idx) { std::cout << fr::format("Bit {} is set in both\n", idx); });

        std::cout << "\n--- Serialization ---\n";

        fr::JsonSerializer serializer({.pretty = true});
        bs.shape(serializer);

        fr::String json = serializer.consume();
        std::cout << "Serialized Bitset:\n" << json << "\n";
    }

    fr::shutdown_core_ctx();
    return 0;
}
