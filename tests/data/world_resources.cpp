#include <doctest.h>

#include "fr/core/typedefs.hpp"
#include "fr/data/world.hpp"

namespace {

struct Score {
    S32 value{0};
};

struct Timer {
    F32 elapsed{0.0f};
};

struct Singleton {};

} // namespace

namespace fr {

TEST_CASE("Resources - check_resource returns false when absent") {
    World world;
    CHECK_FALSE(world.check_resource<Score>());
}

TEST_CASE("Resources - emplace_resource creates resource") {
    World world;
    world.emplace_resource<Score>(42);

    CHECK(world.check_resource<Score>());
}

TEST_CASE("Resources - get_resource returns correct value") {
    World world;
    world.emplace_resource<Score>(99);

    CHECK(world.get_resource<Score>().value == 99);
}

TEST_CASE("Resources - try_get_resource returns nullptr when absent") {
    World world;
    CHECK(world.try_get_resource<Score>() == nullptr);
}

TEST_CASE("Resources - try_get_resource returns pointer when present") {
    World world;
    world.emplace_resource<Score>(7);
    Score *s = world.try_get_resource<Score>();

    REQUIRE(s != nullptr);
    CHECK(s->value == 7);
}

TEST_CASE("Resources - emplace_resource replaces existing") {
    World world;
    world.emplace_resource<Score>(10);
    world.emplace_resource<Score>(20);

    CHECK(world.get_resource<Score>().value == 20);
}

TEST_CASE("Resources - insert_resource copy") {
    World world;
    Score s{55};
    world.insert_resource<Score>(s);
    CHECK(world.check_resource<Score>());
    CHECK(world.get_resource<Score>().value == 55);
}

TEST_CASE("Resources - insert_resource move") {
    World world;
    world.insert_resource<Score>(Score{77});

    CHECK(world.get_resource<Score>().value == 77);
}

TEST_CASE("Resources - insert_resource replaces existing") {
    World world;
    world.insert_resource<Score>(Score{1});
    world.insert_resource<Score>(Score{2});

    CHECK(world.get_resource<Score>().value == 2);
}

TEST_CASE("Resources - destroy<T> removes resource") {
    World world;
    world.emplace_resource<Score>(5);
    bool ok = world.destroy<Score>();

    CHECK(ok);
    CHECK_FALSE(world.check_resource<Score>());
    CHECK(world.try_get_resource<Score>() == nullptr);
}

TEST_CASE("Resources - destroy<T> returns false when absent") {
    World world;
    CHECK_FALSE(world.destroy<Score>());
}

TEST_CASE("Resources - multiple resource types coexist") {
    World world;
    world.emplace_resource<Score>(10);
    world.emplace_resource<Timer>(3.14f);

    CHECK(world.check_resource<Score>());
    CHECK(world.check_resource<Timer>());
    CHECK(world.get_resource<Score>().value == 10);
    CHECK(world.get_resource<Timer>().elapsed == doctest::Approx(3.14f));
}

TEST_CASE("Resources - destroying one resource leaves others") {
    World world;
    world.emplace_resource<Score>(10);
    world.emplace_resource<Timer>(1.0f);
    world.destroy<Score>();

    CHECK_FALSE(world.check_resource<Score>());
    CHECK(world.check_resource<Timer>());
}

TEST_CASE("Resources - get_resource reference is mutable") {
    World world;
    world.emplace_resource<Score>(0);
    world.get_resource<Score>().value = 123;

    CHECK(world.get_resource<Score>().value == 123);
}

TEST_CASE("Resources - trivial singleton resource") {
    World world;

    world.emplace_resource<Singleton>();
    CHECK(world.check_resource<Singleton>());

    world.destroy<Singleton>();
    CHECK_FALSE(world.check_resource<Singleton>());
}

TEST_CASE("Resources - Scope try_get_resource and get_resource") {
    World world;
    Scope scope(&world);
    world.emplace_resource<Score>(42);

    CHECK(scope.check_resource<Score>());
    CHECK(scope.get_resource<Score>().value == 42);

    Score *s = scope.try_get_resource<Score>();

    REQUIRE(s != nullptr);
    CHECK(s->value == 42);
}

TEST_CASE("Resources - Scope check_resource returns false when absent") {
    World world;
    Scope scope(&world);

    CHECK_FALSE(scope.check_resource<Score>());
    CHECK(scope.try_get_resource<Score>() == nullptr);
}

} // namespace fr
