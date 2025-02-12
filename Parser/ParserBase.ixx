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
using std::stack;
using std::move;

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

template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

export
{
    constexpr string_view epsilon = "";
    /// \0 in string means eof, note only work in grammar representation
    constexpr auto eof = "\0";
    //using Input = string; // TODO change

    struct ParseFailResult
    {
        // maybe add failed position later
        string Message;
    };
    template <typename T>
    using ParserResult = expected<T, ParseFailResult>;
    using LeftSide = String;
    using SimpleRightSide = vector<String>;
    using SimpleGrammar = pair<LeftSide, vector<SimpleRightSide>>;
    template <typename T>
    concept IToken = requires (T t)
    {
        { t.Type } -> ExplicitConvertibleTo<int>; // when lex comes with conflict, it will chose the one with lower int value
        { t.Value } -> std::convertible_to<string>;
        { t.IsEof() } -> std::same_as<bool>;
    };
    template <IToken Token, typename Result>
    struct SyntaxTreeNode
    {
        String Name;
        vector<String> ChildSymbols;
        vector<variant<Token, SyntaxTreeNode>> Children;// put all SyntaxTreeNode in a vector, here use raw pointer or deconstruct manually from bottom
        Result Result;

        SyntaxTreeNode(String name, vector<String> childSymbols, vector<variant<Token, SyntaxTreeNode>> children = {})
            : Name(move(name)), ChildSymbols(move(childSymbols)), Children(move(children)), Result() // typo here, Kern invoke me
        { }

        SyntaxTreeNode(SyntaxTreeNode&& that)
            : Name(move(that.Name)), ChildSymbols(move(that.ChildSymbols)), Children(move(that.Children)), Result(move(that.Result))
        { }

        ~SyntaxTreeNode()
        {
            using std::println;
            stack<SyntaxTreeNode> workingNodes;
            for (; not Children.empty(); Children.pop_back())
            {
                auto& back = Children.back();
                std::visit(overloads
                {
                    [&workingNodes](SyntaxTreeNode& n) -> void { workingNodes.push(move(n)); },
                    [](Token) -> void {},
                }, back);
            }

            for (; not workingNodes.empty();)
            {
                //println("working nodes size: {}", workingNodes.size());
                for (auto& working = workingNodes.top(); not working.Children.empty(); working.Children.pop_back())
                {
                    auto& back = working.Children.back();
                    std::visit(overloads
                    {
                        [&workingNodes](SyntaxTreeNode& n) -> void { workingNodes.push(move(n)); },
                        [](Token) -> void {},
                    }, back);
                }
                workingNodes.pop();
            }
        }
    };

    template <IToken Token, typename Result>
    struct std::formatter<SyntaxTreeNode<Token, Result>, char>
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

        template<class... Ts>
        struct overloads : Ts... { using Ts::operator()...; };

        template <class FormatContext>
        constexpr auto format(SyntaxTreeNode<Token, Result> const& t, FormatContext& fc) const
        {
            using std::back_inserter;
            using std::format_to;
            using std::format;
            using std::holds_alternative;
            using std::get;
            using std::ranges::views::reverse;

            stack<variant<Token, SyntaxTreeNode<Token, Result>> const*> workings;
            string s;

            format_to(back_inserter(s), "{{\n");
            format_to(back_inserter(s), "Name: {}\n", t.Name);
            if (t.Children.empty())
            {
                format_to(back_inserter(s), "Children: [");
            }
            else
            {
                format_to(back_inserter(s), "Children: [\n");
                workings.push(nullptr); // to notify encounter current children's end
                for (auto& i : reverse(t.Children))
                {
                    workings.push(&i);
                }
            }
            for (; not workings.empty();)
            {
                variant<Token, SyntaxTreeNode<Token, Result>> const* working = workings.top();
                workings.pop();
                if (working == nullptr)
                {
                    format_to(back_inserter(s), "]\n");
                    format_to(back_inserter(s), "}}\n");
                }
                else if (holds_alternative<Token>(*working))// no specialization formatter<variant>, so we need to do it manually
                {
                    format_to(back_inserter(s), "{}\n", get<0>(*working));
                }
                else if (holds_alternative<SyntaxTreeNode<Token, Result>>(*working))
                {
                    auto& node = get<1>(*working);
                    format_to(back_inserter(s), "{{\n");
                    format_to(back_inserter(s), "Name: {}\n", node.Name);
                    if (node.Children.empty())
                    {
                        format_to(back_inserter(s), "Children: []\n");
                        format_to(back_inserter(s), "}}\n");
                    }
                    else
                    {
                        format_to(back_inserter(s), "Children: [\n");
                        workings.push(nullptr);
                        for (auto& i : reverse(node.Children))
                        {
                            workings.push(&i);
                        }
                    }
                }
            }
            // enclosing for the entire object
            format_to(back_inserter(s), "]\n");
            format_to(back_inserter(s), "}}\n");

            return std::format_to(fc.out(), "{}", s);
        }
    };

    template <size_t N1>
    auto operator| (string_view left, char const(&right)[N1]) -> vector<SimpleRightSide>
    {
        return { { String(left) }, { right } };
    }

    template <size_t N0>
    auto operator| (vector<SimpleRightSide> left, char const(&right)[N0]) -> vector<SimpleRightSide>
    {
        left.push_back({ right });
        return left;
    }
}