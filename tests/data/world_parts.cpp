#include <doctest.h>

#include "fr/core/string.hpp"
#include "fr/data/world.hpp"

namespace {

struct Health {
    int hp{100};
};

struct Mana {
    int mp{50};
};

struct Name {
    fr::String value;
};

struct Trivial {};

} // namespace

namespace fr {

TEST_CASE("Parts - emplace_now inserts part") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a, 80);

    CHECK(world.has<Health>(a));
    CHECK(world.get<Health>(a).hp == 80);
}

TEST_CASE("Parts - emplace_now default args") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a);

    CHECK(world.get<Health>(a).hp == 100);
}

TEST_CASE("Parts - has returns false before insert") {
    World world;
    Thing a = world.spawn();

    CHECK_FALSE(world.has<Health>(a));
}

TEST_CASE("Parts - has returns true after emplace_now") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a);

    CHECK(world.has<Health>(a));
}

TEST_CASE("Parts - try_get returns nullptr when absent") {
    World world;
    Thing a = world.spawn();

    CHECK(world.try_get<Health>(a) == nullptr);
}

TEST_CASE("Parts - try_get returns pointer when present") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a, 42);
    Health *h = world.try_get<Health>(a);

    REQUIRE(h != nullptr);
    CHECK(h->hp == 42);
}

TEST_CASE("Parts - get returns reference to part") {
    World world;
    Thing a = world.spawn();
    world.emplace_now<Health>(a, 77);
    CHECK(world.get<Health>(a).hp == 77);
}

TEST_CASE("Parts - try_emplace_now overrides existing part") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a, 10);
    world.try_emplace_now<Health>(a, 99);

    CHECK(world.get<Health>(a).hp == 99);
}

TEST_CASE("Parts - try_emplace_now on dead thing returns nullptr") {
    World world;
    Thing a = world.spawn();

    world.kill(a);
    Health *h = world.try_emplace_now<Health>(a, 10);

    CHECK(h == nullptr);
}

TEST_CASE("Parts - try_emplace_now on nil returns stub pointer") {
    World world;
    Health *h = world.try_emplace_now<Health>(Thing::nil(), 5);

    CHECK(h != nullptr);
}

TEST_CASE("Parts - insert_now copy inserts or overrides") {
    World world;
    Thing a = world.spawn();

    Health h{55};
    world.insert_now<Health>(a, h);
    CHECK(world.get<Health>(a).hp == 55);

    Health h2{77};
    world.insert_now<Health>(a, h2);
    CHECK(world.get<Health>(a).hp == 77);
}

TEST_CASE("Parts - insert_now move inserts or overrides") {
    World world;
    Thing a = world.spawn();

    world.insert_now<Health>(a, Health{33});
    CHECK(world.get<Health>(a).hp == 33);

    world.insert_now<Health>(a, Health{44});
    CHECK(world.get<Health>(a).hp == 44);
}

TEST_CASE("Parts - insert_now on dead thing is no-op") {
    World world;
    Thing a = world.spawn();

    world.kill(a);
    world.insert_now<Health>(a, Health{99});

    CHECK_FALSE(world.has<Health>(a));
}

TEST_CASE("Parts - insert_now on nil is no-op") {
    World world;
    world.insert_now<Health>(Thing::nil(), Health{99});
}

TEST_CASE("Parts - try_destroy_now returns false when part absent") {
    World world;
    Thing a = world.spawn();

    CHECK_FALSE(world.try_destroy_now<Health>(a));
}

TEST_CASE("Parts - try_destroy_now returns true and removes part") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a);

    CHECK(world.try_destroy_now<Health>(a));
    CHECK_FALSE(world.has<Health>(a));
    CHECK(world.try_get<Health>(a) == nullptr);
}

TEST_CASE("Parts - try_destroy_now on dead thing returns false") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a);
    world.kill(a);

    CHECK_FALSE(world.try_destroy_now<Health>(a));
}

TEST_CASE("Parts - try_destroy_now on nil returns false") {
    World world;
    CHECK_FALSE(world.try_destroy_now<Health>(Thing::nil()));
}

TEST_CASE("Parts - destroy_now removes part") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a, 10);
    world.destroy_now<Health>(a);

    CHECK_FALSE(world.has<Health>(a));
}

TEST_CASE("Parts - multiple part types on same thing") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a, 100);
    world.emplace_now<Mana>(a, 30);

    CHECK(world.has<Health>(a));
    CHECK(world.has<Mana>(a));
    CHECK(world.get<Health>(a).hp == 100);
    CHECK(world.get<Mana>(a).mp == 30);
}

TEST_CASE("Parts - destroying one part leaves other intact") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a, 100);
    world.emplace_now<Mana>(a, 30);
    world.try_destroy_now<Health>(a);

    CHECK_FALSE(world.has<Health>(a));
    CHECK(world.has<Mana>(a));
    CHECK(world.get<Mana>(a).mp == 30);
}

TEST_CASE("Parts - kill destroys all parts") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a);
    world.emplace_now<Mana>(a);
    world.kill(a);

    CHECK(world.is_dead(a));
    CHECK_FALSE(world.has<Health>(a));
    CHECK_FALSE(world.has<Mana>(a));
}

TEST_CASE("Parts - has on nil after pool creation") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a);

    CHECK(world.has<Health>(Thing::nil()));
}

TEST_CASE("Parts - has on dead thing returns false") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a);
    world.kill(a);

    CHECK_FALSE(world.has<Health>(a));
}

TEST_CASE("Parts - try_get on dead thing returns nullptr") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<Health>(a, 10);
    world.kill(a);

    CHECK(world.try_get<Health>(a) == nullptr);
}

TEST_CASE("Parts - has_raw using TypeIdx") {
    World world;

    Thing a = world.spawn();
    TypeIdx tidx = TypeIdx::from_type<Trivial>();
    CHECK_FALSE(world.has_raw(tidx, a));

    world.emplace_now<Trivial>(a);
    CHECK(world.has_raw(tidx, a));
}

TEST_CASE("Parts - try_get_raw returns nullptr when absent") {
    World world;
    Thing a = world.spawn();
    TypeIdx tidx = TypeIdx::from_type<Health>();

    CHECK(world.try_get_raw(tidx, a) == nullptr);
}

TEST_CASE("Parts - Scope part operations forward to world") {
    World world;
    Scope scope(&world);

    Thing a = scope.spawn();
    scope.emplace_now<Health>(a, 60);

    CHECK(scope.has<Health>(a));
    CHECK(scope.get<Health>(a).hp == 60);
    CHECK(scope.try_get<Health>(a) != nullptr);
    CHECK(scope.try_destroy_now<Health>(a));
    CHECK_FALSE(scope.has<Health>(a));
}

TEST_CASE("Parts - Scope insert_now and destroy_now") {
    World world;
    Scope scope(&world);

    Thing a = scope.spawn();
    scope.insert_now<Health>(a, Health{25});

    CHECK(scope.get<Health>(a).hp == 25);
    scope.destroy_now<Health>(a);

    CHECK_FALSE(scope.has<Health>(a));
}

TEST_CASE("Parts - string part stores and retrieves value") {
    World world;
    Thing a = world.spawn();
    world.emplace_now<Name>(a, String{"hello"});

    CHECK(world.get<Name>(a).value == String{"hello"});
}

} // namespace fr
