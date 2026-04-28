#include <doctest.h>

#include "fr/core/dynamic_array.hpp"

namespace fr {
static S32 alive_count = 0;

struct Lifecycle {
    S32 value;
    bool *moved_from = nullptr;

    Lifecycle(S32 v = 0)
        : value(v) {
        alive_count++;
    }

    Lifecycle(const Lifecycle &other)
        : value(other.value) {
        alive_count++;
    }

    Lifecycle(Lifecycle &&other) noexcept
        : value(other.value) {
        alive_count++;
        if (other.moved_from)
            *other.moved_from = true;
    }

    Lifecycle &operator=(const Lifecycle &other) {
        value = other.value;
        return *this;
    }

    Lifecycle &operator=(Lifecycle &&other) noexcept {
        value = other.value;
        if (other.moved_from)
            *other.moved_from = true;
        return *this;
    }

    ~Lifecycle() {
        value = -999;
        alive_count--;
    }
};

TEST_CASE("DynamicArray - Remove Shift Lifecycle") {
    alive_count = 0;

    {
        DynamicArray<Lifecycle> arr;
        arr.push_back(Lifecycle(10));
        arr.push_back(Lifecycle(20));
        arr.push_back(Lifecycle(30));

        CHECK(alive_count == 3);
        arr.remove_shift(0); // Should move 20 to 0, 30 to 1
        CHECK(arr.size() == 2);
        CHECK(arr[0].value == 20);
        CHECK(arr[1].value == 30);
        CHECK(alive_count == 2);
    }

    CHECK(alive_count == 0);
}

TEST_CASE("DynamicArray - Construction") {
    SUBCASE("Default") {
        DynamicArray<int> arr;
        CHECK(arr.size() == 0);
        CHECK(arr.capacity() == 0);
        CHECK(arr.is_empty());
    }

    SUBCASE("With capacity") {
        auto arr = DynamicArray<int>::with_capacity(10);
        CHECK(arr.size() == 0);
        CHECK(arr.capacity() == 10);
    }

    SUBCASE("With size") {
        auto arr = DynamicArray<int>::with_size(5);
        CHECK(arr.size() == 5);
        CHECK(arr.capacity() >= 5);
    }

    SUBCASE("Filled with") {
        auto arr = DynamicArray<int>::filled_with(3, 42, globals::get_default_allocator());
        CHECK(arr.size() == 3);
        CHECK(arr[0] == 42);
        CHECK(arr[1] == 42);
        CHECK(arr[2] == 42);
    }

    SUBCASE("Initializer list") {
        DynamicArray<int> arr = {1, 2, 3, 4, 5};
        CHECK(arr.size() == 5);
        CHECK(arr[0] == 1);
        CHECK(arr[4] == 5);
    }
}

TEST_CASE("DynamicArray - Lifecycle Management") {
    alive_count = 0;

    {
        DynamicArray<Lifecycle> arr;
        arr.push_back(Lifecycle(1));
        arr.push_back(Lifecycle(2));
        CHECK(alive_count == 2);

        arr.pop_back();
        CHECK(alive_count == 1);

        arr.clear();
        CHECK(alive_count == 0);
    }

    CHECK(alive_count == 0);
}

TEST_CASE("DynamicArray - Modifiers") {
    DynamicArray<int> arr = {1, 2, 3};

    SUBCASE("Push and Pop") {
        arr.push_back(4);
        CHECK(arr.size() == 4);
        CHECK(arr.back() == 4);

        arr.pop_back();
        CHECK(arr.size() == 3);
        CHECK(arr.back() == 3);
    }

    SUBCASE("Remove Swap (Unordered)") {
        arr.remove_swap(0); // Removes 1, swaps with 3
        CHECK(arr.size() == 2);
        CHECK(arr[0] == 3);
        CHECK(arr[1] == 2);
    }

    SUBCASE("Remove Shift (Ordered)") {
        arr.remove_shift(0); // Removes 1, shifts 2, 3
        CHECK(arr.size() == 2);
        CHECK(arr[0] == 2);
        CHECK(arr[1] == 3);
    }

    SUBCASE("Insert") {
        arr.insert(1, 42); // {1, 42, 2, 3}
        CHECK(arr.size() == 4);
        CHECK(arr[1] == 42);
        CHECK(arr[2] == 2);
    }
}

TEST_CASE("DynamicArray - Copy and Move") {
    DynamicArray<int> original = {1, 2, 3};

    SUBCASE("Copy constructor") {
        DynamicArray<int> copy = original;
        CHECK(copy.size() == 3);
        CHECK(copy[0] == 1);
        copy[0] = 42;
        CHECK(original[0] == 1);
    }

    SUBCASE("Move constructor") {
        DynamicArray<int> moved = std::move(original);
        CHECK(moved.size() == 3);
        CHECK(original.size() == 0);
    }

    SUBCASE("Copy assignment") {
        DynamicArray<int> copy;
        copy = original;
        CHECK(copy.size() == 3);
        CHECK(copy[2] == 3);
    }

    SUBCASE("Move assignment") {
        DynamicArray<int> moved;
        moved = std::move(original);
        CHECK(moved.size() == 3);
        CHECK(original.size() == 0);
    }
}

TEST_CASE("DynamicArray - Slices") {
    DynamicArray<int> arr = {0, 1, 2, 3, 4};

    SUBCASE("Full slice") {
        auto s = arr.slice();
        CHECK(s.size() == 5);
        CHECK(s[0] == 0);
    }

    SUBCASE("Sub slice") {
        auto s = arr.slice(1, 3); // {1, 2, 3}
        CHECK(s.size() == 3);
        CHECK(s[0] == 1);
        CHECK(s[2] == 3);
    }
}

} // namespace fr
