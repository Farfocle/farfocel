#include <doctest.h>

#include "fr/core/typedefs.hpp"
#include "fr/data/query.hpp"
#include "fr/data/registry.hpp"

namespace fr {

struct A {
    S32 value{0};
};

struct B {
    S32 value{0};
};

struct C {
    S32 value{0};
};

TEST_CASE("Registry - Query basic iteration") {
    impl::Registry reg;

    Thing t1 = reg.handout();
    reg.emplace<A>(t1, 10);
    reg.emplace<B>(t1, 20);

    Thing t2 = reg.handout();
    reg.emplace<A>(t2, 30);

    Thing t3 = reg.handout();
    reg.emplace<A>(t3, 40);
    reg.emplace<B>(t3, 50);

    USize count = 0;
    for (auto [thing, a, b] : reg.query<A, B>()) {
        ++count;

        if (a.value == 10) {
            CHECK(b.value == 20);
        } else if (a.value == 40) {
            CHECK(b.value == 50);
        } else {
            CHECK(false);
        }
    }

    CHECK(count == 2);
}

TEST_CASE("Registry - Query with exclusion") {
    impl::Registry reg;

    Thing t1 = reg.handout();
    reg.emplace<A>(t1, 1);
    reg.emplace<B>(t1, 2);

    Thing t2 = reg.handout();
    reg.emplace<A>(t2, 3);
    reg.emplace<B>(t2, 4);
    reg.emplace<C>(t2, 5);

    USize count = 0;
    for (auto [thing, a, b] : reg.query<A, B>().without<C>()) {
        ++count;

        CHECK(a.value == 1);
        CHECK(b.value == 2);
    }
    CHECK(count == 1);
}

TEST_CASE("Registry - Query smallest pool optimization") {
    impl::Registry reg;

    Thing t1 = reg.handout();
    reg.emplace<A>(t1, 1);
    reg.emplace<B>(t1, 10);

    Thing t2 = reg.handout();
    reg.emplace<A>(t2, 2);

    Thing t3 = reg.handout();
    reg.emplace<A>(t3, 3);

    USize count = 0;

    for (auto [thing, a, b] : reg.query<A, B>()) {
        ++count;

        CHECK(a.value == 1);
        CHECK(b.value == 10);
    }

    CHECK(count == 1);
}

TEST_CASE("Registry - Query and killed things") {
    impl::Registry reg;

    Thing t1 = reg.handout();
    reg.emplace<A>(t1, 1);

    Thing t2 = reg.handout();
    reg.emplace<A>(t2, 2);

    reg.kill(t1);

    USize count = 0;

    for (auto [thing, a] : reg.query<A>()) {
        ++count;
        CHECK(a.value == 2);
    }

    CHECK(count == 1);
}
} // namespace fr
