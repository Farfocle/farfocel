#include "fr/core/optional.hpp"
#include "doctest.h"
#include "fr/core/typedefs.hpp"

using namespace fr;

TEST_CASE("Optional minimal API") {
    SUBCASE("Empty optional") {
        Optional<S32> opt;
        CHECK(opt.is_nil());
        CHECK(opt.ptr_or_null() == nullptr);
    }

    SUBCASE("Explicit nil construction") {
        Optional<S32> opt = fr::none();
        CHECK(opt.is_nil());
    }

    SUBCASE("Optional with value") {
        Optional<S32> opt(42);
        CHECK(!opt.is_nil());
        CHECK(opt.unwrap() == 42);
    }

    SUBCASE("Optional some() factory") {
        auto opt = some(100);
        CHECK(!opt.is_nil());
        CHECK(opt.unwrap() == 100);
    }

    SUBCASE("Optional reset and emplace") {
        Optional<S32> opt(42);
        opt.reset();
        CHECK(opt.is_nil());

        opt.emplace(10);
        CHECK(opt.unwrap() == 10);
    }

    SUBCASE("Optional unwrap_or") {
        Optional<S32> empty;
        CHECK(empty.unwrap_or(5) == 5);
        Optional<S32> full(10);
        CHECK(full.unwrap_or(5) == 10);
    }
}

TEST_CASE("Optional Niche Optimization") {
    SUBCASE("Pointer niche size") {
        CHECK(sizeof(Optional<S32 *>) == sizeof(S32 *));
    }

    SUBCASE("Pointer nil behavior") {
        S32 val = 5;
        Optional<S32 *> opt(&val);
        CHECK(!opt.is_nil());
        CHECK(*opt.unwrap() == 5);

        opt = fr::none();
        CHECK(opt.is_nil());
        CHECK(opt.ptr_or_null() == nullptr);
    }
}

struct CustomNillable {
    S32 val;

    static constexpr CustomNillable nil() noexcept {
        return CustomNillable{-1};
    }

    constexpr bool is_nil() const noexcept {
        return val == -1;
    }

    bool operator==(const CustomNillable &) const = default;
};

TEST_CASE("Optional with Custom Nillable") {
    CHECK(IsNillable<CustomNillable>);

    Optional<CustomNillable> opt;
    CHECK(opt.is_nil());
    CHECK(sizeof(opt) == sizeof(CustomNillable));

    opt.emplace(42);
    CHECK(!opt.is_nil());
    CHECK(opt->val == 42);

    opt = fr::none();
    CHECK(opt.is_nil());
}

TEST_CASE("Optional monadic map") {
    Optional<S32> opt(10);

    auto result = opt.map([](S32 x) noexcept { return x * 2; });
    CHECK(!result.is_nil());
    CHECK(result.unwrap() == 20);

    Optional<S32> empty;
    auto empty_result = empty.map([](S32 x) noexcept { return x * 2; });
    CHECK(empty_result.is_nil());
}

struct NonTrivial {
    static S32 constructions;
    static S32 destructions;
    S32 val;

    NonTrivial(S32 v) noexcept
        : val(v) {
        constructions++;
    }
    ~NonTrivial() noexcept {
        destructions++;
    }
    NonTrivial(const NonTrivial &o) noexcept
        : val(o.val) {
        constructions++;
    }
    NonTrivial(NonTrivial &&o) noexcept
        : val(o.val) {
        constructions++;
    }
    NonTrivial &operator=(const NonTrivial &) noexcept = default;
    NonTrivial &operator=(NonTrivial &&) noexcept = default;
};

int NonTrivial::constructions = 0;
int NonTrivial::destructions = 0;

TEST_CASE("Optional non-trivial types") {
    NonTrivial::constructions = 0;
    NonTrivial::destructions = 0;

    {
        Optional<NonTrivial> opt(42);
        CHECK(opt->val == 42);
        CHECK(NonTrivial::constructions == 1);
    }
    CHECK(NonTrivial::destructions == 1);
}
