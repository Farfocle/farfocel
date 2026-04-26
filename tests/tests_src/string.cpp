#include "fr/core/string.hpp"
#include "doctest.h"
#include "fr/core/string_view.hpp"

namespace fr {

TEST_CASE("StringView - Basic Operations") {
    StringView sv("Farfocel Engine");

    CHECK(sv.size() == 15);
    CHECK(!sv.is_empty());
    CHECK(sv.front() == 'F');
    CHECK(sv.back() == 'e');
    CHECK(sv[9] == 'E');

    StringView sub1 = sv.substr(0, 8);
    CHECK(sub1.size() == 8);
    CHECK(sub1 == "Farfocel");

    StringView sub2 = sv.substr(9);
    CHECK(sub2 == "Engine");

    CHECK(sv.starts_with("Farfocel"));
    CHECK(sv.ends_with("Engine"));
    CHECK(!sv.starts_with("Engine"));

    CHECK(sv.find('c') == 5);
    CHECK(sv.find("Engine") == 9);
    CHECK(sv.find("X") == StringView::npos);

    sv.remove_prefix(9);
    CHECK(sv == "Engine");

    sv.remove_suffix(4);
    CHECK(sv == "En");
}

TEST_CASE("String - SSO max") {
    String empty;
    CHECK(empty.size() == 0);
    CHECK(empty.capacity() == 23);
    CHECK(empty.is_short_string());

    String sso_str("12345678901234567890123");
    CHECK(sso_str.size() == 23);
    CHECK(sso_str.capacity() == 23);
    CHECK(sso_str.is_short_string());

    String heap_str("123456789012345678901234");
    CHECK(heap_str.size() == 24);
    CHECK(heap_str.capacity() >= 24);
    CHECK(!heap_str.is_short_string());

    String copied_heap(heap_str);
    CHECK(copied_heap.size() == 24);
    CHECK(!copied_heap.is_short_string());
    CHECK(copied_heap == heap_str);

    String moved_heap(std::move(copied_heap));
    CHECK(moved_heap.size() == 24);
    CHECK(!moved_heap.is_short_string());
    CHECK(copied_heap.is_short_string());
    CHECK(copied_heap.size() == 0);

    String dynamic_str("Short");
    CHECK(dynamic_str.is_short_string());

    dynamic_str.append(" STRING that defaldkjfalsdjflsadfj ljsadlfkajsdl kjfaslkdjf lsjdaf lksajdf "
                       "olkjfo wejf oiwjef oijweo fjwao djifasdlf jasldkf jasldkfj");
    CHECK(dynamic_str.size() == 51);
    CHECK(!dynamic_str.is_short_string());

    dynamic_str.erase(5);
    CHECK(dynamic_str.size() == 5);
    CHECK(!dynamic_str.is_short_string());

    dynamic_str.shrink_to_fit();
    CHECK(dynamic_str.size() == 5);
    CHECK(dynamic_str.is_short_string());
}

TEST_CASE("String - Modification") {
    String str("Engine");

    str.push_back('X');
    CHECK(str == "EngineX");

    str.pop_back();
    CHECK(str == "Engine");

    str.append(" Farfocel");
    CHECK(str == "Engine Farfocel");

    str.insert(6, " 3D");
    CHECK(str == "Engine 3D Farfocel");

    str.insert(0, 3, 'A');
    CHECK(str == "AAAEngine 3D Farfocel");

    str.erase(0, 3);
    CHECK(str == "Engine 3D Farfocel");

    str.replace(7, "2D", 2);
    CHECK(str == "Engine 2D Farfocel");

    str.clear();
    CHECK(str.size() == 0);
    CHECK(str.capacity() >= 0);
}

TEST_CASE("String - Search") {
    String str("The quick brown fox jumps over the lazy dog");

    CHECK(str.find("brown") == 10);
    CHECK(str.find('q') == 4);
    CHECK(str.find("cat") == String::npos);

    CHECK(str.reverse_find("the") == 31);
    CHECK(str.reverse_find("The") == 0);

    CHECK(str.find_first_of("aeiou") == 2);
    CHECK(str.find_first_not_of("The ") == 4);

    CHECK(str.find_last_of("aeiou") == 37);
    CHECK(str.find_last_not_of("god ") == 39);
}

TEST_CASE("String - Utilities") {
    String whitespace_str("   \t\n  Trim me  \n\r   ");
    whitespace_str.trim();
    CHECK(whitespace_str == "Trim me");

    String lower("FARFOCEL engine 123");
    lower.to_lower_ascii();
    CHECK(lower == "farfocel engine 123");

    String upper("farfocel engine 123");
    upper.to_upper_ascii();
    CHECK(upper == "FARFOCEL ENGINE 123");

    CHECK(upper.contains("ENGINE"));
    CHECK(!upper.contains("engine"));

    String s1 = "Alpha";
    String s2 = "Beta";
    String s3 = s1 + s2;
    CHECK(s3 == "AlphaBeta");
}

TEST_CASE("String - Iterators and element accesss") {
    String str("Data");

    CHECK(str.front() == 'D');
    CHECK(str.back() == 'a');

    str[0] = 'B';
    CHECK(str == "Bata");

    USize count = 0;
    for ([[maybe_unused]] char c : str) {
        count++;
    }
    CHECK(count == 4);

    char *it = str.begin();
    CHECK(*it == 'B');
    *(it + 1) = 'e';
    CHECK(str == "Beta");
}

} // namespace fr
