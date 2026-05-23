#include <doctest.h>

#include "fr/core/bitset.hpp"
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
#include "fr/core/unique_ptr.hpp"

namespace fr {

struct Data {
    DynamicArray<S32> arr;
    HashSet<S32> set;
    HashMap<S32, String> map;
    Optional<S32> opt_some;
    Optional<S32> opt_none;
    UniquePtr<S32> ptr;
    Pair<S32, String> pair;

    void shape(auto &a) {
        a.prop("arr", arr);
        a.prop("set", set);
        a.prop("map", map);
        a.prop("opt_some", opt_some);
        a.prop("opt_none", opt_none);
        a.prop("ptr", ptr);
        a.prop("pair", pair);
    }
};

TEST_CASE("Serialization - Pair") {
    Pair<S32, String> value(7, String("seven"));

    JsonWriterArchive writer;
    writer.prop("pair", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"@first\":7") != StringView::npos);
    CHECK(json.view().find("\"@second\":\"seven\"") != StringView::npos);
}

TEST_CASE("Serialization - Tuple") {
    Tuple<S32, bool, String> value(4, true, String("ok"));

    JsonWriterArchive writer;
    writer.prop("tuple", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"@items\"") != StringView::npos);
    CHECK(json.view().find("\"ok\"") != StringView::npos);
    CHECK(json.view().find("\"@size\"") != StringView::npos);
}

TEST_CASE("Serialization - DynamicArray") {
    DynamicArray<S32> value = {1, 2, 3};

    JsonWriterArchive writer;
    writer.prop("array", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"@size\":3") != StringView::npos);
    CHECK(json.view().find("\"@items\":[1,2,3]") != StringView::npos);
}

TEST_CASE("Serialization - HashSet") {
    HashSet<S32> value;
    value.insert(10);
    value.insert(20);

    JsonWriterArchive writer;
    writer.prop("set", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"@load\":2") != StringView::npos);
    CHECK(json.view().find("\"@items\":[") != StringView::npos);
    CHECK(json.view().find("10") != StringView::npos);
    CHECK(json.view().find("20") != StringView::npos);
}

TEST_CASE("Serialization - HashMap") {
    HashMap<S32, String> value;
    value.insert(1, String("one"));
    value.insert(2, String("two"));

    JsonWriterArchive writer;
    writer.prop("map", value);
    auto json = writer.consume();

    CHECK(json.view().find("\"@load\":2") != StringView::npos);
    CHECK(json.view().find("\"@items\":[") != StringView::npos);
    CHECK(json.view().find("\"@key\":1") != StringView::npos);
    CHECK(json.view().find("\"@value\":\"one\"") != StringView::npos);
}

TEST_CASE("Serialization - Optional") {
    Optional<S32> some_value(42);
    JsonWriterArchive writer;

    writer.prop("opt", some_value);
    auto json = writer.consume();

    CHECK(json.view().find("\"@has_value\":true") != StringView::npos);
    CHECK(json.view().find("\"@value\":42") != StringView::npos);
}

TEST_CASE("Serialization - Allocator Types") {
    OwnershipResult result = OwnershipResult::Owns;
    AllocAction action = AllocAction::Reallocate;
    AllocFrame frame{
        .timestamp = 1000,
        .action = AllocAction::Allocate,
        .prev_pointer = nullptr,
        .next_pointer = reinterpret_cast<void *>(0x1234),
        .prev_size = 0,
        .next_size = 64,
        .alignment = 8,
        .tag = "TestTag",
        .success = true,
        .attempt = 1,
    };

    JsonWriterArchive writer;
    writer.prop("result", result);
    writer.prop("action", action);
    writer.prop("frame", frame);
    auto json = writer.consume();

    CHECK(json.view().find("\"result\":{\"@value\":\"owns\"}") != StringView::npos);
    CHECK(json.view().find("\"action\":{\"@value\":\"reallocate\"}") != StringView::npos);
    CHECK(json.view().find("\"timestamp\":1000") != StringView::npos);
    CHECK(json.view().find("\"tag\":\"TestTag\"") != StringView::npos);
    CHECK(json.view().find("\"next_pointer\":4660") != StringView::npos); // 0x1234 = 4660
}

TEST_CASE("Serialization - Slice") {
    S32 data[] = {10, 20, 30};
    Slice<S32> slice(data, 3);

    JsonWriterArchive writer;
    writer.prop("slice", slice);
    auto json = writer.consume();

    CHECK(json.view().find("\"@size\":3") != StringView::npos);
    CHECK(json.view().find("\"@items\":[10,20,30]") != StringView::npos);
}

TEST_CASE("Serialization - UniquePtr") {
    SUBCASE("Single object") {
        auto ptr = fr::make_unique<S32>(123);
        JsonWriterArchive writer;
        writer.prop("ptr", ptr);
        auto json = writer.consume();

        CHECK(json.view().find("\"@has_value\":true") != StringView::npos);
        CHECK(json.view().find("\"@value\":123") != StringView::npos);
    }

    SUBCASE("Array") {
        auto ptr = fr::make_unique<S32[]>(2);
        ptr[0] = 1;
        ptr[1] = 2;

        JsonWriterArchive writer;
        writer.prop("ptr_arr", ptr);
        auto json = writer.consume();

        CHECK(json.view().find("\"@size\":2") != StringView::npos);
        CHECK(json.view().find("\"@items\":[1,2]") != StringView::npos);
    }
}

TEST_CASE("Serialization - Hash and Hash32") {
    Hash h = Hash::from_raw(123);
    Hash32 h32 = Hash32::from_raw(77);

    JsonWriterArchive writer;
    writer.prop("h", h);
    writer.prop("h32", h32);
    auto json = writer.consume();

    CHECK(json.view().find("\"h\"") != StringView::npos);
    CHECK(json.view().find("\"h32\"") != StringView::npos);
    CHECK(json.view().find("\"@value\":123") != StringView::npos);
    CHECK(json.view().find("\"@value\":77") != StringView::npos);
}

TEST_CASE("Deserialization - Basic Types") {
    String json = String::from_view("{\"b\":true,\"u\":42,\"s\":-10,\"f\":3.14,\"str\":\"hello\"}");
    JsonReaderArchive reader(json.view());

    bool b = false;
    U32 u = 0;
    S32 s = 0;
    F32 f = 0.0f;
    String str;

    reader.prop("b", b);
    reader.prop("u", u);
    reader.prop("s", s);
    reader.prop("f", f);
    reader.prop("str", str);

    CHECK(b == true);
    CHECK(u == 42);
    CHECK(s == -10);
    CHECK(f == doctest::Approx(3.14f));
    CHECK(str == "hello");
}

TEST_CASE("Deserialization - Complex Structure") {
    Data original;
    original.arr = {10, 20};
    original.set.insert(30);
    original.map.insert(1, String("one"));
    original.opt_some = 42;
    original.opt_none = none();
    original.ptr = make_unique<S32>(100);
    original.pair = Pair<S32, String>(7, String("seven"));

    JsonWriterArchive writer;
    writer.prop("data", original);
    String json = writer.consume();

    Data deserialized;
    JsonReaderArchive reader(json.view());
    reader.prop("data", deserialized);

    CHECK(deserialized.arr.size() == 2);
    CHECK(deserialized.arr[0] == 10);
    CHECK(deserialized.arr[1] == 20);
    CHECK(deserialized.set.contains(30));
    CHECK(deserialized.map.contains(1));
    CHECK(*deserialized.map.find(1).unwrap() == "one");
    CHECK(deserialized.opt_some.is_some());
    CHECK(deserialized.opt_some.unwrap() == 42);
    CHECK(deserialized.opt_none.is_none());
    CHECK(deserialized.ptr);
    CHECK(*deserialized.ptr == 100);
    CHECK(deserialized.pair.first() == 7);
    CHECK(deserialized.pair.second() == "seven");
}

TEST_CASE("Serialization - Bitset") {
    Bitset<10> original;
    original.zero_all();
    original.one_bit(0);
    original.one_bit(3);
    original.one_bit(9);

    JsonWriterArchive writer;
    writer.prop("bitset", original);
    String json = writer.consume();

    CHECK(json.view().find("\"@size\":10") != StringView::npos);
    CHECK(json.view().find("\"@value\":\"1001000001\"") != StringView::npos);

    Bitset<10> deserialized;
    JsonReaderArchive reader(json.view());
    reader.prop("bitset", deserialized);

    CHECK(deserialized.count_ones() == 3);
    CHECK(deserialized.check_bit(0));
    CHECK(deserialized.check_bit(3));
    CHECK(deserialized.check_bit(9));
    CHECK(!deserialized.check_bit(1));
}

} // namespace fr
