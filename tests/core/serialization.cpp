#include <doctest.h>

#include "fr/core/dynamic_array.hpp"
#include "fr/core/hash.hpp"
#include "fr/core/hash_map.hpp"
#include "fr/core/hash_set.hpp"
#include "fr/core/json.hpp"
#include "fr/core/optional.hpp"
#include "fr/core/pair.hpp"
#include "fr/core/string.hpp"
#include "fr/core/string_view.hpp"
#include "fr/core/tuple.hpp"

namespace fr {

TEST_CASE("Serialization - Pair") {
    Pair<S32, String> value(7, String("seven"));

    JsonSerializer writer;
    writer.prop("pair", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"first\":7") != StringView::npos);
    CHECK(json.view().find("\"second\":\"seven\"") != StringView::npos);
}

TEST_CASE("Serialization - Tuple") {
    Tuple<S32, bool, String> value(4, true, String("ok"));

    JsonSerializer writer;
    writer.prop("tuple", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"items\"") != StringView::npos);
    CHECK(json.view().find("\"ok\"") != StringView::npos);
    CHECK(json.view().find("\"size\"") != StringView::npos);
}

TEST_CASE("Serialization - DynamicArray") {
    DynamicArray<S32> value = {1, 2, 3};

    JsonSerializer writer;
    writer.prop("array", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"size\":3") != StringView::npos);
    CHECK(json.view().find("\"items\":[1,2,3]") != StringView::npos);
}

TEST_CASE("Serialization - HashSet") {
    HashSet<S32> value;
    value.insert(10);
    value.insert(20);

    JsonSerializer writer;
    writer.prop("set", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"load\":2") != StringView::npos);
    CHECK(json.view().find("\"items\":[") != StringView::npos);
    CHECK(json.view().find("10") != StringView::npos);
    CHECK(json.view().find("20") != StringView::npos);
}

TEST_CASE("Serialization - HashMap") {
    HashMap<S32, String> value;
    value.insert(1, String("one"));
    value.insert(2, String("two"));

    JsonSerializer writer;
    writer.prop("map", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"load\":2") != StringView::npos);
    CHECK(json.view().find("\"items\":[") != StringView::npos);
    CHECK(json.view().find("\"@key\":1") != StringView::npos);
    CHECK(json.view().find("\"@value\":\"one\"") != StringView::npos);
}

TEST_CASE("Serialization - Optional") {
    Optional<S32> some_value(42);
    JsonSerializer writer;

    writer.prop("opt", some_value);
    auto json = writer.consume();

    CHECK(json.view().find("\"has_value\":true") != StringView::npos);
    CHECK(json.view().find("\"value\":42") != StringView::npos);
}

TEST_CASE("Serialization - Hash and Hash32") {
    Hash h = Hash::from_raw(123);
    Hash32 h32 = Hash32::from_raw(77);

    JsonSerializer writer;
    writer.prop("h", h);
    writer.prop("h32", h32);
    auto json = writer.consume();

    CHECK(json.view().find("\"h\"") != StringView::npos);
    CHECK(json.view().find("\"h32\"") != StringView::npos);
    CHECK(json.view().find("\"value\":123") != StringView::npos);
    CHECK(json.view().find("\"value\":77") != StringView::npos);
}

} // namespace fr
