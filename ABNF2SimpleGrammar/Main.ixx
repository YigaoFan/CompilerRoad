import std;
import Lexer;
import Parser;

using std::string;
using std::pair;
using std::vector;
using std::size_t;

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
        }
        return std::format_to(fc.out(), "{}", s);
    }
};

template <typename T>
struct VectorStream
{
    vector<T> Tokens;
    size_t Index = 0;

    auto NextItem() -> T
    {
        return Tokens[Index++];
    }

    auto Eof() const -> bool
    {
        return Index >= Tokens.size();
    }
};

int main()
{
    using std::ranges::views::filter;
    using std::ranges::to;

    std::array rules =
    {
        pair<string, TokType>{ "\\->", TokType::Arrow },
        pair<string, TokType>{ "<", TokType::LeftAngle },
        pair<string, TokType>{ ">", TokType::RightAngle },
        pair<string, TokType>{ "\\[", TokType::LeftSquare },
        pair<string, TokType>{ "\\]", TokType::RightSquare },
        pair<string, TokType>{ "{", TokType::LeftBracket },
        pair<string, TokType>{ "}", TokType::RightBracket },
        pair<string, TokType>{ "\\*", TokType::StarMark },
        pair<string, TokType>{ "\\|", TokType::PipeMark },
        pair<string, TokType>{ " ", TokType::Whitespace },
        pair<string, TokType>{ "[a-zA-Z][a-zA-Z0-9_\\-]*", TokType::Symbol },
        pair<string, TokType>{ "[1-9][0-9]*", TokType::Number },
        pair<string, TokType>{ ";[^\n]*\n", TokType::Comment },
        pair<string, TokType>{ "\\-", TokType::Hyphen },
        pair<string, TokType>{ "\\(", TokType::LeftParen },
        pair<string, TokType>{ "\\)", TokType::RightParen },
        pair<string, TokType>{ "\n", TokType::Newline },
        pair<string, TokType>{ "\"[^\"]*\"", TokType::Terminal },
        pair<string, TokType>{ "'[a-zA-Z0-9]'", TokType::QutotedDigitOrAlphabet },
    };
    auto l = Lexer<TokType>::New(rules);
    auto p = TableDrivenParser::ConstructFrom("grammars",
    {// how to represent empty in current grammar
        { "grammars", {
            { "grammar", "newlines", "grammars" },
            { "grammar" },
            { },
        }},
        { "grammar", {
            { "sym", "->", "productions", "optional-comment" },
        }},
        { "optional-comment", {
            { "comment" },
            { },
        }},
        { "newlines", {
            { "newline", "more-newlines" },
        }},
        { "more-newlines", {
            { "newline", "more-newlines" },
            { },
        }},
        { "productions", {
            { "production", "more-productions" },
            { },
        }},
        { "more-productions", {
            { "|", "productions" }, // ref productions to support "| [end]" to represent empty production
            { },
        }},
        { "production", { // once production occurs, it means not empty, so production doesn't have {} right side
            { "item", "more-items" },
        }},
        { "more-items", {
            { "item", "more-items" },
            { },
        }},
        { "item", {
            { "item_0" },
            { "*", "item_0" },
            { "num", "*", "num", "item_0" },
            { "num", "*", "item_0" },
        }},
        { "item_0", {
            { "terminal" },
            { "sym" },
            { "digitOrAlphabet", "-", "digitOrAlphabet" },
            { "[", "productions", "]" },
            { "(", "production", ")" },
        }},
    },
    {
        { "->", static_cast<int>(TokType::Arrow) },
        { "|", static_cast<int>(TokType::PipeMark) },
        { "<", static_cast<int>(TokType::LeftAngle) },
        { ">", static_cast<int>(TokType::RightAngle) },
        { "(", static_cast<int>(TokType::LeftParen) },
        { ")", static_cast<int>(TokType::RightParen) },
        { "[", static_cast<int>(TokType::LeftSquare) },
        { "]", static_cast<int>(TokType::RightSquare) },
        { "{", static_cast<int>(TokType::LeftBracket) },
        { "}", static_cast<int>(TokType::RightBracket) },
        { "*", static_cast<int>(TokType::StarMark) },
        { "|", static_cast<int>(TokType::PipeMark) },
        { "sym", static_cast<int>(TokType::Symbol) },
        { "num", static_cast<int>(TokType::Number) },
        { "-", static_cast<int>(TokType::Hyphen) },
        { "comment", static_cast<int>(TokType::Comment) },
        { eof, static_cast<int>(TokType::EOF) },
        { "newline" , static_cast<int>(TokType::Newline) },
        { "terminal" , static_cast<int>(TokType::Terminal) },
        { "digitOrAlphabet" , static_cast<int>(TokType::QutotedDigitOrAlphabet) },
    });

    auto filename = "vba.abnf";
    std::ifstream file(filename);
    string content;
    if (file.is_open())
    {
        content.reserve(std::filesystem::file_size(filename));
        std::string line;

        while (std::getline(file, line))
        {
            content.append(move(line));
            content.push_back('\n');
        }
        file.close();
        auto toks = l.Lex(content) | filter([](auto& x) -> bool { return x.Type != TokType::Whitespace; }) | to<vector<Token<TokType>>>();
        toks.push_back({ .Type = TokType::EOF, .Value = "" }); // add eof
        // need to use concept to limit tokType is formattable
        auto ast = p.Parse<Token<TokType>>(VectorStream{ .Tokens = move(toks) });
        if (ast.has_value())
        {
            std::println("ast: {}", ast.value().Result);
        }
    }
    else
    {
        std::println("open file({}) failed", filename);
    }
    return 0;
}
