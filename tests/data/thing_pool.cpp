#include <doctest.h>

#include "fr/data/thing.hpp"
#include "fr/data/thing_pool.hpp"

namespace fr {

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
    CHECK(pool.get_by_idx_unchecked(a.idx()) == a);
    CHECK(pool.get_by_idx_unchecked(b.idx()) == b);
}

TEST_CASE("ThingPool - remove and reuse slot") {
    impl::ThingPool pool;

    Thing a = pool.handout();
    Thing b = pool.handout();

    pool.destroy(a);

    CHECK(!pool.check(a));
    CHECK(pool.check(b));

    Thing c = pool.handout();

    CHECK(c.idx() == a.idx());
    CHECK(c.gen() != a.gen());
    CHECK(!pool.check(a));
    CHECK(pool.check(c));
}
} // namespace fr
