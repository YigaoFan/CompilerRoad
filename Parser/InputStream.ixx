export module InputStream;

import std;

using std::string_view;

template <typename T, typename Item>
concept Stream = requires (T t)
{
    { t.NextItem() } -> std::same_as<Item>;
    { t.Copy() } -> std::same_as<T>;
};

class StringViewStream
{
private:
    string_view str;
    unsigned currentPos;
public:
    StringViewStream(string_view str, unsigned startPos = 0) : str(str), currentPos(startPos)
    {
    }

    auto Next() -> char
    {
        //return { .Value = str[this->currentPos++], };
    }

    auto Copy() -> StringViewStream
    {
        //return { this->mStr, this->currentPos, };
    }
};
