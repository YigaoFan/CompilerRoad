module;
#include <catch2/catch_all.hpp>
export module StringTest;
import std;
import Base;

using std::string;
using std::string_view;

TEST_CASE("String - Default constructor", "[string]")
{
    String s;
    REQUIRE(s.Empty());
    REQUIRE(s.Length() == 0);
}

TEST_CASE("String - EmptyString factory", "[string]")
{
    auto s = String::EmptyString();
    REQUIRE(s.Empty());
    REQUIRE(s.Length() == 0);
    REQUIRE(s == "");
}

TEST_CASE("String - String literal constructor", "[string]")
{
    String s("hello");
    REQUIRE_FALSE(s.Empty());
    REQUIRE(s.Length() == 5);
    REQUIRE(s == "hello");
}

TEST_CASE("String - Single char constructor", "[string]")
{
    String s('x');
    REQUIRE_FALSE(s.Empty());
    REQUIRE(s.Length() == 1);
    REQUIRE(s == "x");
}

TEST_CASE("String - string_view constructor", "[string]")
{
    string_view sv = "world";
    String s(sv);
    REQUIRE(s.Length() == 5);
    REQUIRE(s == "world");
}

TEST_CASE("String - Copy constructor", "[string]")
{
    String a("hello");
    String b(a);
    REQUIRE(b == "hello");
    REQUIRE(b.Length() == 5);
    // original still valid
    REQUIRE(a == "hello");
}

TEST_CASE("String - Move constructor", "[string]")
{
    String a("hello");
    String b(std::move(a));
    REQUIRE(b == "hello");
    REQUIRE(a.Empty());
}

TEST_CASE("String - Copy assignment", "[string]")
{
    String a("hello");
    String b("world");
    b = a;
    REQUIRE(b == "hello");
    REQUIRE(a == "hello");
}

TEST_CASE("String - Move assignment", "[string]")
{
    String a("hello");
    String b("world");
    b = std::move(a);
    REQUIRE(b == "hello");
    REQUIRE(a.Empty());
}

TEST_CASE("String - operator== with String", "[string]")
{
    String a("hello");
    String b("hello");
    String c("world");
    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
}

TEST_CASE("String - operator== with char const*", "[string]")
{
    String s("hello");
    REQUIRE(s == "hello");
    REQUIRE_FALSE(s == "world");
    REQUIRE_FALSE(s == "hell");
}

TEST_CASE("String - operator== with string_view", "[string]")
{
    String s("hello");
    REQUIRE(s == string_view("hello"));
    REQUIRE_FALSE(s == string_view("world"));
}

TEST_CASE("String - operator< and <=>", "[string]")
{
    String a("abc");
    String b("abd");
    String c("abc");
    REQUIRE(a < b);
    REQUIRE_FALSE(b < a);
    REQUIRE_FALSE(a < c);
    REQUIRE((a <=> b) == std::strong_ordering::less);
    REQUIRE((a <=> c) == std::strong_ordering::equal);
    REQUIRE((b <=> a) == std::strong_ordering::greater);
}

TEST_CASE("String - operator+ with char", "[string]")
{
    String s("hel");
    String result = s + 'l';
    REQUIRE(result == "hell");
    REQUIRE(s == "hel"); // original unchanged
}

TEST_CASE("String - operator+ with String", "[string]")
{
    String a("hello");
    String b(" world");
    String result = a + b;
    REQUIRE(result == "hello world");
}

TEST_CASE("String - operator+ with string literal", "[string]")
{
    String s("hello");
    String result = s + " world";
    REQUIRE(result == "hello world");
}

TEST_CASE("String - operator+ with string_view", "[string]")
{
    String s("hello");
    String result = s + string_view(" world");
    REQUIRE(result == "hello world");
}

TEST_CASE("String - operator[]", "[string]")
{
    String s("hello");
    REQUIRE(s[0] == 'h');
    REQUIRE(s[1] == 'e');
    REQUIRE(s[4] == 'o');
}

TEST_CASE("String - Substring from start", "[string]")
{
    String s("hello world");
    auto sub = s.Substring(6);
    REQUIRE(sub == "world");
    REQUIRE(sub.Length() == 5);
}

TEST_CASE("String - Substring with length", "[string]")
{
    String s("hello world");
    auto sub = s.Substring(0, 5);
    REQUIRE(sub == "hello");
    REQUIRE(sub.Length() == 5);
}

TEST_CASE("String - Substring middle", "[string]")
{
    String s("hello world");
    auto sub = s.Substring(3, 4);
    REQUIRE(sub == "lo w");
}

TEST_CASE("String - Substring shares buffer", "[string]")
{
    String s("hello world");
    auto sub = s.Substring(6);
    // Both should still work after sharing
    REQUIRE(s == "hello world");
    REQUIRE(sub == "world");
}

TEST_CASE("String - Substring clamping", "[string]")
{
    String s("hello");
    // start beyond length => empty
    auto sub = s.Substring(10);
    REQUIRE(sub.Empty());

    // length beyond remaining => clamp
    auto sub2 = s.Substring(2, 100);
    REQUIRE(sub2 == "llo");
}

TEST_CASE("String - Substring of substring", "[string]")
{
    String s("hello world");
    auto sub1 = s.Substring(6);       // "world"
    auto sub2 = sub1.Substring(1, 3); // "orl"
    REQUIRE(sub2 == "orl");
}

TEST_CASE("String - Contains char", "[string]")
{
    String s("hello");
    REQUIRE(s.Contains('h'));
    REQUIRE(s.Contains('o'));
    REQUIRE_FALSE(s.Contains('x'));
}

TEST_CASE("String - Contains String", "[string]")
{
    String s("hello world");
    REQUIRE(s.Contains(String("world")));
    REQUIRE(s.Contains(String("hello")));
    REQUIRE(s.Contains(String("lo wo")));
    REQUIRE_FALSE(s.Contains(String("xyz")));
}

TEST_CASE("String - Contains string_view", "[string]")
{
    String s("hello world");
    REQUIRE(s.Contains(string_view("world")));
    REQUIRE_FALSE(s.Contains(string_view("xyz")));
}

TEST_CASE("String - Contains string literal", "[string]")
{
    String s("hello world");
    REQUIRE(s.Contains("world"));
    REQUIRE_FALSE(s.Contains("xyz"));
}

TEST_CASE("String - Contains substring result", "[string]")
{
    String s("hello world");
    auto sub = s.Substring(6); // "world"
    REQUIRE(s.Contains(sub));
}

TEST_CASE("String - StartWith", "[string]")
{
    String s("hello world");
    REQUIRE(s.StartWith(String("hello")));
    REQUIRE(s.StartWith("hell"));
    REQUIRE(s.StartWith("hello"));
    REQUIRE_FALSE(s.StartWith(String("world")));
    REQUIRE_FALSE(s.StartWith("world"));
}

TEST_CASE("String - EndWith", "[string]")
{
    String s("hello world");
    REQUIRE(s.EndWith(String("world")));
    REQUIRE(s.EndWith("rld"));
    REQUIRE(s.EndWith("world"));
    REQUIRE_FALSE(s.EndWith(String("hello")));
    REQUIRE_FALSE(s.EndWith("hello"));
}

TEST_CASE("String - Empty", "[string]")
{
    String s1;
    REQUIRE(s1.Empty());

    String s2("");
    REQUIRE(s2.Empty());

    String s3("a");
    REQUIRE_FALSE(s3.Empty());

    String s4(String("hello").Substring(3, 0));
    REQUIRE(s4.Empty());
}

TEST_CASE("String - Length", "[string]")
{
    String s1("hello");
    REQUIRE(s1.Length() == 5);

    String s2;
    REQUIRE(s2.Length() == 0);

    String s3("a");
    REQUIRE(s3.Length() == 1);
}

TEST_CASE("String - Iterators", "[string]")
{
    String s("hello");
    auto it = s.begin();
    REQUIRE(*it == 'h');
    ++it;
    REQUIRE(*it == 'e');

    // range-based for
    string result;
    for (char c : s)
    {
        result += c;
    }
    REQUIRE(result == "hello");
}

TEST_CASE("String - Conversion to string_view", "[string]")
{
    String s("hello");
    string_view sv = s;
    REQUIRE(sv == "hello");
}

TEST_CASE("String - Conversion to string", "[string]")
{
    String s("hello");
    string std_str = static_cast<string>(s);
    REQUIRE(std_str == "hello");
}

TEST_CASE("String - operator<<", "[string]")
{
    String s("hello");
    std::ostringstream oss;
    oss << s;
    REQUIRE(oss.str() == "hello");
}

TEST_CASE("String - std::format via string_view", "[string]")
{
    String s("hello");
    auto result = std::format("{}", static_cast<string_view>(s));
    REQUIRE(result == "hello");
}

TEST_CASE("String - Substring then compare", "[string]")
{
    String s("hello world");
    auto hello = s.Substring(0, 5);
    auto world = s.Substring(6);
    REQUIRE(hello == "hello");
    REQUIRE(world == "world");
    REQUIRE_FALSE(hello == world);
}

TEST_CASE("String - Concatenation chain", "[string]")
{
    String result = String("a") + "b" + String("c") + 'd';
    REQUIRE(result == "abcd");
    REQUIRE(result.Length() == 4);
}

TEST_CASE("String - Self assignment copy", "[string]")
{
    String s("hello");
    s = s;
    REQUIRE(s == "hello");
}

TEST_CASE("String - Refcounted copy survives original destruction", "[string]")
{
    String copy;
    {
        String original("hello");
        copy = original;
    }
    // original is destroyed, copy should still be valid
    REQUIRE(copy == "hello");
    REQUIRE(copy.Length() == 5);
}

TEST_CASE("String - std::less transparent comparator", "[string]")
{
    std::less<String> comp;
    String a("abc");
    String b("def");
    REQUIRE(comp(a, b));
    REQUIRE_FALSE(comp(b, a));

    // heterogeneous lookup
    REQUIRE(comp(a, string_view("def")));
    REQUIRE(comp(string_view("abc"), b));
}

// ==================== Memory Leak Tests ====================
// These test specific memory management patterns.
// The EventListener in Main.cpp also checks for leaks after EVERY test case.

TEST_CASE("String - Memory: temporary literal destroyed", "[string][memory]")
{
    // Temporary from string literal (non-releasable) goes out of scope
    String s = String("hello") + " world";
    REQUIRE(s == "hello world");
}

TEST_CASE("String - Memory: string_view constructed destroyed", "[string][memory]")
{
    // string_view constructor allocates (releasable)
    string_view sv = "test string";
    String s(sv);
    REQUIRE(s == "test string");
}

TEST_CASE("String - Memory: substring shared buffer lifetime", "[string][memory]")
{
    String copy;
    {
        String original("hello world");
        auto sub = original.Substring(6); // "world", shares buffer
        copy = sub;
    }
    // original and sub destroyed, but shared buffer survives via copy
    REQUIRE(copy == "world");
}

TEST_CASE("String - Memory: reassignment frees old buffer", "[string][memory]")
{
    String s("first");
    s = String("second");
    REQUIRE(s == "second");
    s = String("third");
    REQUIRE(s == "third");
}

TEST_CASE("String - Memory: move from string", "[string][memory]")
{
    String a("hello");
    String b(std::move(a));
    REQUIRE(b == "hello");
    // a is now empty, should not double-free
}

TEST_CASE("String - Memory: concatenation intermediates", "[string][memory]")
{
    // Each + creates a new buffer; intermediates should be freed
    String result;
    for (int i = 0; i < 100; ++i)
    {
        result = result + "x";
    }
    REQUIRE(result.Length() == 100);
}

TEST_CASE("String - Memory: char constructor and assignment", "[string][memory]")
{
    String s('a');
    REQUIRE(s == "a");
    s = String('b');
    REQUIRE(s == "b");
}

TEST_CASE("String - Memory: multiple substrings of same buffer", "[string][memory]")
{
    String original("abcdefghij");
    auto s1 = original.Substring(0, 3);  // "abc"
    auto s2 = original.Substring(3, 4);  // "defg"
    auto s3 = original.Substring(7, 3);  // "hij"
    REQUIRE(s1 == "abc");
    REQUIRE(s2 == "defg");
    REQUIRE(s3 == "hij");
    // all three share the same underlying buffer
}

TEST_CASE("String - Memory: self move assignment", "[string][memory]")
{
    String s("hello");
    String& ref = s;
    s = std::move(ref);
    // Should not crash or leak
}
