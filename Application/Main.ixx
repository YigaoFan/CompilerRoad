import std;
import Lexer;
import Base;
import Parser;
import FiniteAutomata;

using std::pair;
using std::string;
using std::string_view;
using std::vector;

enum class TokType : int
{
    Keyword_If, // TODO temp
    Keyword,
    Id,
    Space,
    ClassName,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Comma,
    String,
    Number,
    Boolean,
    EqualSign,
    PrefixOperator,
    QuestionMark,
    Colon,
    LeftSquareBracket,
    RightSquareBracket,
    Dot,
    AddOperator,
    MinOperator,
    MulOperator,
    DivOperator,
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
        case TokType::Keyword:
            s = "Keyword";
            break;
        case TokType::Id:
            s = "Id";
            break;
        case TokType::Space:
            s = "Space";
            break;
        case TokType::ClassName:
            s = "ClassName";
            break;
        case TokType::Comma:
            s = "Comma";
            break;
        case TokType::LeftBracket:
            s = "LeftBracket";
            break;
        case TokType::RightBracket:
            s = "RightBracket";
            break;
        case TokType::LeftParen:
            s = "LeftParen";
            break;
        case TokType::RightParen:
            s = "RightParen";
            break;
        case TokType::String:
            s = "String";
            break;
        case TokType::Number:
            s = "Number";
            break;
        case TokType::Boolean:
            s = "Boolean";
            break;
        case TokType::EqualSign:
            s = "EqualSign";
            break;
        case TokType::PrefixOperator:
            s = "PrefixOperator";
            break;
        case TokType::QuestionMark:
            s = "QuestionMark";
            break;
        case TokType::Colon:
            s = "QuestionMark";
            break;
        case TokType::LeftSquareBracket:
            s = "LeftSquareBracket";
            break;
        case TokType::RightSquareBracket:
            s = "RightSquareBracket";
            break;
        case TokType::Dot:
            s = "Dot";
            break;
        case TokType::AddOperator:
            s = "AddOperator";
            break;
        case TokType::MinOperator:
            s = "MinOperator";
            break;
        case TokType::MulOperator:
            s = "MulOperator";
            break;
        case TokType::DivOperator:
            s = "DivOperator";
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

auto TestRollBack() -> void
{
    std::array rules =
    {
        pair<string, TokType>{ "ab|(ab)*c", TokType::Id },
    };
    string rollbackStr = "abababab";
    auto l = Lexer<TokType>::New(rules);
    auto toks = l.Lex(rollbackStr);
}

int main()
{
    using std::ranges::views::filter;
    using std::ranges::to;
    using std::ranges::views::iota;
    using std::move;
    using namespace std::string_view_literals;

    //TestRollBack();
    // use book sample to test
    auto starts = Starts("Goal", {
        { "Goal", {
            { "Expr" },
        }},
        { "Expr", {
            { "Term", "Expr'" },
        }},
        { "Expr'", {
            { "+", "Term", "Expr'" },
            { "-", "Term", "Expr'" },
            { },
        }},
        { "Term", {
            { "Factor", "Term'" },
        }},
        { "Term'", {
            { "*", "Factor", "Term'" },
            { "/", "Factor", "Term'" },
            { },
        }},
        { "Factor", {
            { "(", "Expr", ")" },
            { "num" },
            { "name" },
        }},
    });
    std::array rules = 
    {
        pair<string, TokType>{ "if|for|func|var", TokType::Keyword },
        pair<string, TokType>{ "if", TokType::Keyword_If },
        pair<string, TokType>{ "[a-zA-Z][a-zA-Z0-9_]*", TokType::Id },
        pair<string, TokType>{ "[A-Z][a-zA-Z0-9]*", TokType::ClassName },
        pair<string, TokType>{ "\\(", TokType::LeftParen },
        pair<string, TokType>{ "\\)", TokType::RightParen },
        pair<string, TokType>{ "{", TokType::LeftBracket },
        pair<string, TokType>{ "}", TokType::RightBracket },
        pair<string, TokType>{ ",", TokType::Comma },
        pair<string, TokType>{ " ", TokType::Space },
        pair<string, TokType>{ "\"[^\"]*\"", TokType::String },
        pair<string, TokType>{ "[1-9][0-9]*", TokType::Number },
        pair<string, TokType>{ "false|true", TokType::Boolean },
        pair<string, TokType>{ "=", TokType::EqualSign },
        pair<string, TokType>{ "[\\+\\-!]", TokType::PrefixOperator },
        pair<string, TokType>{ "?", TokType::QuestionMark },
        pair<string, TokType>{ "\\[", TokType::LeftSquareBracket },
        pair<string, TokType>{ "\\]", TokType::RightSquareBracket },
        pair<string, TokType>{ ":", TokType::Colon },
        pair<string, TokType>{ ".", TokType::Dot },
        pair<string, TokType>{ "\\+", TokType::AddOperator }, // TODO conflict with PrefixOperator
        pair<string, TokType>{ "\\-", TokType::MinOperator },
        pair<string, TokType>{ "\\*", TokType::MulOperator },
        pair<string, TokType>{ "/", TokType::DivOperator },
    };
    auto l = Lexer<TokType>::New(rules);
    //string code = "if ab for Hello func a";
    //string code = "func a (b, c) {} \"0abc12\"";
    //string code = "func a (b, c) { var a = 1 } ";
    string code = "func a (b, c) { if (false) { var d = 1 } }";
    auto tokens = l.Lex(code) | filter([](auto& x) -> bool { return x.Type != TokType::Space; }) | to<vector<Token<TokType>>>();
    auto p = LLParser::ConstructFrom("program", // support like "exp"s
    {
        { "program", {
            { "function" }
        }},
        { "function", {
            { "func", "id", "(", "paras", ")", "{", "statement"/*s*/, "}" } // how to process ( in parser: define it in lexer or parse it directly
        }},
        { "paras", { // for paras, how to distinguish below two rule? TODO In LL(1), how to handle this? left factor
            { "id", "more-paras" },
            {},
        }},
        { "more-paras", {
            { ",", "paras"},
            {},
        }},
        { "literal", "string"sv | "boolean" | "number" },
        { "suffix-ternary-operation", {
            { "?", "exp", ":", "exp" },
            { },
        }},
        { "suffix-invoke-operation", {
            { "(", "exp", ")" },
            { },
        }},
        { "exp", {
            { "exp_0", "suffix-ternary-operation"},
        }},
        { "exp_0", {
            { "exp_0", "+", "exp_1" },
            { "exp_0", "-", "exp_1" },
            { "exp_1" },
        }},
        { "exp_1", {
            { "exp_1", "*", "exp_2" },
            { "exp_1", "/", "exp_2" },
            { "exp_2" },
        }},
        { "exp_2", {
            { "prefix-operator", "exp_2" },
            { "exp_3", "suffix-invoke-operation" },
        }},
        { "exp_3", {
            { "(", "exp", ")" },
            { "id" },
            { "literal" },
            { "exp_3", ".", "id" },
            { "exp_3", "[", "exp", "]" },
        }},
        { "statement", {
            { "var-stmt" },
            { "if-stmt" },
        }},
        { "var-stmt", {
            { "var", "id", "=", "literal" },
        }},
        { "if-stmt", {
            { "if", "(", "exp", ")", "{", "statement", "}" },
        }},
    },
    {
        // TODO remove the cast
        { "func", static_cast<int>(TokType::Keyword) },
        { "id", static_cast<int>(TokType::Id) },
        { ",", static_cast<int>(TokType::Comma) },
        { "(", static_cast<int>(TokType::LeftParen) },
        { ")", static_cast<int>(TokType::RightParen) },
        { "{", static_cast<int>(TokType::LeftBracket) },
        { "}", static_cast<int>(TokType::RightBracket) },
        { "string", static_cast<int>(TokType::String) },
        { "number", static_cast<int>(TokType::Number) },
        { "boolean", static_cast<int>(TokType::Boolean) },
        { "=", static_cast<int>(TokType::EqualSign) },
        { "var", static_cast<int>(TokType::Keyword) },
        { "if", static_cast<int>(TokType::Keyword_If) }, // how to handle different keyword
        { "prefix-operator", static_cast<int>(TokType::PrefixOperator) },
        { "?", static_cast<int>(TokType::QuestionMark) },
        { ":", static_cast<int>(TokType::Colon) },
        { "[", static_cast<int>(TokType::LeftSquareBracket) },
        { "]", static_cast<int>(TokType::RightSquareBracket) },
        { ".", static_cast<int>(TokType::Dot) },
        { "+", static_cast<int>(TokType::AddOperator) },
        { "-", static_cast<int>(TokType::MinOperator) },
        { "*", static_cast<int>(TokType::MulOperator) },
        { "/", static_cast<int>(TokType::DivOperator) },
    });
    tokens.push_back({ .Value = "" }); // add eof
    auto ast = p.Parse<Token<TokType>>(VectorStream{ .Tokens = move(tokens) });
    if (ast.has_value())
    {
        std::println("ast: {}", ast.value().Result);
    }
}