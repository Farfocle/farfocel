
#include <doctest.h>

#include "fr/core/pair.hpp"

namespace fr {

TEST_CASE("Pair - Basic Operations") {
    Pair<S32, F32> p(42, 3.14f);

    CHECK(p.first() == 42);
    CHECK(p.second() == doctest::Approx(3.14f));

    p.first() = 10;
    p.second() = 1.0f;
    CHECK(p.first() == 10);
    CHECK(p.second() == doctest::Approx(1.0f));
}

TEST_CASE("Pair - Const Access") {
    const Pair<S32, bool> p(7, true);
    CHECK(p.first() == 7);
    CHECK(p.second() == true);
}

TEST_CASE("Pair - Deduction Guide") {
    Pair p(100u, 2.5); // Pair<U32, F64>
    static_assert(std::is_same_v<decltype(p), Pair<U32, F64>>);
    CHECK(p.first() == 100u);
    CHECK(p.second() == doctest::Approx(2.5));
}

TEST_CASE("Pair - Structured Bindings") {
    Pair p(S32(1), S32(2));
    auto [x, y] = p;
    CHECK(x == 1);
    CHECK(y == 2);

    auto &[rx, ry] = p;
    rx = 10;
    ry = 20;
    CHECK(p.first() == 10);
    CHECK(p.second() == 20);
}

TEST_CASE("Pair - at<I>") {
    Pair p(10, 20);
    CHECK(p.at<0>() == 10);
    CHECK(p.at<1>() == 20);

    p.at<0>() = 30;
    CHECK(p.first() == 30);
}

TEST_CASE("Pair - Default Construction") {
    Pair<S32, F32> p;
    CHECK(p.first() == 0);
    CHECK(p.second() == 0.0f);
}

TEST_CASE("Pair - References") {
    S32 val = 10;
    Pair<S32 &, S32> p(val, 20);
    p.first() = 30;
    CHECK(val == 30);
}

} // namespace fr
