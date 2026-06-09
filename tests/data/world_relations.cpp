#include <doctest.h>

#include "fr/data/relations.hpp"
#include "fr/data/world.hpp"

namespace fr {

static Thing make_node(World &world) {
    Thing t = world.spawn();
    world.emplace_now<Relations>(t);
    return t;
}

TEST_CASE("Relations - attach_child_now links parent and child") {
    World world;
    Thing parent = make_node(world);
    Thing child = make_node(world);

    bool ok = world.attach_child_now(parent, child);
    CHECK(ok);

    Relations &pr = world.get<Relations>(parent);
    Relations &cr = world.get<Relations>(child);

    CHECK(cr.parent == parent);
    CHECK(pr.first_child == child);
}

TEST_CASE("Relations - attach_child_now with nil parent returns false") {
    World world;
    Thing child = make_node(world);
    bool ok = world.attach_child_now(Thing::nil(), child);
    CHECK_FALSE(ok);
}

TEST_CASE("Relations - attach_child_now with nil child returns false") {
    World world;
    Thing parent = make_node(world);
    bool ok = world.attach_child_now(parent, Thing::nil());
    CHECK_FALSE(ok);
}

TEST_CASE("Relations - multiple children form a sibling chain") {
    World world;
    Thing parent = make_node(world);
    Thing c1 = make_node(world);
    Thing c2 = make_node(world);
    Thing c3 = make_node(world);

    world.attach_child_now(parent, c1);
    world.attach_child_now(parent, c2);
    world.attach_child_now(parent, c3);

    Relations &pr = world.get<Relations>(parent);
    Relations &r1 = world.get<Relations>(c1);
    Relations &r2 = world.get<Relations>(c2);
    Relations &r3 = world.get<Relations>(c3);

    CHECK(pr.first_child == c3);
    CHECK(r3.next_sibling == c2);
    CHECK(r2.next_sibling == c1);
    CHECK(r1.next_sibling.is_nil());

    CHECK(r3.parent == parent);
    CHECK(r2.parent == parent);
    CHECK(r1.parent == parent);
}

TEST_CASE("Relations - detach_child_now returns false for non-child") {
    World world;
    Thing parent = make_node(world);
    Thing child = make_node(world);
    bool ok = world.detach_child_now(parent, child);
    CHECK_FALSE(ok);
}

TEST_CASE("Relations - detach_child_now removes child from parent") {
    World world;
    Thing parent = make_node(world);
    Thing child = make_node(world);

    world.attach_child_now(parent, child);
    bool ok = world.detach_child_now(parent, child);
    CHECK(ok);

    Relations &pr = world.get<Relations>(parent);
    Relations &cr = world.get<Relations>(child);

    CHECK(pr.first_child.is_nil());
    CHECK(cr.parent.is_nil());
}

TEST_CASE("Relations - detach_child_now with nil parent returns false") {
    World world;
    Thing child = make_node(world);
    CHECK_FALSE(world.detach_child_now(Thing::nil(), child));
}

TEST_CASE("Relations - detach middle child repairs sibling chain") {
    World world;
    Thing parent = make_node(world);
    Thing c1 = make_node(world);
    Thing c2 = make_node(world);
    Thing c3 = make_node(world);

    world.attach_child_now(parent, c1);
    world.attach_child_now(parent, c2);
    world.attach_child_now(parent, c3);

    world.detach_child_now(parent, c2);

    Relations &r3 = world.get<Relations>(c3);
    Relations &r1 = world.get<Relations>(c1);
    Relations &r2 = world.get<Relations>(c2);

    CHECK(r3.next_sibling == c1);
    CHECK(r1.prev_sibling == c3);
    CHECK(r2.parent.is_nil());
    CHECK(r2.next_sibling.is_nil());
    CHECK(r2.prev_sibling.is_nil());
}

TEST_CASE("Relations - update_hierarchy sets correct depths") {
    World world;
    Thing root = make_node(world);
    Thing child = make_node(world);
    Thing grandchild = make_node(world);

    world.attach_child_now(root, child);
    world.attach_child_now(child, grandchild);
    world.update_hierarchy(root);

    CHECK(world.get<Relations>(root).depth == ROOT_HIERARCHY_DEPTH);
    CHECK(world.get<Relations>(child).depth == 1u);
    CHECK(world.get<Relations>(grandchild).depth == 2u);
}

TEST_CASE("Relations - update_hierarchy on nil is no-op") {
    World world;
    world.update_hierarchy(Thing::nil());
}

TEST_CASE("Relations - newly attached child gets correct depth") {
    World world;
    Thing root = make_node(world);
    Thing child = make_node(world);
    world.attach_child_now(root, child);

    CHECK(world.get<Relations>(child).depth == 1u);
}

TEST_CASE("Relations - deferred attach_child applies on commit") {
    World world;
    Thing parent = make_node(world);
    Thing child = make_node(world);

    world.attach_child(parent, child);
    CHECK(world.get<Relations>(child).parent.is_nil());

    world.commit_attach_child();

    CHECK(world.get<Relations>(child).parent == parent);
    CHECK(world.get<Relations>(parent).first_child == child);
}

TEST_CASE("Relations - deferred detach_child applies on commit") {
    World world;
    Thing parent = make_node(world);
    Thing child = make_node(world);

    world.attach_child_now(parent, child);
    world.detach_child(parent, child);
    CHECK(world.get<Relations>(child).parent == parent);

    world.commit_detach_child();

    CHECK(world.get<Relations>(child).parent.is_nil());
    CHECK(world.get<Relations>(parent).first_child.is_nil());
}

TEST_CASE("Relations - Scope attach_child_now") {
    World world;
    Scope scope(&world);
    Thing parent = make_node(world);
    Thing child = make_node(world);

    CHECK(scope.attach_child_now(parent, child));
    CHECK(world.get<Relations>(child).parent == parent);
}

TEST_CASE("Relations - Scope detach_child_now") {
    World world;
    Scope scope(&world);
    Thing parent = make_node(world);
    Thing child = make_node(world);

    scope.attach_child_now(parent, child);
    CHECK(scope.detach_child_now(parent, child));
    CHECK(world.get<Relations>(child).parent.is_nil());
}

TEST_CASE("Relations - sort_by_hierarchy_depth preserves all entries") {
    World world;
    Thing root = make_node(world);
    Thing c1 = make_node(world);
    Thing c2 = make_node(world);
    Thing gc = make_node(world);

    world.attach_child_now(root, c1);
    world.attach_child_now(root, c2);
    world.attach_child_now(c1, gc);

    world.sort_by_hierarchy_depth<Relations>();

    int count = 0;
    for (auto [thing, rel] : world.query<Relations>()) {
        (void)thing;
        (void)rel;
        ++count;
    }

    CHECK(count == 4);
}

} // namespace fr
