#include <doctest.h>

#include "fr/core/hash_map.hpp"
#include "fr/core/string.hpp"

namespace fr {

TEST_CASE("HashMap - Basic Operations") {
    HashMap<S32, String> map;

    CHECK(map.is_empty());
    CHECK(map.load() == 0);

    SUBCASE("Insert and Find") {
        CHECK(map.insert(1, String("One")));
        CHECK(map.insert(2, String("Two")));
        CHECK(map.insert(3, String("Three")));

        CHECK(map.load() == 3);
        CHECK(map.contains(1));
        CHECK(map.contains(2));
        CHECK(map.contains(3));
        CHECK(!map.contains(4));

        auto v1 = map.find(1);
        REQUIRE(v1.is_some());
        CHECK(*v1.unwrap() == "One");

        auto v4 = map.find(4);
        CHECK(v4.is_none());

        // Duplicate insert should fail
        CHECK(!map.insert(1, String("Uno")));
        CHECK(map.load() == 3);
    }

    SUBCASE("Operator[]") {
        map[10] = String("Ten");
        CHECK(map.load() == 1);
        CHECK(map[10] == "Ten");

        map[10] = String("Diez");
        CHECK(map[10] == "Diez");
        CHECK(map.load() == 1);

        // Accessing non-existent key should default-construct
        CHECK(map[20] == "");
        CHECK(map.load() == 2);
    }

    SUBCASE("Remove") {
        map.insert(1, String("One"));
        map.insert(2, String("Two"));

        CHECK(map.remove(1));
        CHECK(!map.contains(1));
        CHECK(map.load() == 1);

        CHECK(!map.remove(1)); // Already removed
    }

    SUBCASE("Clear") {
        map.insert(1, String("One"));
        map.insert(2, String("Two"));
        map.clear();
        CHECK(map.is_empty());
        CHECK(map.load() == 0);
    }
}

TEST_CASE("HashMap - Iteration and Structured Bindings") {
    HashMap<S32, S32> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    S32 key_sum = 0;
    S32 val_sum = 0;
    USize count = 0;

    for (auto pair : map) {
        auto &[k, v] = pair;
        key_sum += k;
        val_sum += v;
        count++;
    }

    CHECK(count == 3);
    CHECK(key_sum == 6);
    CHECK(val_sum == 60);

    // Mutable iteration
    for (auto pair : map) {
        auto &[k, v] = pair;
        v += 1;
    }

    CHECK(*map.find(1).unwrap() == 11);
}

TEST_CASE("HashMap - Move and Copy") {
    HashMap<S32, S32> original;
    original.insert(1, 10);
    original.insert(2, 20);

    SUBCASE("Copy constructor") {
        HashMap<S32, S32> copy = original;
        CHECK(copy.load() == 2);
        CHECK(copy.contains(1));
        CHECK(*copy.find(2).unwrap() == 20);
    }

    SUBCASE("Move constructor") {
        HashMap<S32, S32> moved = std::move(original);
        CHECK(moved.load() == 2);
        CHECK(original.is_empty());
        CHECK(moved.contains(1));
    }
}

} // namespace fr
