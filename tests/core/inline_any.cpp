#include <doctest.h>

#include "fr/core/inline_any.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

struct InlineAnyPayload {
    S32 a{0};
    S32 b{0};
};

TEST_CASE("InlineAny - nil") {
    InlineAny<32, 8> any;

    CHECK(any.is_nil());
}

TEST_CASE("InlineAny - store and cast") {
    InlineAny<32, 8> any(42);

    CHECK(!any.is_nil());
    CHECK(any.cast<S32>() == 42);
}

TEST_CASE("InlineAny - copy") {
    InlineAny<32, 8> any(7);
    InlineAny<32, 8> copy(any);

    CHECK(!copy.is_nil());
    CHECK(copy.cast<S32>() == 7);
}

TEST_CASE("InlineAny - move") {
    InlineAny<32, 8> any(11);
    InlineAny<32, 8> moved(std::move(any));

    CHECK(!moved.is_nil());
    CHECK(moved.cast<S32>() == 11);
}

TEST_CASE("InlineAny - struct payload") {
    InlineAny<32, 8> any(InlineAnyPayload{1, 2});

    CHECK(!any.is_nil());
    CHECK(any.cast<InlineAnyPayload>().a == 1);
    CHECK(any.cast<InlineAnyPayload>().b == 2);
}

} // namespace fr
