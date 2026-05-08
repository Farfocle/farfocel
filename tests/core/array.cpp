#include <doctest.h>

#include "fr/core/array.hpp"
#include "fr/core/string.hpp"

#include "fr/core/json.hpp"

namespace fr {

TEST_CASE("Array - Serialization") {
    Array<S32, 3> arr = {1, 2, 3};
    JsonSerializer writer;

    writer.prop("arr", arr);
    String json = writer.consume();

    CHECK(json.contains("\"@size\":3"));
    CHECK(json.contains("\"@items\":[1,2,3]"));

    Array<S32, 3> deserialized;
    JsonDeserializer reader(json.view());
    reader.prop("arr", deserialized);
    CHECK(reader.consume());

    CHECK(deserialized[0] == 1);
    CHECK(deserialized[1] == 2);
    CHECK(deserialized[2] == 3);
}

TEST_CASE("Array - Construction") {
    SUBCASE("Default construction") {
        Array<S32, 3> arr;

        CHECK(arr.size() == 3);
        CHECK(arr[0] == 0);
        CHECK(arr[1] == 0);
        CHECK(arr[2] == 0);
    }

    SUBCASE("Initializer list construction") {
        Array<S32, 3> arr = {1, 2, 3};

        CHECK(arr[0] == 1);
        CHECK(arr[1] == 2);
        CHECK(arr[2] == 3);
    }

    SUBCASE("Partial initializer list") {
        Array<S32, 5> arr = {1, 2};

        CHECK(arr[0] == 1);
        CHECK(arr[1] == 2);
        CHECK(arr[2] == 0);
        CHECK(arr[3] == 0);
        CHECK(arr[4] == 0);
    }
}

TEST_CASE("Array - Static Constructors") {
    SUBCASE("from_repeated") {
        auto arr = Array<S32, 3>::from_repeated(42);

        CHECK(arr[0] == 42);
        CHECK(arr[1] == 42);
        CHECK(arr[2] == 42);

        auto arr2 = Array<S32, 4>::from_repeated(7);

        CHECK(arr2.size() == 4);
        CHECK(arr2[0] == 7);
        CHECK(arr2[3] == 7);
    }
}

TEST_CASE("Array - Element Access") {
    Array<S32, 3> arr = {10, 20, 30};

    CHECK(arr[0] == 10);
    CHECK(arr.front() == 10);
    CHECK(arr.back() == 30);
    CHECK(arr.data()[1] == 20);

    arr[1] = 42;
    CHECK(arr[1] == 42);

    const Array<S32, 3> const_arr = {1, 2, 3};

    CHECK(const_arr[1] == 2);
    CHECK(const_arr.front() == 1);
    CHECK(const_arr.back() == 3);
}

TEST_CASE("Array - Iterators") {
    Array<S32, 3> arr = {1, 2, 3};
    S32 sum = 0;

    for (S32 x : arr) {
        sum += x;
    }

    CHECK(sum == 6);

    auto it = arr.begin();
    *it = 10;

    CHECK(arr[0] == 10);
}

TEST_CASE("Array - Slices") {
    Array<S32, 5> arr = {0, 1, 2, 3, 4};

    SUBCASE("Full slice") {
        auto s = arr.slice();
        CHECK(s.size() == 5);
        CHECK(s[0] == 0);
        CHECK(s[4] == 4);
    }

    SUBCASE("Sub slice") {
        auto s = arr.slice(1, 3); // {1, 2, 3}

        CHECK(s.size() == 3);
        CHECK(s[0] == 1);
        CHECK(s[2] == 3);
    }

    SUBCASE("Mutable slice") {
        auto s = arr.slice_mut();
        s[0] = 100;

        CHECK(arr[0] == 100);
    }
}

TEST_CASE("Array - Hashing") {
    Array<S32, 3> arr1 = {1, 2, 3};
    Array<S32, 3> arr2 = {1, 2, 3};
    Array<S32, 3> arr3 = {3, 2, 1};

    CHECK(arr1.hash().value == arr2.hash().value);
    CHECK(arr1.hash().value != arr3.hash().value);
}

TEST_CASE("Array - Structured Bindings") {
    Array<S32, 3> arr = {10, 20, 30};
    auto [x, y, z] = arr;

    CHECK(x == 10);
    CHECK(y == 20);
    CHECK(z == 30);

    auto &[rx, ry, rz] = arr;
    rx = 100;

    CHECK(arr[0] == 100);
}

TEST_CASE("Array - Zero size") {
    Array<S32, 0> arr;

    CHECK(arr.size() == 0);
    CHECK(arr.is_empty());
    CHECK(arr.begin() == arr.end());
}

} // namespace fr
