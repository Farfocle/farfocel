#include <doctest.h>

#include "fr/data/thing.hpp"
#include "fr/data/thing_pool.hpp"

namespace fr {

TEST_CASE("ThingPool - nil thing contract") {
    impl::ThingPool pool;

    CHECK(pool.alive_count() == 1);
    CHECK(pool.dead_count() == MAX_THINGS - 1);
    CHECK(pool.free_count() == 0);
}

TEST_CASE("ThingPool - handout and check") {
    impl::ThingPool pool;

    Thing a = pool.handout();
    Thing b = pool.handout();

    CHECK(a.idx() == 1);
    CHECK(b.idx() == 2);
    CHECK(!a.is_nil());
    CHECK(!b.is_nil());
    CHECK(a != b);

    CHECK(pool.check(a));
    CHECK(pool.check(b));
}

TEST_CASE("ThingPool - remove and reuse slot") {
    impl::ThingPool pool;

    Thing a = pool.handout();
    Thing b = pool.handout();

    pool.kill(a);

    CHECK(!pool.check(a));
    CHECK(pool.check(b));

    Thing c = pool.handout();

    CHECK(c.idx() == a.idx());
    CHECK(c.gen() != a.gen());

    CHECK(!pool.check(a));
    CHECK(pool.check(c));
}
} // namespace fr
