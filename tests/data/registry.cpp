#include <doctest.h>

#include "fr/core/typedefs.hpp"
#include "fr/data/registry.hpp"

namespace fr {

struct Position {
    S32 x{0};
};

struct Velocity {
    S32 v{0};
};

TEST_CASE("Registry - thing lifecycle") {
    impl::Registry reg;

    Thing a = reg.handout();

    CHECK(reg.is_alive(a));
    CHECK(!a.is_nil());

    reg.kill(a);
    CHECK(!reg.is_alive(a));
    CHECK(reg.is_alive(Thing::nil()));
}

TEST_CASE("Registry - part pool existence and nil behavior") {
    impl::Registry reg;

    CHECK_FALSE(reg.has_part_pool<Position>());
    CHECK_FALSE(reg.has<Position>(Thing::nil()));

    Position *nil_pos = reg.emplace_checked<Position>(Thing::nil(), Position{7});

    CHECK(nil_pos != nullptr);
    CHECK(reg.has_part_pool<Position>());
    CHECK(reg.has<Position>(Thing::nil()));
    CHECK(reg.part_pool<Position>() != nullptr);
}

TEST_CASE("Registry - attach, destroy, and dead thing") {
    impl::Registry reg;

    Thing a = reg.handout();

    CHECK_FALSE(reg.destroy_checked<Velocity>(a));

    Position *pos = reg.emplace_checked<Position>(a, Position{3});

    CHECK(pos != nullptr);
    CHECK(reg.has<Position>(a));
    CHECK(pos->x == 3);

    // emplace_checked overrides if already present
    Position *pos2 = reg.emplace_checked<Position>(a, Position{9});
    CHECK(pos2 != nullptr);
    CHECK(pos2->x == 9);

    CHECK(reg.destroy_checked<Position>(a));
    CHECK_FALSE(reg.has<Position>(a));
    CHECK_FALSE(reg.destroy_checked<Position>(a));

    reg.emplace_checked<Position>(a, Position{11});
    reg.kill(a);

    CHECK_FALSE(reg.has<Position>(a));
    CHECK(reg.emplace_checked<Position>(a, Position{1}) == nullptr);
}

} // namespace fr
