#include <doctest.h>

#include "fr/core/hash_set.hpp"
#include "fr/core/string.hpp"

namespace fr {

TEST_CASE("HashSet - Basic Operations") {
    HashSet<S32> set;

    CHECK(set.is_empty());
    CHECK(set.load() == 0);

    SUBCASE("Insert and Contains") {
        CHECK(set.insert(10));
        CHECK(set.insert(20));
        CHECK(set.insert(30));

        CHECK(set.load() == 3);
        CHECK(!set.is_empty());

        CHECK(set.contains(10));
        CHECK(set.contains(20));
        CHECK(set.contains(30));
        CHECK(!set.contains(40));

        // Duplicate insert should fail
        CHECK(!set.insert(10));
        CHECK(set.load() == 3);
    }

    SUBCASE("Emplace") {
        CHECK(set.emplace(100));
        CHECK(set.contains(100));
        CHECK(set.load() == 1);

        // Duplicate emplace should fail
        CHECK(!set.emplace(100));
        CHECK(set.load() == 1);
    }
    SUBCASE("Remove") {
        set.insert(10);
        set.insert(20);

        CHECK(set.remove(10));
        CHECK(!set.contains(10));
        CHECK(set.contains(20));
        CHECK(set.load() == 1);

        CHECK(!set.remove(10)); // Already removed
        CHECK(set.load() == 1);
    }

    SUBCASE("Clear") {
        set.insert(1);
        set.insert(2);
        set.clear();
        CHECK(set.is_empty());
        CHECK(set.load() == 0);
        CHECK(!set.contains(1));
    }
}

TEST_CASE("HashSet - String Keys") {
    HashSet<String> set;

    set.insert(String("Hello"));
    set.insert(String("World"));

    CHECK(set.contains("Hello"));
    CHECK(set.contains("World"));
    CHECK(!set.contains("Farfocel"));

    set.remove("Hello");
    CHECK(!set.contains("Hello"));
    CHECK(set.load() == 1);
}

TEST_CASE("HashSet - Iteration") {
    HashSet<S32> set;
    set.insert(10);
    set.insert(20);
    set.insert(30);

    S32 sum = 0;
    USize count = 0;
    for (S32 val : set) {
        sum += val;
        count++;
    }

    CHECK(count == 3);
    CHECK(sum == 60);
}

TEST_CASE("HashSet - Resizing and Rehash") {
    HashSet<S32> set;

    // Force many insertions to trigger growth
    const USize num_elements = 100;
    for (USize i = 0; i < num_elements; ++i) {
        set.insert(static_cast<S32>(i));
    }

    CHECK(set.load() == num_elements);
    CHECK(set.capacity() >= num_elements);

    bool all_found = true;
    for (USize i = 0; i < num_elements; ++i) {
        if (!set.contains(static_cast<S32>(i))) {
            all_found = false;
            break;
        }
    }
    CHECK(all_found);
}

TEST_CASE("HashSet - Move and Copy") {
    HashSet<S32> original;
    original.insert(1);
    original.insert(2);

    SUBCASE("Copy constructor") {
        HashSet<S32> copy = original;
        CHECK(copy.load() == 2);
        CHECK(copy.contains(1));
        CHECK(copy.contains(2));
    }

    SUBCASE("Move constructor") {
        HashSet<S32> moved = std::move(original);
        CHECK(moved.load() == 2);
        CHECK(original.is_empty());
        CHECK(moved.contains(1));
    }

    SUBCASE("Copy assignment") {
        HashSet<S32> copy;
        copy = original;
        CHECK(copy.load() == 2);
        CHECK(copy.contains(1));
    }

    SUBCASE("Move assignment") {
        HashSet<S32> moved;
        moved = std::move(original);
        CHECK(moved.load() == 2);
        CHECK(moved.contains(1));
        CHECK(original.is_empty());
    }
}

} // namespace fr
