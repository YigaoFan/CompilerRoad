export module Parser:InputStream;

import std;

using std::string_view;
using std::ifstream;
using std::string;
using std::vector;
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

//class FileStream
//{
//private:
//    ifstream instream;
//public:
//    static auto New(string_view filename) -> FileStream
//    {
//        ifstream istrm(filename.data(), std::ios::binary);
//    }
//
//    FileStream(ifstream instream) : instream(move(instream))
//    { }
//
//    auto NextItem() -> Char
//    {
//        return instream.get();
//    }
//
//    auto RollbackTo(size_t i) -> void
//    {
//        throw; // not implement
//    }
//
//    auto CurrentPosition() -> size_t
//    {
//        throw; // not implement
//    }
//};

template <typename T>
struct VectorStream
{
    vector<T> Tokens;
    size_t Index = 0;

    auto MoveNext() -> void
    {
        ++Index;
    }

    auto Current() -> T
    {
        return Tokens[Index];
    }

    auto Rollback() -> void
    {
        if (Index > 0)
        {
            --Index;
        }
    }

    auto RollbackTo(size_t i) -> void
    {
        Index = i;
    }

    auto CurrentPosition() -> size_t
    {
        return Index;
    }
};

export
{
    template <typename T, typename Item>
    concept Stream = requires (T t, size_t i)
    {
        { t.MoveNext() };
        { t.Current() } -> std::same_as<Item>;
        { t.Rollback() } -> std::same_as<void>;
        { t.RollbackTo(i) } -> std::same_as<void>;
        { t.CurrentPosition() } -> std::same_as<size_t>;
    };
    template <typename T>
    struct VectorStream;
}
