#include <doctest.h>

#include "fr/data/thing.hpp"
#include "fr/data/world.hpp"

namespace fr {

TEST_CASE("World - spawn returns alive non-nil thing") {
    World world;
    Thing a = world.spawn();

    CHECK_FALSE(a.is_nil());
    CHECK(world.is_alive(a));
    CHECK_FALSE(world.is_dead(a));
}

TEST_CASE("World - multiple spawns return unique things") {
    World world;
    Thing a = world.spawn();
    Thing b = world.spawn();
    Thing c = world.spawn();

    CHECK(a != b);
    CHECK(b != c);
    CHECK(a != c);
}

TEST_CASE("World - nil thing is alive and not dead") {
    World world;

    CHECK(world.is_alive(Thing::nil()));
    CHECK_FALSE(world.is_dead(Thing::nil()));
}

TEST_CASE("World - kill makes thing dead") {
    World world;
    Thing a = world.spawn();

    world.kill(a);

    CHECK(world.is_dead(a));
    CHECK_FALSE(world.is_alive(a));
}

TEST_CASE("World - kill nil is no-op") {
    World world;
    world.kill(Thing::nil());
    CHECK(world.is_alive(Thing::nil()));
}

TEST_CASE("World - kill dead thing is no-op") {
    World world;
    Thing a = world.spawn();

    world.kill(a);
    world.kill(a);

    CHECK(world.is_dead(a));
}

TEST_CASE("World - slot reuse after kill bumps generation") {
    World world;

    Thing a = world.spawn();
    world.kill(a);

    Thing b = world.spawn();

    CHECK(world.is_alive(b));
    CHECK(world.is_dead(a));
    CHECK(a.idx() == b.idx());
    CHECK(a.gen() != b.gen());
}

TEST_CASE("World - spawn_deferred returns alive thing immediately") {
    World world;
    Thing a = world.spawn_deferred();

    CHECK_FALSE(a.is_nil());
    CHECK(world.is_alive(a));
}

TEST_CASE("World - kill_deferred keeps thing alive until commit") {
    World world;
    Thing a = world.spawn();

    world.kill_deferred(a);
    CHECK(world.is_alive(a));

    world.commit_kill();
    CHECK(world.is_dead(a));
}

TEST_CASE("World - kill_deferred nil is no-op") {
    World world;

    world.kill_deferred(Thing::nil());
    world.commit_kill();

    CHECK(world.is_alive(Thing::nil()));
}

TEST_CASE("World - Scope forwards spawn and is_alive") {
    World world;
    Scope scope(&world);
    Thing a = scope.spawn();

    CHECK(world.is_alive(a));
    CHECK(scope.is_alive(a));
    CHECK_FALSE(scope.is_dead(a));
}

TEST_CASE("World - Scope kill") {
    World world;
    Scope scope(&world);
    Thing a = scope.spawn();

    scope.kill(a);

    CHECK(world.is_dead(a));
    CHECK(scope.is_dead(a));
}

TEST_CASE("World - Scope world() returns underlying world") {
    World world;
    Scope scope(&world);
    CHECK(&scope.world() == &world);
}

} // namespace fr
