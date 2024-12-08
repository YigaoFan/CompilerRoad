import std;
import Lexer;
import Base;
import Parser;

using std::pair;
using std::string;
using std::string_view;
using std::vector;

enum class TokType : int // TODO recover to enum class
{
    Keyword,
    Id,
    Space,
    ClassName,
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
    constexpr bool b = std::is_convertible_v<TokType, int>;
    std::array rules = 
    {
        pair<string, TokType>{ "if|for|function", TokType::Keyword },
        pair<string, TokType>{ "[a-zA-Z][a-zA-Z0-9_]*", TokType::Id },
        pair<string, TokType>{ "[A-Z][a-zA-Z0-9]*", TokType::ClassName },
        pair<string, TokType>{ " ", TokType::Space },
    };
    auto l = Lexer<TokType>::New(rules);
    string code = "if ab0 for Hello";
    auto tokens = l.Lex(code);
    auto p = TableDrivenParser::ConstructFrom("program",
    {
        { "program", {
            { "function" }
        }},
        { "function", {
            { "func", "id", "(", "paras", ")", "{", "}"} // how to process ( in parser: define it in lexer or parse it directly
        }},
        { "paras", {
            { "id", ",", "paras" },
            { "id" },
        }},
    },
    {
        { "id", static_cast<int>(TokType::Id) },
    });
    p.Parse<Token<TokType>>(VectorStream{ .Tokens = move(tokens) });
}