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
    constexpr int a = not 0;
    using std::ranges::views::filter;
    using std::ranges::to;
    using std::move;

    //{
    //    std::set<Step, std::less<void>> ss;
    //    ss.insert({ .Signal = Step::Strategy::PassOne, .Data = 'a' });
    //    std::println("ss has a: {}", ss.contains('a'));
    //    std::println("ss has b: {}", ss.contains('b'));
    //}
    //{
    //    std::set<Step, std::less<void>> ss;
    //    ss.insert({ .Signal = Step::Strategy::BlockOne, .Data = 'a' });
    //    std::println("ss has a: {}", ss.contains('a'));
    //    std::println("ss has b: {}", ss.contains('b'));
    //}
    //{
    //    std::set<Step, std::less<void>> ss;
    //    ss.insert({ .Signal = Step::Strategy::BlockOne, .Data = 'b' });
    //    std::println("ss has a: {}", ss.contains('a'));
    //    std::println("ss has b: {}", ss.contains('b'));
    //}
    //return 0;
    //TestRollBack();
    std::array rules = 
    {
        pair<string, TokType>{ "if|for|func", TokType::Keyword },
        pair<string, TokType>{ "[a-zA-Z][a-zA-Z0-9_]*", TokType::Id },
        pair<string, TokType>{ "[A-Z][a-zA-Z0-9]*", TokType::ClassName },
        pair<string, TokType>{ "\\(", TokType::LeftParen },
        pair<string, TokType>{ "\\)", TokType::RightParen },
        pair<string, TokType>{ "{", TokType::LeftBracket },
        pair<string, TokType>{ "}", TokType::RightBracket },
        pair<string, TokType>{ ",", TokType::Comma },
        pair<string, TokType>{ " ", TokType::Space },
        pair<string, TokType>{ "\"[^\"]*\"", TokType::String }, // TODO support
        //pair<string, TokType>{ "\"[^a-z]*\"", TokType::String }, // TODO support 
        //pair<string, TokType>{ "\"[^0]*\"", TokType::String }, // OK
    };
    auto l = Lexer<TokType>::New(rules);
    //string code = "if ab for Hello func a";
    string code = "func a (b, c) {} \"01234a\""; // not work as expected for string regular exp: "\"[^a-z]*\"" TODO check 
    auto tokens = l.Lex(code) | filter([](auto& x) -> bool { return x.Type != TokType::Space; }) | to<vector<Token<TokType>>>();
    auto p = TableDrivenParser::ConstructFrom("program",
    {
        { "program", {
            { "function" }
        }},
        { "function", {
            { "func", "id", "(", "paras", ")", "{", "}" } // how to process ( in parser: define it in lexer or parse it directly
        }},
        { "paras", { // for paras, how to distinguish below two rule? TODO In LL(1), how to handle this? left factor
            //{ "id", ",", "paras" },
            //{ "id" },
            { "id", "more-paras" }, // TODO not at least one para
        }},
        { "more-paras", {
            { ",", "paras"},
            {}
        }},
        //{ "literal", {
        //    { "string" },
        //    { "boolean" },
        //    { "number" },
        //}},
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
    });
    tokens.push_back({ .Value = "" });
    auto ast = p.Parse<Token<TokType>>(VectorStream{ .Tokens = move(tokens) });
}