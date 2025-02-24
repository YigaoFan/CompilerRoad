export module LexerTest;

import std;
import Base;
import Lexer;

using std::pair;
using std::array;
using std::string;
using std::vector;

#define nameof(x) #x

// define tok in VBA and construct string to test it
enum class TokType : int
{
    Newline,
    Arrow,
    LeftAngle,
    RightAngle,
    LeftParen,
    RightParen,
    LeftSquare,
    RightSquare,
    LeftBracket,
    RightBracket,
    StarMark,
    PipeMark,
    Hyphen,
    Whitespace,
    RegularExpression,
    QutotedDigitOrAlphabet,
    Terminal,
    Symbol,
    Number,
    Comment,
    EOF,
};

template<>
struct std::formatter<TokType, char>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        auto it = ctx.begin();
        return it;
    }

    template<class FormatContext>
    constexpr auto format(TokType const& t, FormatContext& fc) const
    {
        string_view s;
        switch (t)
        {
        case TokType::Arrow:
            s = "Arrow";
            break;
        case TokType::LeftAngle:
            s = "LeftAngle";
            break;
        case TokType::RightAngle:
            s = "RightAngle";
            break;
        case TokType::LeftParen:
            s = "LeftParen";
            break;
        case TokType::RightParen:
            s = "RightParen";
            break;
        case TokType::LeftSquare:
            s = "LeftSquare";
            break;
        case TokType::RightSquare:
            s = "RightSquare";
            break;
        case TokType::LeftBracket:
            s = "LeftBracket";
            break;
        case TokType::RightBracket:
            s = "RightBracket";
            break;
        case TokType::StarMark:
            s = "StarMark";
            break;
        case TokType::PipeMark:
            s = "PipeMark";
            break;
        case TokType::Hyphen:
            s = "Hyphen";
            break;
        case TokType::Whitespace:
            s = "Whitespace";
            break;
        case TokType::Symbol:
            s = "Symbol";
            break;
        case TokType::Number:
            s = "Number";
            break;
        case TokType::Comment:
            s = "Comment";
            break;
        case TokType::Newline:
            s = "Newline";
            break;
        case TokType::EOF:
            s = "EOF";
            break;
        case TokType::Terminal:
            s = "Terminal";
            break;
        case TokType::QutotedDigitOrAlphabet:
            s = "QutotedDigitOrAlphabet";
            break;
        }
        return std::format_to(fc.out(), "{}", s);
    }
};

auto Test() -> void
{
    using std::ranges::views::filter;
    using std::ranges::to;

    array rules =
    {
        //pair<string, TokType>{ "\\->", TokType::Arrow },
        //pair<string, TokType>{ "<", TokType::LeftAngle },
        //pair<string, TokType>{ ">", TokType::RightAngle },
        //pair<string, TokType>{ "\\[", TokType::LeftSquare },
        //pair<string, TokType>{ "\\]", TokType::RightSquare },
        //pair<string, TokType>{ "{", TokType::LeftBracket },
        //pair<string, TokType>{ "}", TokType::RightBracket },
        //pair<string, TokType>{ "\\*", TokType::StarMark },
        //pair<string, TokType>{ "\\|", TokType::PipeMark },
        pair<string, TokType>{ " |\t", TokType::Whitespace },
        //pair<string, TokType>{ "[a-zA-Z][a-zA-Z0-9_\\-]*", TokType::Symbol },
        //pair<string, TokType>{ "[1-9][0-9]*", TokType::Number },
        //pair<string, TokType>{ ";[^\n]*", TokType::Comment },
        //pair<string, TokType>{ "\\-", TokType::Hyphen },
        //pair<string, TokType>{ "\\(", TokType::LeftParen },
        //pair<string, TokType>{ "\\)", TokType::RightParen },
        //pair<string, TokType>{ "\n", TokType::Newline },
        pair<string, TokType>{ "\"((\\\\[\"\\\\])|[^\"\n])*\"", TokType::Terminal }, // should support more escape char except newline in Terminal and RegularExpression
        //pair<string, TokType>{ "'[a-zA-Z0-9]'", TokType::QutotedDigitOrAlphabet },
        pair<string, TokType>{ "r\"((\\\\[\"\\\\])|[^\"\n])*\"", TokType::RegularExpression },
    };

    auto l = Lexer<TokType>::New(rules);
    string code = "r\"ab\" \"abc\"";
    auto toks = l.Lex(code) | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace and x.Type != TokType::Comment; }) | to<vector<Token<TokType>>>();
    Assert(toks.size() == 2, "tokens size not equal as expect");
    Assert(toks[0].Type == TokType::RegularExpression, "toks[0] type not " nameof(TokType::RegularExpression));
    Assert(toks[1].Type == TokType::Terminal, "toks[1] type not " nameof(TokType::Terminal));
}

export
{
    auto Test() -> void;
}