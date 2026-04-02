#include "fr/core/slice.hpp"
#include "doctest.h"
#include <array>

namespace fr {

TEST_CASE("Slice - Construction and Basic Access") {
    std::array<S32, 5> data = {0, 1, 2, 3, 4};

    Slice<S32> s(data.data(), data.size());

    CHECK(s.size() == 5);
    CHECK(s[0] == 0);
    CHECK(s[4] == 4);

    // Modification through slice
    s[2] = 42;
    CHECK(data[2] == 42);
}

TEST_CASE("Slice - Const Safety") {
    std::array<S32, 5> data = {0, 1, 2, 3, 4};
    Slice<S32> mut_slice(data.data(), data.size());

    // Implicit conversion to const slice
    Slice<const S32> const_slice = mut_slice;

    CHECK(const_slice.size() == 5);
    CHECK(const_slice[0] == 0);

    // const_slice[0] = 10; // This would fail to compile
    static_assert(std::is_const_v<std::remove_reference_t<decltype(const_slice[0])>>);
    static_assert(!std::is_const_v<std::remove_reference_t<decltype(mut_slice[0])>>);
}

TEST_CASE("Slice - Slicing") {
    std::array<S32, 5> data = {0, 1, 2, 3, 4};
    Slice<S32> s(data.data(), data.size());

    SUBCASE("Sub-slice") {
        auto sub = s.slice(1, 3); // {1, 2, 3}
        CHECK(sub.size() == 3);
        CHECK(sub[0] == 1);
        CHECK(sub[2] == 3);
    }

    SUBCASE("Slice from") {
        auto sub = s.slice_from(2); // {2, 3, 4}
        CHECK(sub.size() == 3);
        CHECK(sub[0] == 2);
    }

    SUBCASE("Slice to") {
        auto sub = s.slice_to(2); // {0, 1, 2}
        CHECK(sub.size() == 3);
        CHECK(sub[2] == 2);
    }
}

TEST_CASE("Slice - Iterators") {
    std::array<S32, 3> data = {10, 20, 30};
    Slice<int> s(data.data(), data.size());

    S32 sum = 0;
    for (S32 val : s) {
        sum += val;
    }

    CHECK(sum == 60);

    auto it = s.begin();

    CHECK(*it == 10);
    ++it;
    CHECK(*it == 20);
}

} // namespace fr
