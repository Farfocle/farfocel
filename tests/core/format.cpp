#include <doctest.h>

#include "fr/core/dynamic_array.hpp"
#include "fr/core/format.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

TEST_CASE("format - primitive_to_string") {
    SUBCASE("Integers") {
        CHECK(primitive_to_string(U32(123)) == "123");
        CHECK(primitive_to_string(S32(-456)) == "-456");
        CHECK(primitive_to_string(U64(0)) == "0");
        CHECK(primitive_to_string(USize(999999)) == "999999");
    }

    SUBCASE("Floats") {
        CHECK(primitive_to_string(F32(3.14f)).starts_with("3.14"));
    }

    SUBCASE("Boolean") {
        CHECK(primitive_to_string(true) == "true");
        CHECK(primitive_to_string(false) == "false");
    }

    SUBCASE("Char") {
        CHECK(primitive_to_string('A') == "A");
    }

    SUBCASE("Byte") {
        CHECK(primitive_to_string(Byte(10)) == "10");
    }
}

TEST_CASE("format - trim_float_string") {
    String s1("3.14000");
    impl::trim_float_string(s1);
    CHECK(s1 == "3.14");

    String s2("3.000");
    impl::trim_float_string(s2);
    CHECK(s2 == "3");

    String s3("123");
    impl::trim_float_string(s3);
    CHECK(s3 == "123");
}

TEST_CASE("format - format basic") {
    CHECK(format("Hello {}", "World") == "Hello World");
    CHECK(format("{} + {} = {}", 1, 2, 3) == "1 + 2 = 3");
    CHECK(format("Bool: {}, Char: {}", true, 'Z') == "Bool: true, Char: Z");
}

TEST_CASE("format - format with options") {
    FormatOptions opts;
    opts.float_precision = 2;
    CHECK(format_with_options(opts, "Val: {}", 3.14159) == "Val: 3.14");

    opts.float_precision = 4;
    CHECK(format_with_options(opts, "Val: {}", 3.1) == "Val: 3.1000");
}

TEST_CASE("format - format serialization") {
    DynamicArray<S32> arr = {1, 2, 3};
    String result = format("Array: {}", arr);

    // Result contains the serialized JSON
    CHECK(result.contains("\"@size\":3"));
    CHECK(result.contains("\"@items\":[1,2,3]"));
}

} // namespace fr
