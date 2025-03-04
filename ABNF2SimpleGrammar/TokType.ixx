export module TokType;

import std;

using std::string_view;

enum class TokType : int
{
    Newline,
    LexRuleHeader,
    ParseRuleHeader,
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
        // not implement
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for QuotableString.");

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
        case TokType::LexRuleHeader:
            s = "LexRuleHeader";
            break;
        case TokType::ParseRuleHeader:
            s = "ParseRuleHeader";
            break;
        }
        return std::format_to(fc.out(), "{}", s);
    }
};

export
{
    enum class TokType;
    template<>
    struct std::formatter<TokType, char>;
}