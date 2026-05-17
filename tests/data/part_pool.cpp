#include <doctest.h>

#include "fr/core/typedefs.hpp"
#include "fr/data/part_pool.hpp"

namespace fr {

struct TestPart {
    S32 value{0};
};

TEST_CASE("PartPool - emplace and get") {
    impl::PartPool<TestPart> pool;

    Thing a = Thing(3, 0);
    Thing b = Thing(7, 0);

    pool.emplace_unchecked(a, TestPart{42});
    pool.emplace_unchecked(b, TestPart{11});

    CHECK(pool.part_count() == 3);
    CHECK(pool.get_unchecked(a)->value == 42);
    CHECK(pool.get_unchecked(b)->value == 11);
}

TEST_CASE("PartPool - remove swaps last") {
    impl::PartPool<TestPart> pool;

    Thing a = Thing(2, 0);
    Thing b = Thing(5, 0);
    Thing c = Thing(9, 0);

    pool.emplace_unchecked(a, TestPart{10});
    pool.emplace_unchecked(b, TestPart{20});
    pool.emplace_unchecked(c, TestPart{30});

    CHECK(pool.part_count() == 4);

    pool.destroy(b);

    CHECK(pool.part_count() == 3);

    const auto &parts = pool.part_slice_with_stub();
    CHECK(parts.size() == 3);
    CHECK(parts[1].value == 10);
    CHECK(parts[2].value == 30);
}

TEST_CASE("PartPool - insert overloads") {
    impl::PartPool<TestPart> pool;

    Thing a = Thing(1, 0);
    Thing b = Thing(4, 0);

    TestPart a_part{5};
    pool.insert_unchecked(a, a_part);
    pool.insert_unchecked(b, TestPart{9});

    CHECK(pool.part_count() == 3);
    CHECK(pool.get_unchecked(a)->value == 5);
    CHECK(pool.get_unchecked(b)->value == 9);
}

} // namespace fr
