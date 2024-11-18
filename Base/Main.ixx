import std;
import Base;

using namespace std;

int main()
{
    auto s = String("Hello World");
    cout << s << endl;
    auto newS0 = s + s;
    cout << newS0 << endl;
    auto newS1 = s + "I am very OK";
    cout << newS1 << endl;
    auto newS2 = s + 'a';
    cout << newS2 << endl;
    auto otherS = String("Hello World");
    cout << format("compare result: {}\n", s == otherS);

    cout << format("s contains a: {}\n", s.Contains('a'));
    cout << format("s empty: {}\n", s.Empty());
    cout << format("s length: {}\n", s.Length());
    cout.put('\n');
    s = s.Substring(6, 5);
    cout << format("after change: {}\n", s);
    cout << format("s contains a: {}\n", s.Contains('a'));
    cout << format("s empty: {}\n", s.Empty());
    cout << format("s length: {}\n", s.Length());
    s = s.Substring(9);
    cout << format("after change: {}\n", s);
    cout << format("s contains a: {}\n", s.Contains('a'));
    cout << format("s empty: {}\n", s.Empty());
    cout << format("s length: {}\n", s.Length());

    string stdStr;
    {
        auto myStr = String{ "Hello std string" };
        stdStr = myStr;
    }
    cout << format("copy content from String to std::string: {}\n", stdStr);


    // test share work as expect
}