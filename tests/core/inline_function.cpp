#include <doctest.h>

#include "fr/core/inline_function.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

TEST_CASE("InlineFunction - nil") {
    InlineFunction<S32(S32), 32> fn;

    CHECK(fn.is_nil());
    CHECK(!fn);
}

TEST_CASE("InlineFunction - invoke lambda") {
    InlineFunction<S32(S32), 32> fn([](S32 value) { return value + 1; });

    CHECK(!fn.is_nil());
    CHECK(fn(10) == 11);
}

TEST_CASE("InlineFunction - copy") {
    S32 offset = 7;
    InlineFunction<S32(S32), 32> fn([offset](S32 value) { return value + offset; });
    InlineFunction<S32(S32), 32> copy(fn);

    CHECK(copy(5) == 12);
}

TEST_CASE("InlineFunction - move") {
    InlineFunction<S32(S32), 32> fn([](S32 value) { return value * 2; });
    InlineFunction<S32(S32), 32> moved(std::move(fn));

    CHECK(moved(6) == 12);
    CHECK(fn.is_nil());
}

TEST_CASE("InlineFunction - reset and assign") {
    InlineFunction<S32(S32), 32> fn([](S32 value) { return value + 3; });

    CHECK(fn(2) == 5);

    fn.reset();
    CHECK(fn.is_nil());

    fn = [](S32 value) { return value - 4; };
    CHECK(fn(9) == 5);
}

TEST_CASE("InlineFunction - function pointer") {
    auto plus_two = +[](S32 value) { return value + 2; };
    InlineFunction<S32(S32), 32> fn(plus_two);

    CHECK(fn(40) == 42);
}

TEST_CASE("InlineFunction - const invoke") {
    const InlineFunction<S32(S32), 32> fn([](S32 value) { return value + 5; });
    CHECK(fn(2) == 7);
}

} // namespace fr
