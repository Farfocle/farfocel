#include <doctest.h>

#include "fr/data/thing.hpp"
#include "fr/data/thing_pool.hpp"

namespace fr {

TEST_CASE("ThingPool - handout and check") {
    ThingPool pool;

    Thing a = pool.handout();
    Thing b = pool.handout();

    CHECK(!a.is_nil());
    CHECK(!b.is_nil());
    CHECK(a != b);

    CHECK(pool.check(a));
    CHECK(pool.check(b));
    CHECK(pool.get(a.idx()) == a);
    CHECK(pool.get(b.idx()) == b);
}

TEST_CASE("ThingPool - remove and reuse slot") {
    ThingPool pool;

    Thing a = pool.handout();
    Thing b = pool.handout();

    CHECK(pool.remove(a));
    CHECK(!pool.check(a));
    CHECK(pool.check(b));

    Thing c = pool.handout();

    CHECK(c.idx() == a.idx());
    CHECK(c.gen() != a.gen());
    CHECK(pool.check(c));
    CHECK(!pool.check(a));
}

TEST_CASE("ThingPool - remove invalid") {
    ThingPool pool;

    CHECK(!pool.remove(Thing::nil()));

    Thing a = pool.handout();
    CHECK(pool.remove(a));
    CHECK(!pool.remove(a));
}

} // namespace fr
