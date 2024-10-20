export module ParserBase;

import std;
import InputStream;

using std::string;
using std::string_view;
using std::expected;
using std::vector;
using std::pair;
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

template <typename TokenType>
struct Token
{
    TokenType Type;
    string Value;
};

//template <typename T>
//concept IToken = requires (T t)
//{
//    t.Type;
//    { t.Value } -> std::convertible_to<string>;
//};

struct AstNode
{
    Token<int> Token; // TODO temp
    vector<AstNode> Children;
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
    using LeftSide = string;
    using RightSide = vector<string>;
    using Grammar = pair<LeftSide, vector<RightSide>>;
    template <typename T>
    concept IToken = requires (T t)
    {
        t.Type;
        { t.Value } -> std::convertible_to<string>;
    };
    template <typename TokenType>
    struct Token;
    struct AstNode;
}