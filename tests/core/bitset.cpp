#include <doctest.h>

#include "fr/core/bitset.hpp"

namespace fr {

TEST_CASE("Bitset - Basic Operations") {
    Bitset<10> bs;

    CHECK(bs.size() == 10);
    CHECK(!bs.is_empty());

    SUBCASE("Set and Check") {
        bs.zero_all();
        bs.one_bit(0);
        bs.one_bit(9);

        CHECK(bs.check_bit(0));
        CHECK(bs.check_bit(9));
        CHECK(bs.count_ones() == 2);
        CHECK(bs.count_zeros() == 8);
    }

    SUBCASE("set_bit and zero_bit") {
        bs.zero_all();
        bs.set_bit(3, true);
        CHECK(bs.check_bit(3));

        bs.zero_bit(3);
        CHECK(!bs.check_bit(3));
    }

    SUBCASE("flip_bit") {
        bs.zero_all();
        bs.set_bit(2, true);
        bs.flip_bit(2);
        CHECK(!bs.check_bit(2));
        bs.flip_bit(2);
        CHECK(bs.check_bit(2));
    }

    SUBCASE("one_all, zero_all, flip_all") {
        bs.one_all();
        for (USize i = 0; i < 10; ++i) {
            CHECK(bs.check_bit(i));
        }
        CHECK(bs.count_ones() == 10);
        CHECK(bs.count_zeros() == 0);

        bs.zero_all();
        for (USize i = 0; i < 10; ++i) {
            CHECK(!bs.check_bit(i));
        }
        CHECK(bs.count_ones() == 0);
        CHECK(bs.count_zeros() == 10);

        bs.flip_all();
        for (USize i = 0; i < 10; ++i) {
            CHECK(bs.check_bit(i));
        }
        CHECK(bs.count_ones() == 10);
        CHECK(bs.count_zeros() == 0);
    }

    SUBCASE("set_all") {
        bs.set_all(true);
        CHECK(bs.count_ones() == 10);
        CHECK(bs.count_zeros() == 0);

        bs.set_all(false);
        CHECK(bs.count_ones() == 0);
        CHECK(bs.count_zeros() == 10);
    }
}

TEST_CASE("Bitset - Bitwise Operators") {
    Bitset<4> a;
    a.set_bit(0, true);
    a.set_bit(1, true); // 0011

    Bitset<4> b;
    b.set_bit(1, true);
    b.set_bit(2, true); // 0110

    SUBCASE("AND") {
        auto res = a & b; // 0010
        CHECK(!res.check_bit(0));
        CHECK(res.check_bit(1));
        CHECK(!res.check_bit(2));
        CHECK(!res.check_bit(3));
    }

    SUBCASE("OR") {
        auto res = a | b; // 0111
        CHECK(res.check_bit(0));
        CHECK(res.check_bit(1));
        CHECK(res.check_bit(2));
        CHECK(!res.check_bit(3));
    }

    SUBCASE("XOR") {
        auto res = a ^ b; // 0101
        CHECK(res.check_bit(0));
        CHECK(!res.check_bit(1));
        CHECK(res.check_bit(2));
        CHECK(!res.check_bit(3));
    }

    SUBCASE("NOT") {
        auto res = ~a; // 1100
        CHECK(!res.check_bit(0));
        CHECK(!res.check_bit(1));
        CHECK(res.check_bit(2));
        CHECK(res.check_bit(3));
    }

    SUBCASE("compound operators") {
        Bitset<4> c = a;
        c &= b;
        CHECK(!c.check_bit(0));
        CHECK(c.check_bit(1));
        CHECK(!c.check_bit(2));
        CHECK(!c.check_bit(3));

        c = a;
        c |= b;
        CHECK(c.check_bit(0));
        CHECK(c.check_bit(1));
        CHECK(c.check_bit(2));
        CHECK(!c.check_bit(3));

        c = a;
        c ^= b;
        CHECK(c.check_bit(0));
        CHECK(!c.check_bit(1));
        CHECK(c.check_bit(2));
        CHECK(!c.check_bit(3));
    }
}

TEST_CASE("Bitset - Iteration") {
    Bitset<10> bs;
    bs.set_bit(1, true);
    bs.set_bit(4, true);
    bs.set_bit(9, true);

    SUBCASE("ones_begin / ones_end") {
        USize count = 0;
        USize indices[3] = {};

        for (auto it = bs.ones_begin(); it != bs.ones_end(); ++it) {
            indices[count++] = *it;
        }

        CHECK(count == 3);
        CHECK(indices[0] == 1);
        CHECK(indices[1] == 4);
        CHECK(indices[2] == 9);
    }

    SUBCASE("each") {
        USize set_bits = 0;

        bs.each([&](USize idx, bool val) {
            if (val) {
                ++set_bits;
                CHECK((idx == 1 || idx == 4 || idx == 9));
            }
        });

        CHECK(set_bits == 3);
    }

    SUBCASE("each_one") {
        USize count = 0;

        bs.each_one([&](USize idx) {
            ++count;
            CHECK((idx == 1 || idx == 4 || idx == 9));
        });

        CHECK(count == 3);
    }
}

TEST_CASE("Bitset - Large Bitset (multiple words)") {
    Bitset<128> bs;

    bs.set_bit(0, true);
    bs.set_bit(63, true);
    bs.set_bit(64, true);
    bs.set_bit(127, true);

    CHECK(bs.check_bit(0));
    CHECK(bs.check_bit(63));
    CHECK(bs.check_bit(64));
    CHECK(bs.check_bit(127));
    CHECK(bs.count_ones() == 4);
    CHECK(bs.count_zeros() == 124);

    bs.flip_all();

    CHECK(!bs.check_bit(0));
    CHECK(!bs.check_bit(63));
    CHECK(!bs.check_bit(64));
    CHECK(!bs.check_bit(127));
    CHECK(bs.check_bit(1));
    CHECK(bs.check_bit(65));
    CHECK(bs.count_ones() == 124);
    CHECK(bs.count_zeros() == 4);
}

TEST_CASE("Bitset - Zero size") {
    Bitset<0> bs;

    CHECK(bs.size() == 0);
    CHECK(bs.is_empty());
    CHECK(bs.ones_begin() == bs.ones_end());
    CHECK(bs.count_ones() == 0);
    CHECK(bs.count_zeros() == 0);
}

} // namespace fr
