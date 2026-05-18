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

    Thing a = reg.handout_thing();

    CHECK(reg.check_thing(a));
    CHECK(!a.is_nil());

    reg.kill_thing(a);
    CHECK(!reg.check_thing(a));
    CHECK(reg.check_thing(Thing::nil()));
}

TEST_CASE("Registry - part pool existence and nil behavior") {
    impl::Registry reg;

    CHECK_FALSE(reg.check_part_pool<Position>());
    CHECK_FALSE(reg.check_part<Position>(Thing::nil()));

    Position *nil_pos = reg.try_emplace_part<Position>(Thing::nil(), Position{7});

    CHECK(nil_pos != nullptr);
    CHECK(reg.check_part_pool<Position>());
    CHECK(reg.check_part<Position>(Thing::nil()));
    CHECK(reg.part_pool_of<Position>() != nullptr);
}

TEST_CASE("Registry - attach, destroy, and dead thing") {
    impl::Registry reg;

    Thing a = reg.handout_thing();

    CHECK_FALSE(reg.try_destroy_part<Velocity>(a));

    Position *pos = reg.try_emplace_part<Position>(a, Position{3});

    CHECK(pos != nullptr);
    CHECK(reg.check_part<Position>(a));
    CHECK(pos->x == 3);
    CHECK(reg.try_emplace_part<Position>(a, Position{9}) == nullptr);

    CHECK(reg.try_destroy_part<Position>(a));
    CHECK_FALSE(reg.check_part<Position>(a));
    CHECK_FALSE(reg.try_destroy_part<Position>(a));

    reg.try_emplace_part<Position>(a, Position{11});
    reg.kill_thing(a);

    CHECK_FALSE(reg.check_part<Position>(a));
    CHECK(reg.try_emplace_part<Position>(a, Position{1}) == nullptr);
}

} // namespace fr
