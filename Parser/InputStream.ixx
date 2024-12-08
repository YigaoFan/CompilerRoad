export module Parser:InputStream;

import std;

using std::string_view;
using std::ifstream;
using std::string;
using std::move;

using Char = char;

class StringViewStream
{
private:
    string_view str;
    unsigned currentPos;
public:
    StringViewStream(string_view str, unsigned startPos = 0) : str(str), currentPos(startPos)
    {
    }

    auto NextItem() -> Char
    {
        return str[this->currentPos++];
    }

    auto Copy() const -> StringViewStream
    {
        return { this->str, this->currentPos, };
    }
};

class FileStream
{
private:
    ifstream instream;
public:
    static auto New(string_view filename) -> FileStream
    {
        ifstream istrm(filename.data(), std::ios::binary);
    }

    FileStream(ifstream instream) : instream(move(instream))
    { }

    auto NextItem() -> Char
    {
        return instream.get();
    }

    auto Copy() const -> FileStream
    {
        throw; // not implement
    }
};

export
{
    template <typename T, typename Item>
    concept Stream = requires (T t)
    {
        { t.NextItem() } -> std::same_as<Item>;
        //{ t.Copy() } -> std::same_as<T>; // TODO: used?
        //{ t.Eof() } -> std::same_as<bool>; // TODO: used?
    };
}
