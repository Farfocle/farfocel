#include <doctest.h>

#include "fr/data/part_pool.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

struct TestPart {
    S32 value{0};
};

TEST_CASE("PartPool - emplace and get") {
    PartPool<TestPart> pool;

    pool.emplace(3, TestPart{42});
    pool.emplace(7, TestPart{11});

    CHECK(pool.size() == 2);
    CHECK(pool.get(3).value == 42);
    CHECK(pool.get(7).value == 11);
}

TEST_CASE("PartPool - remove swaps last") {
    PartPool<TestPart> pool;

    pool.emplace(2, TestPart{10});
    pool.emplace(5, TestPart{20});
    pool.emplace(9, TestPart{30});

    CHECK(pool.size() == 3);

    pool.remove(5);

    CHECK(pool.size() == 2);

    const auto &parts = pool.parts();
    CHECK(parts.size() == 2);
    CHECK((parts[0].value == 10 || parts[0].value == 30));
    CHECK((parts[1].value == 10 || parts[1].value == 30));
}

TEST_CASE("PartPool - insert overloads") {
    PartPool<TestPart> pool;

    TestPart a{5};
    pool.insert(1, a);
    pool.insert(4, TestPart{9});

    CHECK(pool.size() == 2);
    CHECK(pool.get(1).value == 5);
    CHECK(pool.get(4).value == 9);
}

} // namespace fr
