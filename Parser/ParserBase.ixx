export module ParserBase;

import std;

using std::string;
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
    string Content;
};

template <typename T>
concept IToken = requires (T t)
{
    t.Type;
    { t.Content } -> std::convertible_to<string>;
};


export
{
    template <typename T>
    struct ParseSuccessResult;
    struct ParseFailResult;
    template <typename T>
    using ParserResult = expected<ParseSuccessResult<T>, ParseFailResult>;
    using LeftSide = string;
    using RightSide = vector<string>;
    using Grammar = pair<LeftSide, vector<RightSide>>;
    template <typename T>
    concept ITokenStream = requires (T t)
    {
        { t.NextToken() } -> IToken;
        { t.Copy() } -> std::convertible_to<T>;
    };
}