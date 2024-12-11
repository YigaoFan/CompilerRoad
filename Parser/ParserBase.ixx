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

template <typename A, typename B>
concept ExplicitConvertibleTo = requires (A a) { static_cast<B>(a); };

export
{
    constexpr string_view epsilon = "";
    /// \0 in string means eof, note only work in grammar representation
    constexpr auto eof = "\0";
    //using Input = string; // TODO change

    template <typename T>
    struct ParseSuccessResult
    {
        T Result;
        String Remain;
    };

    struct ParseFailResult
    {
        // maybe add failed position later
        string Message;
    };
    template <typename T>
    using ParserResult = expected<ParseSuccessResult<T>, ParseFailResult>;
    using LeftSide = String;
    using RightSide = vector<String>;
    using Grammar = pair<LeftSide, vector<RightSide>>;
    template <typename T>
    concept IToken = requires (T t)
    {
        { t.Type } -> ExplicitConvertibleTo<int>; // when lex comes with conflict, it will chose the one with lower int value
        { t.Value } -> std::convertible_to<string>;
        { t.IsEof() } -> std::same_as<bool>;
    };
    template <IToken Token>
    struct AstNode
    {
        String Name;
        vector<String> const ChildSymbols;
        vector<variant<Token, AstNode>> Children;
    };
}