#include <doctest.h>

#include "fr/core/stack.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

TEST_CASE("Stack - Construction") {
    SUBCASE("Default") {
        Stack<S32> stack;

        CHECK(stack.size() == 0);
        CHECK(stack.capacity() == 0);
        CHECK(stack.is_empty());
    }

    SUBCASE("With capacity") {
        auto stack = Stack<S32>::with_capacity(10);

        CHECK(stack.size() == 0);
        CHECK(stack.capacity() == 10);
    }

    SUBCASE("Initializer list") {
        Stack<S32> stack = {1, 2, 3};

        CHECK(stack.size() == 3);
        CHECK(stack.slice()[0] == 1);
        CHECK(stack.slice()[2] == 3);
        CHECK(stack.top() == 3);
    }
}

TEST_CASE("Stack - Push and Pop") {
    Stack<S32> stack;

    stack.push(10);
    stack.emplace(20);
    stack.push(30);

    CHECK(stack.size() == 3);
    CHECK(stack.top() == 30);

    stack.pop();
    CHECK(stack.size() == 2);
    CHECK(stack.top() == 20);
}

TEST_CASE("Stack - Static factories") {
    auto sized = Stack<S32>::with_size(3);
    CHECK(sized.size() == 3);
    CHECK(sized.capacity() >= 3);

    auto filled = Stack<S32>::from_repeated(3, 42);
    CHECK(filled.size() == 3);
    CHECK(filled.top() == 42);
    CHECK(filled.slice()[0] == 42);
}

TEST_CASE("Stack - Underlying array access") {
    Stack<S32> stack;
    stack.push(1);

    DynamicArray<S32> &arr = stack.dynamic_array();
    arr.push_back(2);

    CHECK(stack.size() == 2);
    CHECK(stack.top() == 2);
}

} // namespace fr
