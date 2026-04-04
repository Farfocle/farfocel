#include "fr/core/tuple.hpp"
#include "doctest.h"

namespace fr {

TEST_CASE("Tuple - Construction and Access") {
    Tuple<S32, F32, bool> tuple(42, 0.5f, true);

    CHECK(tuple.size() == 3);
    CHECK(tuple.at<0>() == 42);
    CHECK(tuple.at<1>() == doctest::Approx(0.5f));
    CHECK(tuple.at<2>());
}

TEST_CASE("Tuple - Type Deduction Guide") {
    Tuple tuple(7u, 1.25f, false);

    static_assert(std::is_same_v<decltype(tuple), Tuple<U32, F32, bool>>);
    CHECK(tuple.at<0>() == U32(7));
    CHECK(tuple.at<1>() == doctest::Approx(1.25f));
    CHECK(!tuple.at<2>());
}

TEST_CASE("Tuple - References and CV-Qualifiers") {
    S32 value = 9;
    const S32 c_value = 12;
    Tuple<S32 &, const S32 &> tuple(value, c_value);

    static_assert(std::is_same_v<decltype(tuple.at<0>()), S32 &>);
    static_assert(std::is_same_v<decltype(tuple.at<1>()), const S32 &>);

    tuple.at<0>() = 33;
    CHECK(value == 33);
    CHECK(tuple.at<1>() == 12);
}

TEST_CASE("Tuple - Structured Bindings") {
    Tuple<S32, F32, bool> tuple(11, 2.0f, true);
    auto [x, y, z] = tuple;

    CHECK(x == 11);
    CHECK(y == doctest::Approx(2.0f));
    CHECK(z);
}

TEST_CASE("Tuple - each") {
    Tuple<S32, S32, S32> tuple(1, 2, 3);
    S32 sum = 0;

    tuple.each([&sum](const auto &item) { sum += item; });

    CHECK(sum == 6);
}

TEST_CASE("Tuple - map") {
    Tuple<S32, F32, bool> tuple(10, 3.0f, true);
    auto mapped = tuple.map([](const auto &item) { return sizeof(item); });

    static_assert(std::is_same_v<decltype(mapped), Tuple<USize, USize, USize>>);
    CHECK(mapped.at<0>() == sizeof(S32));
    CHECK(mapped.at<1>() == sizeof(F32));
    CHECK(mapped.at<2>() == sizeof(bool));
}

TEST_CASE("Tuple - first, second, last") {
    Tuple tuple(1, 2.0f, 3u);

    CHECK(tuple.first() == 1);
    CHECK(tuple.second() == doctest::Approx(2.0f));
    CHECK(tuple.last() == 3u);

    // Test with single element tuple
    Tuple single(42);
    CHECK(single.first() == 42);
    CHECK(single.last() == 42);

    // Test mutation through these methods
    tuple.first() = 10;
    tuple.last() = 30u;
    CHECK(tuple.at<0>() == 10);
    CHECK(tuple.at<2>() == 30u);
}

} // namespace fr
