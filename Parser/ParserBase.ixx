export module Parser:ParserBase;

import std;
import Base;
import :InputStream;

using std::string;
using std::string_view;
using std::expected;
using std::vector;
using std::pair;
using std::variant;
using Input = string; // TODO change

template <typename T>
struct ParseSuccessResult
{
    T Result;
    Input Remain;
};

struct ParseFailResult
{
    // maybe add failed position later
    string Message;
};

export
{
    constexpr string_view epsilon = "";
    /// \0 in string means eof, note only work in grammar representation
    constexpr string_view eof = "\0";
    template <typename T>
    struct ParseSuccessResult;
    struct ParseFailResult;
    template <typename T>
    using ParserResult = expected<ParseSuccessResult<T>, ParseFailResult>;
    using LeftSide = String;
    using RightSide = vector<String>;
    using Grammar = pair<LeftSide, vector<RightSide>>;
    template <typename T>
    concept IToken = requires (T t)
    {
        { t.Type } -> std::convertible_to<int>; // when lex comes with conflict, it will chose the one with lower int value
        { t.Value } -> std::same_as<string>;
        { t.IsEof() } -> std::same_as<bool>;
    };
    template <IToken Token>
    struct AstNode
    {
        vector<String> const ChildSymbols;
        vector<variant<Token, AstNode>> Children;
    };
}