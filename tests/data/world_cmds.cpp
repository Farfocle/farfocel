#include <doctest.h>

#include "fr/data/cmd.hpp"
#include "fr/data/parts.hpp"
#include "fr/data/world.hpp"

namespace {

struct CPos {
    float x{0.0f};
    float y{0.0f};
};

struct CVel {
    float dx{0.0f};
    float dy{0.0f};
};

struct CHealth {
    int hp{100};
};

} // namespace

namespace fr {

TEST_CASE("Cmds - deferred insert does not apply before commit") {
    World world;
    Thing a = world.spawn();
    world.insert<CPos>(a, CPos{1.0f, 2.0f});

    CHECK_FALSE(world.has<CPos>(a));
}

TEST_CASE("Cmds - commit_insert applies deferred insert") {
    World world;
    Thing a = world.spawn();

    world.insert<CPos>(a, CPos{1.0f, 2.0f});
    world.commit_insert();

    CHECK(world.has<CPos>(a));
    CHECK(world.get<CPos>(a).x == 1.0f);
    CHECK(world.get<CPos>(a).y == 2.0f);
}

TEST_CASE("Cmds - insert move records and commits correctly") {
    World world;
    Thing a = world.spawn();

    world.insert<CPos>(a, CPos{5.0f, 6.0f});
    world.commit_insert();

    CHECK(world.get<CPos>(a).x == 5.0f);
}

TEST_CASE("Cmds - deferred destroy does not apply before commit") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<CPos>(a, 1.0f, 2.0f);
    world.destroy<CPos>(a);

    CHECK(world.has<CPos>(a));
}

TEST_CASE("Cmds - commit_destroy applies deferred destroy") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<CPos>(a, 1.0f, 2.0f);
    world.destroy<CPos>(a);
    world.commit_destroy();

    CHECK_FALSE(world.has<CPos>(a));
}

TEST_CASE("Cmds - deferred destroy on absent part is no-op") {
    World world;
    Thing a = world.spawn();

    world.destroy<CPos>(a);
    world.commit_destroy();

    CHECK_FALSE(world.has<CPos>(a));
}

TEST_CASE("Cmds - deferred insert on dead thing is no-op") {
    World world;
    Thing a = world.spawn();

    world.kill(a);
    world.insert<CPos>(a, CPos{});
    world.commit_insert();

    CHECK_FALSE(world.has<CPos>(a));
}

TEST_CASE("Cmds - deferred insert on thing that already has part overrides it") {
    World world;
    Thing a = world.spawn();

    world.emplace_now<CPos>(a, 9.0f, 9.0f);
    world.insert<CPos>(a, CPos{1.0f, 1.0f});
    world.commit_insert();

    CHECK(world.get<CPos>(a).x == 1.0f);
    CHECK(world.get<CPos>(a).y == 1.0f);
}

TEST_CASE("Cmds - commit applies all command types in order") {
    World world;
    Thing a = world.spawn();
    Thing b = world.spawn();

    world.emplace_now<CPos>(a, 1.0f, 0.0f);
    world.emplace_now<CVel>(b, 0.5f, 0.5f);

    world.destroy<CPos>(a);
    world.insert<CVel>(a, CVel{1.0f, 1.0f});

    world.commit();

    CHECK_FALSE(world.has<CPos>(a));
    CHECK(world.has<CVel>(a));
    CHECK(world.get<CVel>(a).dx == 1.0f);
}

TEST_CASE("Cmds - mutate_raw records and commits a value change") {
    World world;
    Thing a = world.spawn();
    world.emplace_now<CPos>(a, 1.0f, 2.0f);

    CPos prev = world.get<CPos>(a);
    CPos next{10.0f, 20.0f};
    TypeIdx tidx = TypeIdx::from_type<CPos>();
    world.mutate_raw(tidx, a, &prev, &next);

    world.commit_mutate();

    CHECK(world.get<CPos>(a).x == 10.0f);
    CHECK(world.get<CPos>(a).y == 20.0f);
}

TEST_CASE("Cmds - commit_inverse reverses an insert (destroy)") {
    World world;
    Thing a = world.spawn();
    world.insert<CPos>(a, CPos{3.0f, 4.0f});
    world.commit();
    REQUIRE(world.has<CPos>(a));

    world.cmd_batch().reset();
    world.destroy<CPos>(a);
    world.commit_inverse();

    CHECK(world.has<CPos>(a));
    CHECK(world.get<CPos>(a).x == 3.0f);
}

TEST_CASE("Cmds - kill_deferred committed via commit()") {
    World world;
    Thing a = world.spawn();
    world.kill_deferred(a);
    CHECK(world.is_alive(a));
    world.commit();
    CHECK(world.is_dead(a));
}

TEST_CASE("Cmds - spawn_deferred committed via commit() is no-op (thing already alive)") {
    World world;
    Thing a = world.spawn_deferred();
    CHECK(world.is_alive(a));
    world.commit();
    CHECK(world.is_alive(a));
}

TEST_CASE("Cmds - commit resets batch after applying") {
    World world;
    Thing a = world.spawn();
    world.insert<CPos>(a, CPos{1.0f, 1.0f});
    world.commit();
    CHECK(world.has<CPos>(a));

    world.commit();
    CHECK(world.has<CPos>(a));
}

TEST_CASE("Cmds - deferred attach_child commits correctly") {
    World world;
    Thing parent = world.spawn();
    Thing child = world.spawn();
    world.emplace_now<RelationsPart>(parent);
    world.emplace_now<RelationsPart>(child);

    world.attach_child(parent, child);
    CHECK(world.get<RelationsPart>(child).parent.is_nil());
    world.commit_attach_child();
    CHECK(world.get<RelationsPart>(child).parent == parent);
}

TEST_CASE("Cmds - deferred detach_child commits correctly") {
    World world;
    Thing parent = world.spawn();
    Thing child = world.spawn();
    world.emplace_now<RelationsPart>(parent);
    world.emplace_now<RelationsPart>(child);
    world.attach_child_now(parent, child);

    world.detach_child(parent, child);
    CHECK(world.get<RelationsPart>(child).parent == parent);
    world.commit_detach_child();
    CHECK(world.get<RelationsPart>(child).parent.is_nil());
}

TEST_CASE("Cmds - commit_from applies commands from external batch") {
    World world;
    Thing a = world.spawn();
    world.ensure<CPos>();

    CmdBatch batch;
    batch.record_insert<CPos>(a, CPos{5.0f, 6.0f});

    world.commit_insert_from(batch);
    CHECK(world.has<CPos>(a));
    CHECK(world.get<CPos>(a).x == 5.0f);
}

TEST_CASE("Cmds - commit_destroy_from applies destroy from external batch") {
    World world;
    Thing a = world.spawn();
    world.emplace_now<CPos>(a, 1.0f, 2.0f);

    CPos snap = world.get<CPos>(a);
    CmdBatch batch;
    batch.record_destroy<CPos>(a, snap);

    world.commit_destroy_from(batch);
    CHECK_FALSE(world.has<CPos>(a));
}

TEST_CASE("Cmds - Scope deferred insert and commit") {
    World world;
    Scope scope(&world);
    Thing a = scope.spawn();

    scope.insert<CHealth>(a, CHealth{75});
    CHECK_FALSE(scope.has<CHealth>(a));
    world.commit_insert();
    CHECK(scope.has<CHealth>(a));
    CHECK(scope.get<CHealth>(a).hp == 75);
}

TEST_CASE("Cmds - Scope deferred destroy and commit") {
    World world;
    Scope scope(&world);
    Thing a = scope.spawn();

    world.emplace_now<CHealth>(a, 50);
    scope.destroy<CHealth>(a);
    CHECK(scope.has<CHealth>(a));
    world.commit_destroy();
    CHECK_FALSE(scope.has<CHealth>(a));
}

} // namespace fr
