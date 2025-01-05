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
using std::size_t;

template <typename A, typename B>
concept ExplicitConvertibleTo = requires (A a) { static_cast<B>(a); };

template <size_t N>
struct ZeroOrMoreItems
{
    char p[N]{};

    constexpr ZeroOrMoreItems(char const(&pp)[N])
    {
        std::ranges::copy(pp, p);
    }
};

//template <ZeroOrMoreItems A>
//constexpr auto operator""s()
//{
//
//}

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

    template <IToken Token>
    struct std::formatter<AstNode<Token>, char>
    {
        constexpr auto parse(std::format_parse_context& ctx)
        {
            auto it = ctx.begin();
            if (it == ctx.end())
                return it;

            if (it != ctx.end() && *it != '}')
                throw std::format_error("Invalid format args for QuotableString.");

            return it;
        }

        template <class FormatContext>
        constexpr auto format(AstNode<Token> const& t, FormatContext& fc) const
        {
            using std::back_inserter;
            using std::format_to;
            using std::format;

            string s;
            format_to(back_inserter(s), "{{\n");
            format_to(back_inserter(s), "Name: {}\n", t.Name);

            struct
            {
                auto operator()(Token tok) -> string { return format("{}\n", tok); }
                auto operator()(AstNode<Token> const& ast) -> string { return format("{}", ast); }
            } fmt;

            if (t.Children.empty())
            {
                format_to(back_inserter(s), "Children: []\n");
            }
            else
            {
                format_to(back_inserter(s), "Children: [\n");
                for (auto& i : t.Children) // no specialization formatter<variant>, so we need to do it manually
                {
                    format_to(back_inserter(s), "{}", std::visit(fmt, i));
                }
                format_to(back_inserter(s), "]\n");
            }
            format_to(back_inserter(s), "}}\n");
            return std::format_to(fc.out(), "{}", s);
        }
    };

    template <size_t N1>
    auto operator| (string_view left, char const(&right)[N1]) -> vector<RightSide>
    {
        return { { String(left) }, { right } };
    }

    template <size_t N0>
    auto operator| (vector<RightSide> left, char const(&right)[N0]) -> vector<RightSide>
    {
        left.push_back({ right });
        return left;
    }
}