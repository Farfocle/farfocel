#include <doctest.h>

#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"

namespace {

struct QPos {
    float x{0.0f};
    float y{0.0f};
};

struct QVel {
    float dx{0.0f};
    float dy{0.0f};
};

struct QTag {};

} // namespace

namespace fr {

TEST_CASE("Query - empty world yields no results") {
    World world;
    int count = 0;
    for (auto [thing, pos] : world.query<QPos>()) {
        (void)thing;
        (void)pos;
        ++count;
    }
    CHECK(count == 0);
}

TEST_CASE("Query - single part query iterates matching things") {
    World world;
    Thing a = world.spawn();
    Thing b = world.spawn();
    Thing c = world.spawn();

    world.emplace_now<QPos>(a, 1.0f, 2.0f);
    world.emplace_now<QPos>(b, 3.0f, 4.0f);

    int count = 0;
    for (auto [thing, pos] : world.query<QPos>()) {
        (void)thing;
        (void)pos;
        ++count;
    }
    CHECK(count == 2);
    (void)c;
}

TEST_CASE("Query - yields correct thing and part values") {
    World world;
    Thing a = world.spawn();
    world.emplace_now<QPos>(a, 7.0f, 8.0f);

    for (auto [thing, pos] : world.query<QPos>()) {
        CHECK(thing == a);
        CHECK(pos.x == 7.0f);
        CHECK(pos.y == 8.0f);
    }
}

TEST_CASE("Query - multi-part query only yields things with all parts") {
    World world;
    Thing a = world.spawn();
    Thing b = world.spawn();
    Thing c = world.spawn();

    world.emplace_now<QPos>(a);
    world.emplace_now<QVel>(a);

    world.emplace_now<QPos>(b);

    world.emplace_now<QVel>(c);

    int count = 0;
    for (auto [thing, pos, vel] : world.query<QPos, QVel>()) {
        CHECK(thing == a);
        (void)pos;
        (void)vel;
        ++count;
    }
    CHECK(count == 1);
}

TEST_CASE("Query - multi-part query yields correct values") {
    World world;
    Thing a = world.spawn();
    world.emplace_now<QPos>(a, 1.0f, 2.0f);
    world.emplace_now<QVel>(a, 0.5f, 0.5f);

    for (auto [thing, pos, vel] : world.query<QPos, QVel>()) {
        CHECK(thing == a);
        CHECK(pos.x == 1.0f);
        CHECK(vel.dx == 0.5f);
    }
}

TEST_CASE("Query - part values are mutable through iterator") {
    World world;
    Thing a = world.spawn();
    world.emplace_now<QPos>(a, 0.0f, 0.0f);

    for (auto [thing, pos] : world.query<QPos>()) {
        pos.x = 99.0f;
    }

    CHECK(world.get<QPos>(a).x == 99.0f);
}

TEST_CASE("Query - reverse_query iterates in reverse order") {
    World world;
    Thing a = world.spawn();
    Thing b = world.spawn();
    Thing c = world.spawn();

    world.emplace_now<QTag>(a);
    world.emplace_now<QTag>(b);
    world.emplace_now<QTag>(c);

    int fwd_count = 0;
    Thing fwd_first;
    for (auto [thing, tag] : world.query<QTag>()) {
        if (fwd_count == 0)
            fwd_first = thing;
        ++fwd_count;
    }

    int rev_count = 0;
    Thing rev_first;
    for (auto [thing, tag] : world.reverse_query<QTag>()) {
        if (rev_count == 0)
            rev_first = thing;
        ++rev_count;
    }

    CHECK(fwd_count == 3);
    CHECK(rev_count == 3);
    CHECK(fwd_first != rev_first);
}

TEST_CASE("Query - QueryOptions without excludes things") {
    World world;
    Thing a = world.spawn();
    Thing b = world.spawn();

    world.emplace_now<QPos>(a);
    world.emplace_now<QPos>(b);
    world.emplace_now<QVel>(b);

    QueryOptions opts;
    opts.without = Signature::from_parts<QVel>();

    int count = 0;
    Thing matched;
    for (auto [thing, pos] : world.query<QPos>(opts)) {
        matched = thing;
        ++count;
    }
    CHECK(count == 1);
    CHECK(matched == a);
}

TEST_CASE("Query - killed thing not returned in query") {
    World world;
    Thing a = world.spawn();
    Thing b = world.spawn();

    world.emplace_now<QPos>(a);
    world.emplace_now<QPos>(b);

    world.kill(a);

    int count = 0;
    for (auto [thing, pos] : world.query<QPos>()) {
        CHECK(thing == b);
        ++count;
    }
    CHECK(count == 1);
}

TEST_CASE("Query - shallow_query iterates direct children only") {
    World world;
    Thing root = world.spawn();
    Thing c1 = world.spawn();
    Thing c2 = world.spawn();
    Thing gc = world.spawn();

    world.emplace_now<Relations>(root);
    world.emplace_now<Relations>(c1);
    world.emplace_now<Relations>(c2);
    world.emplace_now<Relations>(gc);
    world.emplace_now<QPos>(c1);
    world.emplace_now<QPos>(c2);
    world.emplace_now<QPos>(gc);

    world.attach_child_now(root, c1);
    world.attach_child_now(root, c2);
    world.attach_child_now(c1, gc);

    int count = 0;
    for (auto [thing, pos] : world.shallow_query<QPos>(root)) {
        (void)pos;
        CHECK((thing == c1 || thing == c2));
        ++count;
    }
    CHECK(count == 2);
}

TEST_CASE("Query - deep_query iterates all descendants") {
    World world;
    Thing root = world.spawn();
    Thing c1 = world.spawn();
    Thing gc1 = world.spawn();
    Thing gc2 = world.spawn();

    world.emplace_now<Relations>(root);
    world.emplace_now<Relations>(c1);
    world.emplace_now<Relations>(gc1);
    world.emplace_now<Relations>(gc2);
    world.emplace_now<QTag>(c1);
    world.emplace_now<QTag>(gc1);
    world.emplace_now<QTag>(gc2);

    world.attach_child_now(root, c1);
    world.attach_child_now(c1, gc1);
    world.attach_child_now(c1, gc2);

    int count = 0;
    for (auto [thing, tag] : world.deep_query<QTag>(root)) {
        (void)thing;
        (void)tag;
        ++count;
    }
    CHECK(count == 3);
}

TEST_CASE("Query - deep_query on thing without children yields empty") {
    World world;
    Thing root = world.spawn();
    world.emplace_now<Relations>(root);
    world.emplace_now<QTag>(root);

    int count = 0;
    for (auto [thing, tag] : world.deep_query<QTag>(root)) {
        (void)thing;
        (void)tag;
        ++count;
    }
    CHECK(count == 0);
}

TEST_CASE("Query - top_down_query yields parents before children") {
    World world;
    Thing root = world.spawn();
    Thing child = world.spawn();
    Thing grandchild = world.spawn();

    world.emplace_now<Relations>(root);
    world.emplace_now<Relations>(child);
    world.emplace_now<Relations>(grandchild);
    world.emplace_now<QPos>(root, 0.0f, 0.0f);
    world.emplace_now<QPos>(child, 1.0f, 0.0f);
    world.emplace_now<QPos>(grandchild, 2.0f, 0.0f);

    world.attach_child_now(root, child);
    world.attach_child_now(child, grandchild);
    world.sort_by_hierarchy_depth<QPos>();

    int count = 0;
    float prev_depth = -1.0f;
    for (auto [thing, pos] : world.top_down_query<QPos>()) {
        float depth = static_cast<float>(world.get<Relations>(thing).depth);
        CHECK(depth >= prev_depth);
        prev_depth = depth;
        ++count;
    }
    CHECK(count == 3);
}

TEST_CASE("Query - bottom_up_query yields children before parents") {
    World world;
    Thing root = world.spawn();
    Thing child = world.spawn();
    Thing grandchild = world.spawn();

    world.emplace_now<Relations>(root);
    world.emplace_now<Relations>(child);
    world.emplace_now<Relations>(grandchild);
    world.emplace_now<QPos>(root, 0.0f, 0.0f);
    world.emplace_now<QPos>(child, 1.0f, 0.0f);
    world.emplace_now<QPos>(grandchild, 2.0f, 0.0f);

    world.attach_child_now(root, child);
    world.attach_child_now(child, grandchild);
    world.sort_by_hierarchy_depth<QPos>();

    int count = 0;
    float prev_depth = 999.0f;
    for (auto [thing, pos] : world.bottom_up_query<QPos>()) {
        float depth = static_cast<float>(world.get<Relations>(thing).depth);
        CHECK(depth <= prev_depth);
        prev_depth = depth;
        ++count;
    }
    CHECK(count == 3);
}

TEST_CASE("Query - Scope query same as world query") {
    World world;
    Scope scope(&world);
    Thing a = world.spawn();
    world.emplace_now<QTag>(a);

    int count = 0;
    for (auto [thing, tag] : scope.query<QTag>()) {
        CHECK(thing == a);
        ++count;
    }
    CHECK(count == 1);
}

} // namespace fr
