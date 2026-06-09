#include <doctest.h>

#include "fr/core/algo.hpp"
#include "fr/core/array.hpp"
#include "fr/core/dynamic_array.hpp"
#include "fr/core/typedefs.hpp"

namespace fr {

TEST_CASE("Radix Sort - Unsigned integral keys") {
    DynamicArray<U32> keys;
    keys.push_back(4);
    keys.push_back(1);
    keys.push_back(3);
    keys.push_back(2);
    keys.push_back(0);
    keys.push_back(9);

    radix_sort(keys.slice_mut());

    CHECK(keys.size() == 6);
    CHECK(keys[0] == 0);
    CHECK(keys[1] == 1);
    CHECK(keys[2] == 2);
    CHECK(keys[3] == 3);
    CHECK(keys[4] == 4);
    CHECK(keys[5] == 9);
}

TEST_CASE("Radix Sort - Signed integral keys") {
    DynamicArray<S32> keys;
    keys.push_back(0);
    keys.push_back(-1);
    keys.push_back(5);
    keys.push_back(-10);
    keys.push_back(2);

    radix_sort(keys.slice_mut());

    CHECK(keys.size() == 5);
    CHECK(keys[0] == -10);
    CHECK(keys[1] == -1);
    CHECK(keys[2] == 0);
    CHECK(keys[3] == 2);
    CHECK(keys[4] == 5);
}

TEST_CASE("Radix Sort - Byte keys") {
    DynamicArray<Array<U8, 2>> keys;
    keys.push_back(Array<U8, 2>{1, 0});
    keys.push_back(Array<U8, 2>{0, 1});
    keys.push_back(Array<U8, 2>{0, 0});
    keys.push_back(Array<U8, 2>{2, 0});

    radix_sort_raw(keys.slice_mut());

    CHECK(keys.size() == 4);
    CHECK(keys[0][0] == 0);
    CHECK(keys[0][1] == 0);
    CHECK(keys[1][0] == 1);
    CHECK(keys[1][1] == 0);
    CHECK(keys[2][0] == 2);
    CHECK(keys[2][1] == 0);
    CHECK(keys[3][0] == 0);
    CHECK(keys[3][1] == 1);
}

TEST_CASE("Radix Sort - By key") {
    struct Item {
        S32 key;
        S32 id;
    };

    DynamicArray<Item> items;
    items.push_back(Item{2, 20});
    items.push_back(Item{1, 10});
    items.push_back(Item{2, 21});
    items.push_back(Item{0, 5});

    radix_sort_key(items.slice_mut(), [](const Item &item) { return item.key; });

    CHECK(items.size() == 4);
    CHECK(items[0].key == 0);
    CHECK(items[0].id == 5);
    CHECK(items[1].key == 1);
    CHECK(items[1].id == 10);
    CHECK(items[2].key == 2);
    CHECK(items[2].id == 20);
    CHECK(items[3].key == 2);
    CHECK(items[3].id == 21);
}

} // namespace fr
